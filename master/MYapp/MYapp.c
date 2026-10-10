#include "MYapp.h"
#include "stm32f4xx.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "can_monitor.h"
#include "can_ui.h"
#include "LED.h"
#include "commuication_protocol/MYCAN.h"

void vTaskLVGL(void *pvParameters);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
static void CAN_Task(void *pvParameters);

#define TASK_LVGL_STACK_SIZE 2048
#define TASK_LVGL_PRIORITY 1
TaskHandle_t xLVGLTaskHandle;
#define TASK_CAN_STACK_SIZE 256
#define TASK_CAN_PRIORITY 2
TaskHandle_t xCANTaskHandle;



void FreeRTOS_Start(void)
{
    LED_Init();
    xTaskCreate(vTaskLVGL, "LVGL Task", TASK_LVGL_STACK_SIZE, NULL, TASK_LVGL_PRIORITY, &xLVGLTaskHandle);
    xTaskCreate(CAN_Task, "CAN Task", TASK_CAN_STACK_SIZE, NULL, TASK_CAN_PRIORITY, &xCANTaskHandle);
    vTaskStartScheduler();
}

/* ================================================================
 *  CAN 接收任务: 轮询 FIFO 排空, 每帧喂给数据层 CM_OnFrame
 * ================================================================ */
static void CAN_Task(void *pvParameters)
{
    CanRxMsg rx;
    mycan_diag_t diag;

    MYCAN_Init();

#if MYCAN_TEST_LOOPBACK
    /* 回环自测: 每 500ms 自发一帧 0x300 (光照300/温度32), 验证收发链路 */
    static CanTxMsg tx_selftest = {0};
    uint32_t tx_last_ms = 0;
    tx_selftest.StdId = 0x300;
    tx_selftest.IDE = CAN_Id_Standard;
    tx_selftest.RTR = CAN_RTR_Data;
    tx_selftest.DLC = 4;
    tx_selftest.Data[0] = 0x01; tx_selftest.Data[1] = 0x2C;   /* 光照 300 */
    tx_selftest.Data[2] = 0x00; tx_selftest.Data[3] = 0x20;   /* 温度 32  */
#endif

    while (1)
    {
        while (MYCAN_ReceiveFlag())
        {
            MYCAN_Receive(&rx);
            CM_OnFrame(rx.StdId, rx.Data, rx.DLC);
        }
#if MYCAN_TEST_LOOPBACK
        if (xTaskGetTickCount() - tx_last_ms >= pdMS_TO_TICKS(500))
        {
            tx_last_ms = xTaskGetTickCount();
            MYCAN_Transmit(&tx_selftest);
        }
#endif
        MYCAN_GetDiag(&diag);
        CM_SetBusDiag(diag.state, diag.tec, diag.rec, diag.lec);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}



void vTaskLVGL(void *pvParameters)
{
	uint32_t led_last_ms = 0;

	lv_init();
	lv_tick_set_cb((lv_tick_get_cb_t)xTaskGetTickCount);   /* FreeRTOS tick 当 LVGL 时钟 (1ms) */
	lv_port_disp_init();                                   /* 显示器 + TFT_Init */
	lv_port_indev_init();                                  /* 触摸输入 */
	CM_Init();                                             /* 数据层初始化 */
	CAN_UI_Init();                                         /* 监控台界面 */

	while(1)
	{
		uint32_t now = (uint32_t)xTaskGetTickCount();
		if (now - led_last_ms >= 500)      /* 时间基准心跳: 不受渲染耗时影响, LED 恒定 2Hz */
		{
			led_last_ms = now;
			LED1_Turn();
		}
		CM_Refresh();          /* 数据层刷新(每帧) */
		CAN_UI_Refresh();      /* 界面刷新(节流+变化检测) */
		lv_timer_handler();    /* LVGL 渲染 */
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

/* 栈溢出回调 (configCHECK_FOR_STACK_OVERFLOW=2), 正常永不执行.
 * 触发时停在此处: 任务心跳 LED1 停止闪烁即为死信号 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	for (;;)
	{
	}
}
