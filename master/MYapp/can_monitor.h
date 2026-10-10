#ifndef __CAN_MONITOR_H
#define __CAN_MONITOR_H

#include <stdint.h>

/* 监控台 UI 与数据层的接口契约.
 * 数据入口: CM_OnFrame() —— CAN_Task 每收到一帧调用, 解析写入数据层.
 * UI 读取:  CM_Get* 接口 (UI 只读, 不关心数据从哪来). */

#define CM_NODE_A       0
#define CM_NODE_B       1
#define CM_NODE_CNT     2
#define CM_HIST_LEN     100
#define CM_OFFLINE_MS   2000

/* 主机→从机命令协议 (双向化, 详见 F1 docs/PROTOCOL.md) */
#define CM_CMD_ID           0x500u
#define CM_TARGET_SENSORS   0x00u
#define CM_TARGET_MPU       0x01u
#define CM_TARGET_ALL       0xFFu
#define CM_CMD_SET_PERIOD   0x01u   /* 参数=周期ms(大端) */
#define CM_CMD_PING         0x02u

typedef struct {
	uint8_t  valid;      /* 1=已收到过从机 ACK */
	uint8_t  node;       /* 0=sensors 1=mpu */
	uint8_t  opcode;     /* 回显指令码 */
	uint8_t  result;     /* 0=OK */
	uint8_t  tec;        /* 从机侧发送错误计数 */
	uint8_t  rec;        /* 从机侧接收错误计数 */
	uint32_t tick;       /* ACK 到达时刻 */
} cm_ack_t;

typedef struct {
	uint8_t  online;      /* 0=离线 1=在线 */
	uint16_t v1;          /* NodeA: 光照 / NodeB: AX(int16 位模式) */
	uint16_t v2;          /* NodeA: 温度(0.1°C) / NodeB: AY(int16 位模式) */
	uint8_t  state;       /* NodeB: MPU 健康标志 0=正常 1=异常 (预留后续逻辑使用) */
	uint32_t rx_cnt;      /* 该节点累计收帧数 */
} cm_node_t;

typedef struct {
	uint16_t loss_pct;    /* 丢帧率 千分比 0~1000 */
	uint16_t bus_state;   /* 0=normal 1=error-passive 2=bus-off */
	uint32_t rx_total;    /* 累计收帧 */
	uint32_t rx_lost;     /* 累计丢帧 */
	uint8_t  tec;         /* 发送错误计数 (CAN_ESR) */
	uint8_t  rec;         /* 接收错误计数 (CAN_ESR) */
	uint8_t  lec;         /* 最后错误码 0无 1填充 2格式 3ACK 4隐性 5显性 6CRC */
} cm_bus_t;

void     CM_Init(void);                        /* 数据层初始化, LVGL 任务内调用一次 */
void     CM_Refresh(void);                     /* 周期调用: 超时离线检测 */
void     CM_OnFrame(uint32_t std_id, const uint8_t *data, uint8_t dlc);  /* CAN 收帧入口 */
void     CM_SetBusDiag(uint8_t state, uint8_t tec, uint8_t rec, uint8_t lec);  /* CAN_Task 周期喂控制器诊断 */
void     CM_SendCommand(uint8_t target, uint8_t opcode, uint16_t param);       /* 发主机命令帧(UI触发) */
uint8_t  CM_GetAck(uint8_t node, cm_ack_t *out);         /* 读某从机最近一次 ACK (无有效则0), 各从机独立 */
uint8_t  CM_GetNode(uint8_t node, cm_node_t *out);
void     CM_GetBus(cm_bus_t *out);
uint16_t CM_GetThreshold(void);
void     CM_SetThreshold(uint16_t v);          /* 触摸设定阈值, 阶段二转发为 CAN 指令 */
uint16_t CM_GetHistory(uint8_t node, uint8_t idx);   /* 历史曲线, 0~CM_HIST_LEN-1 */

#endif
