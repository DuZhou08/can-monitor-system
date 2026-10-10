#include "lvgl.h"
#include "tft.h"
#include "lv_port_disp.h"

#define LV_PORT_BUF_LINES 32   /* 1/10 屏高度, LVGL 建议局部渲染缓冲区最小值 */

static uint8_t buf1[TFT_WIDTH * LV_PORT_BUF_LINES * 2];
static uint8_t buf2[TFT_WIDTH * LV_PORT_BUF_LINES * 2];

static void lv_port_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
	TFT_WritePixels(area->x1, area->y1, area->x2, area->y2, px_map);
	lv_display_flush_ready(disp);
}

void lv_port_disp_init(void)
{
	lv_display_t *disp;

	TFT_Init();

	disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT); // 告诉 LVGL 屏幕多大
	lv_display_set_flush_cb(disp, lv_port_flush_cb); // 告诉 LVGL 怎么把像素画上去
	lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}
