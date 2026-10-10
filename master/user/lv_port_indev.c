#include "lvgl.h"
#include "touch.h"
#include "lv_port_indev.h"

static void lv_port_touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
	uint16_t x = 0, y = 0;

	if (TOUCH_GetXY(&x, &y))
	{
		data->point.x = x;
		data->point.y = y;
		data->state = LV_INDEV_STATE_PRESSED;
	}
	else
	{
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

void lv_port_indev_init(void)
{
	lv_indev_t *indev;

	TOUCH_Init();

	indev = lv_indev_create();
	lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(indev, lv_port_touchpad_read_cb);
	lv_indev_set_display(indev, lv_display_get_default());
}
