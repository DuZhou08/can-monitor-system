#include "can_monitor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "MYCAN.h"   /* 主机命令帧经 MYCAN_Transmit 下发 */

/* ================================================================
 *  数据层 —— 真实 CAN 数据
 * ----------------------------------------------------------------
 *  数据入口: CM_OnFrame()  —— CAN_Task 每收一帧调用, 按从机地址解析
 *  离线检测: CM_Refresh()  —— 超过 CM_OFFLINE_MS 无帧 → offline
 *  UI 读取:  CM_Get* 接口  —— 接口不变, UI 零改动
 * ----------------------------------------------------------------
 *  帧格式 (F1 协议 docs/PROTOCOL.md):
 *    0x300 (NodeA): [光照2B][温度2B][帧序号1B]          DLC=5
 *    0x301 (NodeB): [AX 2B][AY 2B][健康标志1B][帧序号1B] DLC=6
 *    0x310/0x311:  从机 ACK (不参与丢帧统计)
 *  丢帧统计: 按帧序号差值 (接收序号 - 上次序号 - 1), 从机离线超时后重新同步
 * ================================================================ */

/* ================================================================
 *  温度标定 —— 过零点线性 (单点实测 + 假设过原点)
 *  --------------------------------------------------------------
 *  常温 26°C ↔ ADC 原始值 1750
 *    T(0.1°C) = raw × TEMP_DEG1 / TEMP_RAW1   (1750 → 260 → 26.0°C)
 *  升级两点式: 标定 TEMP_RAW0/TEMP_DEG0 第二实测点, 并换用线性插值
 * ================================================================ */
#define TEMP_RAW1   1750u
#define TEMP_DEG1   260u        /* 26.0°C × 10 */

static cm_node_t nodes[CM_NODE_CNT];
static cm_bus_t  bus;
static cm_ack_t  ack_info[CM_NODE_CNT];        /* 每从机独立 ACK 槽 (S0/S1 互不覆盖) */
static uint16_t  threshold = 40;          /* 温度告警阈值, °C 整数 (与 0.1°C 温度比较时 ×10) */
static uint16_t  hist[CM_NODE_CNT][CM_HIST_LEN];
static uint8_t   hist_idx;
static uint32_t  last_rx_ms[CM_NODE_CNT];   /* 每节点最后收帧时刻, 离线检测依据 */
static uint8_t   last_seq[CM_NODE_CNT];     /* 每节点上次帧序号 */
static uint8_t   seq_armed[CM_NODE_CNT];    /* 序号已同步 (离线超时后清0重新同步) */

void CM_Init(void)
{
	uint8_t n, i;

	for (n = 0; n < CM_NODE_CNT; n++)
	{
		for (i = 0; i < CM_HIST_LEN; i++)
		{
			hist[n][i] = 0;
		}
		last_rx_ms[n] = 0;
	}
}

/* CAN 收帧入口: 按从机地址解析写入数据层 (由 CAN_Task 每帧调用) */
void CM_OnFrame(uint32_t std_id, const uint8_t *data, uint8_t dlc)
{
	uint8_t addr = (uint8_t)(std_id & 0xFF);   /* 从机地址: 0x300→0x00, 0x301→0x01 */
	uint32_t now = (uint32_t)xTaskGetTickCount();
	uint8_t node;
	uint16_t nv1 = 0, nv2 = 0;
	uint8_t st = 0;
	uint8_t seq = 0;
	uint32_t lost = 0;

	/* 从机 ACK 应答帧 (0x310 sensors / 0x311 mpu): 完整 ID 匹配, 单独处理 */
	if (std_id == 0x310u || std_id == 0x311u)
	{
		if (dlc >= 4)
		{
			uint8_t ack_node = (std_id == 0x311u) ? CM_NODE_B : CM_NODE_A;
			cm_ack_t a;
			a.node   = ack_node;
			a.opcode = data[0];
			a.result = data[1];
			a.tec    = data[2];
			a.rec    = data[3];
			a.tick   = now;
			a.valid  = 1;
			taskENTER_CRITICAL();
			ack_info[ack_node] = a;
			taskEXIT_CRITICAL();
		}
		return;
	}

	switch (addr)
	{
	case 0x00:   /* NodeA (F1 sensors): 光照 + 温度, 大端 */
		if (dlc < 5)
		{
			return;
		}
		node = CM_NODE_A;
		nv1 = (uint16_t)((data[0] << 8) | data[1]);
		/* 温度原始值 → 0.1°C (过零点线性, 见标定宏) */
		nv2 = (uint16_t)((uint32_t)((data[2] << 8) | data[3]) * TEMP_DEG1 / TEMP_RAW1);
		seq = data[4];
		break;
	case 0x01:   /* NodeB (F1 MPU): AX + AY + 健康标志 */
		if (dlc < 6)
		{
			return;
		}
		node = CM_NODE_B;
		nv1 = (uint16_t)((data[0] << 8) | data[1]);
		nv2 = (uint16_t)((data[2] << 8) | data[3]);
		st = data[4];
		seq = data[5];
		break;
	default:
		return;   /* 非从机地址, 忽略 */
	}

	/* 丢帧检测: 序号已同步时按差值计数, 未同步(重启/刚上线)只对齐不计数 */
	if (seq_armed[node])
	{
		if (seq != last_seq[node])
		{
			lost = (uint32_t)(uint8_t)(seq - last_seq[node] - 1u);
		}
	}
	else
	{
		seq_armed[node] = 1;
	}
	last_seq[node] = seq;

	/* 临界区: CAN_Task(优先级2) 写 vs LVGL任务(优先级1) 读 nodes[], 防半更新 */
	taskENTER_CRITICAL();
	nodes[node].v1 = nv1;
	nodes[node].v2 = nv2;
	nodes[node].state = st;
	nodes[node].online = 1;
	nodes[node].rx_cnt++;
	last_rx_ms[node] = now;
	bus.rx_total++;
	if (lost)
	{
		bus.rx_lost += lost;
		bus.loss_pct = (uint16_t)((uint32_t)bus.rx_lost * 1000u / (bus.rx_total + bus.rx_lost));
	}
	taskEXIT_CRITICAL();

	/* 历史环: 每收一帧记一次 v1 */
	hist_idx = (uint8_t)((hist_idx + 1) % CM_HIST_LEN);
	hist[node][hist_idx] = nodes[node].v1;
}

/* 周期调用 (LVGL 任务每帧): 超时离线检测 */
void CM_Refresh(void)
{
	uint32_t now = (uint32_t)xTaskGetTickCount();
	uint8_t i;

	for (i = 0; i < CM_NODE_CNT; i++)
	{
		if (last_rx_ms[i] != 0 && (now - last_rx_ms[i]) > CM_OFFLINE_MS)
		{
			nodes[i].online = 0;
			seq_armed[i] = 0;   /* 掉线超时: 清序号同步, 重连后重新对齐避免误计 */
		}
	}
}

uint8_t CM_GetNode(uint8_t node, cm_node_t *out)
{
	if (node >= CM_NODE_CNT)
	{
		return 0;
	}
	taskENTER_CRITICAL();
	*out = nodes[node];
	taskEXIT_CRITICAL();
	return 1;
}

void CM_GetBus(cm_bus_t *out)
{
	*out = bus;
}

/* CAN 控制器诊断喂入 (CAN_Task 每轮拉一次硬件 ESR 后调用) */
void CM_SetBusDiag(uint8_t state, uint8_t tec, uint8_t rec, uint8_t lec)
{
	bus.bus_state = state;
	bus.tec = tec;
	bus.rec = rec;
	bus.lec = lec;
}

/* 主机命令下发 (UI 任务调用): 组 0x500 帧, SET_PERIOD 带周期参数 */
void CM_SendCommand(uint8_t target, uint8_t opcode, uint16_t param)
{
	CanTxMsg tx;

	tx.StdId = CM_CMD_ID;
	tx.ExtId = 0;
	tx.IDE = CAN_Id_Standard;
	tx.RTR = CAN_RTR_Data;
	tx.DLC = (opcode == CM_CMD_SET_PERIOD) ? 4u : 2u;
	tx.Data[0] = target;
	tx.Data[1] = opcode;
	tx.Data[2] = (uint8_t)(param >> 8);
	tx.Data[3] = (uint8_t)(param & 0xFF);
	MYCAN_Transmit(&tx);
}

/* 读某从机最近一次 ACK */
uint8_t CM_GetAck(uint8_t node, cm_ack_t *out)
{
	if (node >= CM_NODE_CNT || !ack_info[node].valid)
	{
		return 0;
	}
	taskENTER_CRITICAL();
	*out = ack_info[node];
	taskEXIT_CRITICAL();
	return 1;
}

uint16_t CM_GetThreshold(void)
{
	return threshold;
}

void CM_SetThreshold(uint16_t v)
{
	threshold = v;
}

uint16_t CM_GetHistory(uint8_t node, uint8_t idx)
{
	return hist[node][idx];
}
