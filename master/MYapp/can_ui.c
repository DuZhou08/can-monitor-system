#include "can_ui.h"
#include "lvgl.h"
#include "can_monitor.h"
#include "tft.h"
#include "touch.h"   /* 触摸调试: 红点跟随手指, 定位触摸链路问题 */
#include "W25Q64.h"  /* 参数持久化: 阈值存 SPI Flash */

#define TAB_H       24
#define TAB_CNT     4
#define CARD_GAP    6

/* 顶部页签按钮 */
static lv_obj_t *tab_btns[TAB_CNT];
/* 四个页面容器, 只有当前页可见 */
static lv_obj_t *pages[TAB_CNT];
/* 触摸调试: 红色圆点跟随手指位置, 定位触摸链路 */
static lv_obj_t *touch_dot;

/* 节点卡片: state=在线/离线, rows[最大4]=数据行(行数按节点而定) */
#define NODE_ROWS_MAX  4
typedef struct {
	lv_obj_t *state;
	lv_obj_t *rows[NODE_ROWS_MAX];
} card_t;
static card_t cards[CM_NODE_CNT];

/* 刷新用静态文本缓冲: lv_label_set_text_static 只存指针不复制.
 * 若用 lv_label_set_text 每帧 malloc/free, LV_MEM 会碎片化, 运行一段时间后分配失败卡死. */
static char state_buf[CM_NODE_CNT][9];
static char row_buf[CM_NODE_CNT][NODE_ROWS_MAX][20];

/* 上一次显示的值: 值没变就不更新标签 → 不触发重绘 */
static uint16_t last_v1[CM_NODE_CNT];
static uint16_t last_v2[CM_NODE_CNT];
static uint8_t  last_st[CM_NODE_CNT] = {0xFF, 0xFF};   /* 0xFF 哨兵: 首帧即显示 */
static uint32_t last_rx[CM_NODE_CNT];
static uint8_t  last_online[CM_NODE_CNT];
static uint8_t  last_alarm[CM_NODE_CNT];   /* 告警着色状态缓存 (文本刷新独立于颜色) */

static const char *row_names[CM_NODE_CNT][NODE_ROWS_MAX] = {
	{"LIGHT", "TEMP", "RX", NULL},   /* NodeA: 光照/温度(0.1°C)/收帧数 */
	{"AX",    "AY",   "RX", "ST"},   /* NodeB: 加速度X/Y/收帧数/健康标志 */
};
static const uint8_t row_cnt[CM_NODE_CNT] = {3, 4}; //节点1三行参数，节点2四行参数

static void tab_cb(lv_event_t *e);
static lv_obj_t *row_create(lv_obj_t *parent, int x, int y, const char *name);
static void page_nodes_create(lv_obj_t *pg);
static void page_nodes_refresh(void);
static void page_chart_create(lv_obj_t *pg);
static void page_chart_refresh(void);
static void page_diag_create(lv_obj_t *pg);
static void page_diag_refresh(void);
static void page_set_create(lv_obj_t *pg);
static void page_set_refresh(void);

/* 全对象禁滚: LVGL 对象默认带 SCROLLABLE, 拖动会出滚动条/滑动.
 * 固定页面 UI 不需要滚动, 每个创建的固定尺寸控件都过一遍 */
static void no_scroll(lv_obj_t *o)
{
	lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}

/* ================= 页签 ================= */

static void tabs_create(lv_obj_t *scr)
{
	static const char *names[TAB_CNT] = {"Node", "Chart", "Diag", "Set"};
	uint8_t i;
	lv_obj_t *btn, *lbl;
	uint16_t w = TFT_WIDTH / TAB_CNT;

	for (i = 0; i < TAB_CNT; i++)
	{
		btn = lv_button_create(scr);
		lv_obj_set_size(btn, w, TAB_H - 2);
		lv_obj_set_pos(btn, i * w, 1);
		lv_obj_add_event_cb(btn, tab_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
		no_scroll(btn);
		lbl = lv_label_create(btn);
		lv_label_set_text(lbl, names[i]);
		lv_obj_center(lbl);
		tab_btns[i] = btn;
	}
}

/* user_data = 目标页码: 显示它, 隐藏其它 */
static void tab_cb(lv_event_t *e)
{
	uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
	uint8_t i;

	for (i = 0; i < TAB_CNT; i++)
	{
		if (i == idx)
		{
			lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
		}
		else
		{
			lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
		}
	}
}

/* ================= 页面容器 ================= */

static void pages_create(lv_obj_t *scr)
{
	uint8_t i;

	for (i = 0; i < TAB_CNT; i++)
	{
		lv_obj_t *pg = lv_obj_create(scr);
		lv_obj_set_size(pg, TFT_WIDTH, TFT_HEIGHT - TAB_H);
		lv_obj_set_pos(pg, 0, TAB_H);
		lv_obj_set_style_border_width(pg, 0, 0);
		lv_obj_set_style_pad_all(pg, 0, 0);   /* 默认 padding 会把子对象坐标整体右移/下移导致超界 */
		/* 不透明黑底: 透明容器在 PARTIAL 模式需整页层缓冲(320x216x2=138KB),
		 * 超过 LV_MEM(40KB) 会分配失败导致页面黑屏. 实心背景直接画进屏幕, 无需层. */
		lv_obj_set_style_bg_color(pg, lv_color_hex(0x000000), 0);
		no_scroll(pg);
		pages[i] = pg;
		if (i > 0)
		{
			lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
		}
	}

	page_nodes_create(pages[0]);   /* [3] 节点页 */
	page_chart_create(pages[1]);   /* [4] 曲线页 */
	page_diag_create(pages[2]);    /* [6] 诊断页 */
	page_set_create(pages[3]);     /* [7] 设置页 */
}

/* ================= 节点页 ================= */

static void page_nodes_create(lv_obj_t *pg)
{
	static const char *titles[CM_NODE_CNT] = {"NodeA", "NodeB"};
	uint8_t i, r;
	lv_obj_t *box, *lbl;
	uint16_t w = (TFT_WIDTH - CARD_GAP * 3) / 2;

	for (i = 0; i < CM_NODE_CNT; i++)
	{
		box = lv_obj_create(pg);
		lv_obj_set_size(box, w, TFT_HEIGHT - TAB_H - CARD_GAP * 2);
		lv_obj_set_pos(box, CARD_GAP + i * (w + CARD_GAP), CARD_GAP);
		lv_obj_set_style_pad_all(box, 0, 0);   /* 同上: 卡片内标签坐标按绝对定位 */
		no_scroll(box);

		lbl = lv_label_create(box);
		lv_label_set_text(lbl, titles[i]);
		lv_obj_set_pos(lbl, 8, 6);

		lbl = lv_label_create(box);
		lv_label_set_text(lbl, "ONLINE");
		lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -8, 6);
		cards[i].state = lbl;

		for (r = 0; r < row_cnt[i]; r++)
		{
			cards[i].rows[r] = row_create(box, 8, 30 + r * 22, row_names[i][r]);
		}
	}
}

static lv_obj_t *row_create(lv_obj_t *parent, int x, int y, const char *name)
{
	lv_obj_t *lbl = lv_label_create(parent);
	char buf[16];

	lv_snprintf(buf, sizeof(buf), "%s: --", name);
	lv_label_set_text(lbl, buf);
	lv_obj_set_pos(lbl, x, y);
	return lbl;
}

static void page_nodes_refresh(void)
{
	cm_node_t n;
	uint8_t i, r;

	for (i = 0; i < CM_NODE_CNT; i++)
	{
		if (!CM_GetNode(i, &n))
		{
			continue;
		}
		if (n.online != last_online[i])
		{
			last_online[i] = n.online;
			lv_snprintf(state_buf[i], sizeof(state_buf[i]), "%s", n.online ? "ONLINE" : "OFFLINE");
			lv_label_set_text_static(cards[i].state, state_buf[i]);
			lv_obj_set_style_text_color(cards[i].state,
				n.online ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);
		}

		if (n.v1 != last_v1[i] || n.v2 != last_v2[i])
		{
			last_v1[i] = n.v1;
			last_v2[i] = n.v2;
			for (r = 0; r < 2; r++)
			{
				int16_t val = (int16_t)((r == 0) ? n.v1 : n.v2);
				if (i == CM_NODE_A && r == 1)
				{
					/* TEMP 以 0.1°C 存储 (260 = 26.0), 显示带一位小数 */
					lv_snprintf(row_buf[i][r], sizeof(row_buf[i][r]), "%s: %d.%d",
						row_names[i][r], val / 10, val % 10);
				}
				else
				{
					/* 其余值行按 int16: NodeA 正值与无符号同形, NodeB AX/AY 可负 */
					lv_snprintf(row_buf[i][r], sizeof(row_buf[i][r]), "%s: %d",
						row_names[i][r], val);
				}
				lv_label_set_text_static(cards[i].rows[r], row_buf[i][r]);
			}
		}
		if (n.rx_cnt != last_rx[i])
		{
			last_rx[i] = n.rx_cnt;
			lv_snprintf(row_buf[i][2], sizeof(row_buf[i][2]), "%s: %lu",
				row_names[i][2], (unsigned long)n.rx_cnt);
			lv_label_set_text_static(cards[i].rows[2], row_buf[i][2]);
		}
		if (row_cnt[i] > 3 && n.state != last_st[i])
		{
			last_st[i] = n.state;
			lv_snprintf(row_buf[i][3], sizeof(row_buf[i][3]), "%s: %u",
				row_names[i][3], n.state);
			lv_label_set_text_static(cards[i].rows[3], row_buf[i][3]);
		}

		/* 告警着色 (镜像 F1 control 判定, 在线才判):
		 *   NodeA TEMP 行: 温度(×10) > 阈值(°C×10) → 红
		 *   NodeB ST   行: MPU 健康标志 =1        → 红
		 * 正常时移除本地色样式, 恢复主题默认色 */
		if (n.online)
		{
			uint8_t alarm = 0;
			lv_obj_t *tgt = NULL;

			if (i == CM_NODE_A)
			{
				alarm = (n.v2 > (uint16_t)CM_GetThreshold() * 10u) ? 1 : 0;
				tgt = cards[i].rows[1];   /* TEMP 行 */
			}
			else
			{
				alarm = (n.state == 1) ? 1 : 0;
				tgt = cards[i].rows[3];   /* ST 行 */
			}
			if (alarm != last_alarm[i])
			{
				last_alarm[i] = alarm;
				if (alarm)
				{
					lv_obj_set_style_text_color(tgt, lv_palette_main(LV_PALETTE_RED), 0);
				}
				else
				{
					lv_obj_remove_local_style_prop(tgt, LV_STYLE_TEXT_COLOR, 0);
				}
			}
		}
	}
}

/* ================================================================
 *  [4] 曲线页 (Chart) —— NodeA 光照/温度滚动波形
 * ----------------------------------------------------------------
 *  数据源: CM_GetNode(CM_NODE_A) 的 v1(光照)/v2(温度)
 *  更新:   CAN_UI_Refresh 每 100ms 推入 chart 滚动窗口
 *  优化:   页面隐藏时不更新 (隐藏对象不渲染, 省 CPU)
 * ================================================================ */

static lv_obj_t *chart_light;
static lv_obj_t *chart_temp;
static lv_chart_series_t *ser_light;
static lv_chart_series_t *ser_temp;

static void page_chart_create(lv_obj_t *pg)
{
	lv_obj_t *lbl;

	lbl = lv_label_create(pg);
	lv_label_set_text(lbl, "LIGHT");
	lv_obj_set_pos(lbl, 6, 2);
	no_scroll(lbl);

	chart_light = lv_chart_create(pg);
	lv_obj_set_size(chart_light, TFT_WIDTH - 12, 94);
	lv_obj_set_pos(chart_light, 6, 18);
	lv_chart_set_type(chart_light, LV_CHART_TYPE_LINE);
	lv_chart_set_point_count(chart_light, CM_HIST_LEN);
	lv_chart_set_div_line_count(chart_light, 4, 8);
	lv_chart_set_axis_range(chart_light, LV_CHART_AXIS_PRIMARY_Y, 0, 4095);
	lv_obj_set_style_pad_all(chart_light, 4, 0);
	no_scroll(chart_light);
	ser_light = lv_chart_add_series(chart_light, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

	lbl = lv_label_create(pg);
	lv_label_set_text(lbl, "TEMP C");
	lv_obj_set_pos(lbl, 6, 116);
	no_scroll(lbl);

	chart_temp = lv_chart_create(pg);
	lv_obj_set_size(chart_temp, TFT_WIDTH - 12, 90);
	lv_obj_set_pos(chart_temp, 6, 132);
	lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);
	lv_chart_set_point_count(chart_temp, CM_HIST_LEN);
	lv_chart_set_div_line_count(chart_temp, 4, 8);
	lv_chart_set_axis_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y, 0, 500); /* 0~50.0°C (×10) */
	lv_obj_set_style_pad_all(chart_temp, 4, 0);
	no_scroll(chart_temp);
	ser_temp = lv_chart_add_series(chart_temp, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
}

static void page_chart_refresh(void)
{
	cm_node_t n;

	if (lv_obj_has_flag(pages[1], LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}
	if (!CM_GetNode(CM_NODE_A, &n))
	{
		return;
	}
	lv_chart_set_next_value(chart_light, ser_light, n.v1);
	lv_chart_set_next_value(chart_temp, ser_temp, n.v2);
}

/* ================================================================
 *  [6] 诊断页 (Diag) —— 总线统计 + 控制器错误 + 阈值 + LVGL 内存
 * ----------------------------------------------------------------
 *  数据源: CM_GetBus() (含 ESR 诊断) + CM_GetThreshold() + lv_mem_monitor()
 *  优化:   变化检测 (值不变不更新, 零重绘) + 隐藏页不更新
 * ================================================================ */

static lv_obj_t *diag_rows[7];
static char diag_buf[7][32];
static uint32_t last_rx_total;
static uint32_t last_rx_disp = 0xFFFFFFFF;   /* 哨兵: 首刷即显示丢帧行 */
static uint16_t last_rx_lost;
static uint16_t last_bus_state = 0xFF;   /* 0xFF 哨兵: 首帧即显示真实值 */
static uint16_t last_tec = 0xFF;
static uint16_t last_rec = 0xFF;
static uint16_t last_lec = 0xFF;
static uint16_t last_threshold;
static uint32_t last_mem_ms;       /* LVGL 内存 1s 刷新节流 */

static void page_diag_create(lv_obj_t *pg)
{
	static const char *names[7] = {"RX FRAMES", "RX LOST", "BUS STATE", "ERR T/R", "LEC", "THRESH(C)", "LVGL MEM"};
	uint8_t i;
	lv_obj_t *lbl;

	for (i = 0; i < 7; i++)
	{
		lbl = lv_label_create(pg);
		lv_label_set_text(lbl, names[i]);
		lv_obj_set_pos(lbl, 8, 10 + i * 24);
		no_scroll(lbl);
		diag_rows[i] = lbl;
	}
}

static void page_diag_refresh(void)
{
	static const char *const lec_names[8] = {"NONE", "STUFF", "FORM", "ACK", "RECESSIVE", "DOMINANT", "CRC", "SW"};
	cm_bus_t b;
	uint16_t thr = CM_GetThreshold();

	if (lv_obj_has_flag(pages[2], LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}
	CM_GetBus(&b);

	if (b.rx_total != last_rx_total)
	{
		last_rx_total = b.rx_total;
		lv_snprintf(diag_buf[0], sizeof(diag_buf[0]), "RX FRAMES: %lu", (unsigned long)b.rx_total);
		lv_label_set_text_static(diag_rows[0], diag_buf[0]);
	}
	/* 丢帧率实时现算: 只要收帧总数在动(分母变)就刷新, 丢帧率实时下降而非冻结 */
	if (b.rx_lost != last_rx_lost || b.rx_total != last_rx_disp)
	{
		uint32_t tot = b.rx_total + b.rx_lost;
		uint16_t pct = (tot) ? (uint16_t)((uint64_t)b.rx_lost * 1000u / tot) : 0;

		last_rx_lost = b.rx_lost;
		last_rx_disp = b.rx_total;
		if (tot)
		{
			lv_snprintf(diag_buf[1], sizeof(diag_buf[1]), "RX LOST: %lu (%u.%u%%)",
				(unsigned long)b.rx_lost, pct / 10u, pct % 10u);
		}
		else
		{
			lv_snprintf(diag_buf[1], sizeof(diag_buf[1]), "RX LOST: %lu", (unsigned long)b.rx_lost);
		}
		lv_label_set_text_static(diag_rows[1], diag_buf[1]);
	}
	if (b.bus_state != last_bus_state)
	{
		last_bus_state = b.bus_state;
		lv_snprintf(diag_buf[2], sizeof(diag_buf[2]), "BUS STATE: %s",
			b.bus_state == 0 ? "NORMAL" : b.bus_state == 1 ? "PASSIVE" : "BUS-OFF");
		lv_label_set_text_static(diag_rows[2], diag_buf[2]);
	}
	if (b.tec != last_tec || b.rec != last_rec)
	{
		last_tec = b.tec;
		last_rec = b.rec;
		lv_snprintf(diag_buf[3], sizeof(diag_buf[3]), "ERR T/R: %u/%u", b.tec, b.rec);
		lv_label_set_text_static(diag_rows[3], diag_buf[3]);
	}
	if (b.lec != last_lec)
	{
		last_lec = b.lec;
		lv_snprintf(diag_buf[4], sizeof(diag_buf[4]), "LEC: %u %s", b.lec, lec_names[b.lec & 7]);
		lv_label_set_text_static(diag_rows[4], diag_buf[4]);
	}
	if (thr != last_threshold)
	{
		last_threshold = thr;
		lv_snprintf(diag_buf[5], sizeof(diag_buf[5]), "THRESHOLD: %u", thr);
		lv_label_set_text_static(diag_rows[5], diag_buf[5]);
	}

	/* LVGL 内存池用量 + 峰值, 1s 刷新一次 (峰值 max_used 比稳态更能反映余量) */
	{
		uint32_t now_ms = lv_tick_get();
		if (now_ms - last_mem_ms > 1000)
		{
			lv_mem_monitor_t mon;
			last_mem_ms = now_ms;
			lv_mem_monitor(&mon);
			lv_snprintf(diag_buf[6], sizeof(diag_buf[6]), "LVGL MEM: %u%% PK%uKB",
				mon.used_pct, (uint32_t)(mon.max_used / 1024U));
			lv_label_set_text_static(diag_rows[6], diag_buf[6]);
		}
	}
}

/* ================================================================
 *  [7] 设置页 (Set) —— 温度阈值调节
 * ----------------------------------------------------------------
 *  交互: [-] [+] 按钮步进调节, 触摸点击 (user_data 传步进量)
 *  数据: CM_GetThreshold() 读 / CM_SetThreshold() 写
 *  优化: 变化检测 + 隐藏页不更新
 * ================================================================ */

/* 温度告警阈值: °C 整数语义 (镜像 F1 control 的阈值设定, 单位与温度一致) */
#define SET_THR_MIN    0
#define SET_THR_MAX    80
#define W25Q64_PARAM_ADDR  0x00000000u   /* 阈值参数存放扇区起始地址 */

static lv_obj_t *set_value;
static lv_obj_t *set_feedback;
static char set_buf[16];
static char cmd_buf[64];
static uint16_t last_set_threshold;
static uint32_t last_ack_tick[CM_NODE_CNT];   /* 每从机 ACK 变化检测 */

/* 主机命令按钮规格 (广播到全部从机) */
typedef struct {
	uint8_t  opcode;
	uint16_t param;
	const char *txt;
} cmd_spec_t;
static const cmd_spec_t cmd_specs[] = {
	{CM_CMD_SET_PERIOD, 500, "500ms"},
	{CM_CMD_SET_PERIOD,  70, "70ms"},
	{CM_CMD_SET_PERIOD,  20, "20ms"},
	{CM_CMD_PING,         0, "PING"},
};

static void set_btn_cb(lv_event_t *e)
{
	int16_t delta = (int16_t)(intptr_t)lv_event_get_user_data(e);
	int32_t v = (int32_t)CM_GetThreshold() + delta;

	if (v < SET_THR_MIN) v = SET_THR_MIN;
	if (v > SET_THR_MAX) v = SET_THR_MAX;
	CM_SetThreshold((uint16_t)v);
}

/* SAVE: 阈值写 W25Q64 并回读校验, 反馈 成功/失败 */
static void set_save_cb(lv_event_t *e)
{
	int16_t v = (int16_t)CM_GetThreshold();
	int16_t chk = 0;

	lv_label_set_text_static(set_feedback, "SAVING..");
	lv_obj_set_style_text_color(set_feedback, lv_palette_main(LV_PALETTE_GREY), 0);

	if (W25Q64_SaveValue(W25Q64_PARAM_ADDR, v) &&
	    W25Q64_LoadValue(W25Q64_PARAM_ADDR, &chk) && chk == v)
	{
		lv_label_set_text_static(set_feedback, "SAVE OK");
		lv_obj_set_style_text_color(set_feedback, lv_palette_main(LV_PALETTE_GREEN), 0);
	}
	else
	{
		lv_label_set_text_static(set_feedback, "SAVE FAIL");
		lv_obj_set_style_text_color(set_feedback, lv_palette_main(LV_PALETTE_RED), 0);
	}
}

/* 命令按钮: 广播 SET_PERIOD / PING, 结果由 ACK 帧在刷新中回显 */
static void set_cmd_cb(lv_event_t *e)
{
	const cmd_spec_t *sp = (const cmd_spec_t *)lv_event_get_user_data(e);

	lv_label_set_text_static(set_feedback, "CMD SENT..");
	lv_obj_set_style_text_color(set_feedback, lv_palette_main(LV_PALETTE_GREY), 0);
	CM_SendCommand(CM_TARGET_ALL, sp->opcode, sp->param);
}

static void set_step_btn(lv_obj_t *pg, int x, int y, const char *txt, int16_t delta)
{
	lv_obj_t *btn = lv_button_create(pg);
	lv_obj_t *lbl;

	lv_obj_set_size(btn, 68, 36);
	lv_obj_set_pos(btn, x, y);
	lv_obj_add_event_cb(btn, set_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
	no_scroll(btn);

	lbl = lv_label_create(btn);
	lv_label_set_text(lbl, txt);
	lv_obj_center(lbl);
}

static void page_set_create(lv_obj_t *pg)
{
	lv_obj_t *lbl;
	uint8_t i;

	lbl = lv_label_create(pg);
	lv_label_set_text(lbl, "THRESHOLD(C)");
	lv_obj_set_pos(lbl, 8, 6);
	no_scroll(lbl);

	set_value = lv_label_create(pg);
	lv_snprintf(set_buf, sizeof(set_buf), "%u", CM_GetThreshold());
	lv_label_set_text_static(set_value, set_buf);
	lv_obj_set_pos(set_value, 8, 28);
	lv_obj_set_style_text_font(set_value, &lv_font_montserrat_14, 0);
	no_scroll(set_value);

	/* 状态行: SAVE 结果 / 命令 ACK 回显共用 */
	set_feedback = lv_label_create(pg);
	lv_label_set_text_static(set_feedback, "");
	lv_obj_set_pos(set_feedback, 8, 52);
	no_scroll(set_feedback);

	/* 行1: 阈值步进调节 (°C, ±1 细调 / ±5 粗调) */
	set_step_btn(pg, 8,  94, "-5", -5);
	set_step_btn(pg, 88, 94, "-1",  -1);
	set_step_btn(pg, 168, 94, "+1",  +1);
	set_step_btn(pg, 248, 94, "+5",  +5);

	/* 行2: 主机命令 (广播) */
	for (i = 0; i < (sizeof(cmd_specs) / sizeof(cmd_specs[0])); i++)
	{
		lv_obj_t *btn = lv_button_create(pg);
		lv_obj_t *b_lbl;
		lv_obj_set_size(btn, 68, 34);
		lv_obj_set_pos(btn, 8 + i * 80, 138);
		lv_obj_add_event_cb(btn, set_cmd_cb, LV_EVENT_CLICKED, (void *)&cmd_specs[i]);
		no_scroll(btn);
		b_lbl = lv_label_create(btn);
		lv_label_set_text(b_lbl, cmd_specs[i].txt);
		lv_obj_center(b_lbl);
	}

	/* SAVE 到 W25Q64 */
	{
		lv_obj_t *btn = lv_button_create(pg);
		lv_obj_t *lbl;
		lv_obj_set_size(btn, TFT_WIDTH - 16, 30);
		lv_obj_set_pos(btn, 8, 182);
		lv_obj_add_event_cb(btn, set_save_cb, LV_EVENT_CLICKED, NULL);
		no_scroll(btn);
		lbl = lv_label_create(btn);
		lv_label_set_text(lbl, "SAVE TO FLASH");
		lv_obj_center(lbl);
	}
}

static void page_set_refresh(void)
{
	uint16_t thr = CM_GetThreshold();

	if (lv_obj_has_flag(pages[3], LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}
	if (thr != last_set_threshold)
	{
		last_set_threshold = thr;
		lv_snprintf(set_buf, sizeof(set_buf), "%u", thr);
		lv_label_set_text_static(set_value, set_buf);
	}

	/* ACK 回显: 双从机各占一行, 有任一节点的新 ACK 即整行刷新 (广播不再互相覆盖) */
	{
		cm_ack_t a;
		uint8_t i, changed = 0, all_ok = 1, any_valid = 0;

		for (i = 0; i < CM_NODE_CNT; i++)
		{
			if (CM_GetAck(i, &a) && a.tick != last_ack_tick[i])
			{
				last_ack_tick[i] = a.tick;
				changed = 1;
			}
		}
		if (changed)
		{
			char *p = cmd_buf;
			uint16_t room = sizeof(cmd_buf);
			for (i = 0; i < CM_NODE_CNT; i++)
			{
				int n;
				if (CM_GetAck(i, &a))
				{
					any_valid = 1;
					if (a.result != 0) all_ok = 0;
					n = lv_snprintf(p, room, "%s %s %s T%u/R%u\n",
						i == CM_NODE_A ? "S0" : "S1",
						a.opcode == CM_CMD_SET_PERIOD ? "PER" : "PING",
						a.result == 0 ? "OK" : "ERR", a.tec, a.rec);
				}
				else
				{
					all_ok = 0;
					n = lv_snprintf(p, room, "%s --\n", i == CM_NODE_A ? "S0" : "S1");
				}
				if (n > 0 && (uint16_t)n < room)
				{
					p += n;
					room -= (uint16_t)n;
				}
			}
			if (p > cmd_buf) p[-1] = '\0';          /* 去掉末尾换行 */
			lv_label_set_text_static(set_feedback, cmd_buf);
			lv_obj_set_style_text_color(set_feedback,
				all_ok ? lv_palette_main(LV_PALETTE_GREEN) :
				(any_valid ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREY)), 0);
		}
	}
}

/* ================================================================
 *  [5] 对外接口
 * ================================================================ */

void CAN_UI_Init(void)
{
	lv_obj_t *scr = lv_screen_active();
	int16_t saved;

	/* 参数持久化: 上电从 W25Q64 恢复上次保存的阈值 (无有效数据则保持默认) */
	W25Q64_Init();
	if (W25Q64_LoadValue(W25Q64_PARAM_ADDR, &saved) && saved >= SET_THR_MIN && saved <= SET_THR_MAX)
	{
		CM_SetThreshold((uint16_t)saved);
	}

	tabs_create(scr);
	pages_create(scr);

	/* 触摸调试红点 */
	touch_dot = lv_obj_create(scr);
	lv_obj_set_size(touch_dot, 8, 8);
	lv_obj_set_style_bg_color(touch_dot, lv_palette_main(LV_PALETTE_RED), 0);
	lv_obj_set_style_bg_opa(touch_dot, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(touch_dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
	lv_obj_remove_flag(touch_dot, LV_OBJ_FLAG_CLICKABLE);   /* 关键: 不抢点击事件 (命中检测跳过它) */
	no_scroll(touch_dot);
}

void CAN_UI_Refresh(void)
{
	static uint32_t last_ms;
	uint32_t now = lv_tick_get();

	/* 节流 100ms: 屏幕刷新周期 33ms, 监控界面 10Hz 更新足够.
	 * 配合"值变化才更新", 数据稳定时零重绘, 不给渲染让路 */
	if (now - last_ms < 100)
	{
		return;
	}
	last_ms = now;
	page_nodes_refresh();   /* [3] 节点页: 变化检测, 值不变零重绘 */
	page_chart_refresh();   /* [4] 曲线页: 滚动波形 */
	page_diag_refresh();    /* [6] 诊断页: 总线统计 */
	page_set_refresh();     /* [7] 设置页: 阈值显示 */

	/* 触摸调试: 红点跟随手指, 验证触摸链路 */
	{
		uint16_t tx, ty;
		if (TOUCH_GetXY(&tx, &ty))
		{
			lv_obj_remove_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
			lv_obj_set_pos(touch_dot, tx - 4, ty - 4);
		}
		else
		{
			lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
		}
	}
}
