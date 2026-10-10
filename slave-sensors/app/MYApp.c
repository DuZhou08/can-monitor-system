#include "MYApp.h"

void vTaskLED(void *pvParameters);
void vTaskOLED(void *pvParameters);
void vTaskCAN_TX(void *pvParameters);
void vTaskCAN_RX(void *pvParameters);

/* 双向协议 (详见 PROTOCOL.md):
 *  上报帧:   StdId=0x300, DLC=5: [光照2B][温度2B][帧序号1B]
 *            帧序号 0~255 循环, 主机按差值统计丢帧
 *  主机命令帧: StdId=0x500, Data[0]=目标 0x00=本机/0xFF=广播, Data[1]=指令码
 *    0x01 SET_PERIOD: Data[2..3]=周期ms(大端, 20~1000)  0x02 PING
 *  本机应答帧: StdId=0x310, Data[0]=回显指令码 Data[1]=结果 Data[2]=TEC Data[3]=REC */
#define CMD_ID          0x500u
#define CMD_TARGET      0x00u        /* 本机地址 (0x300 & 0xFF) */
#define ACK_ID          0x310u
#define CMD_SET_PERIOD  0x01u
#define CMD_PING        0x02u

volatile uint16_t g_report_ms = 70;  /* 上报周期, CAN_RX 任务可改 */
uint8_t g_tx_seq = 0;                /* 上报帧序号 (丢帧统计依据) */

CanTxMsg TxMsg_TEMP = {
    .StdId = 0x300,
    .ExtId = 0x00000000,
    .IDE = CAN_Id_Standard,
    .RTR = CAN_RTR_Data,
    .DLC = 5,
};

#define TASK_LED_STACK_SIZE 128
#define TASK_LED_PRIORITY 1
TaskHandle_t xLEDTaskHandle;
#define TASK_OLED_STACK_SIZE 128
#define TASK_OLED_PRIORITY 1
TaskHandle_t xOLEDTaskHandle;
#define TASK_MYCAN_TX_STACK_SIZE 256
#define TASK_MYCAN_TX_PRIORITY 1
TaskHandle_t xMYCAN_TxTaskHandle;
#define TASK_MYCAN_RX_STACK_SIZE 256
#define TASK_MYCAN_RX_PRIORITY 2
TaskHandle_t xMYCAN_RxTaskHandle;


void FreeRTOS_Start(void)
{
    xTaskCreate(vTaskLED, "Task LED", TASK_LED_STACK_SIZE, NULL, TASK_LED_PRIORITY, &xLEDTaskHandle);
    xTaskCreate(vTaskOLED, "Task OLED", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_PRIORITY, &xOLEDTaskHandle);
    xTaskCreate(vTaskCAN_TX, "Task CAN TX", TASK_MYCAN_TX_STACK_SIZE, NULL, TASK_MYCAN_TX_PRIORITY, &xMYCAN_TxTaskHandle);
    xTaskCreate(vTaskCAN_RX, "Task CAN RX", TASK_MYCAN_RX_STACK_SIZE, NULL, TASK_MYCAN_RX_PRIORITY, &xMYCAN_RxTaskHandle);
    vTaskStartScheduler();
}

void vTaskLED(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1)
    {
        Work_LED_Turn();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void vTaskOLED(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint16_t Lum = 0, Temp = 0;
    AdcSample_t sample;
    while(1)
    {
        /* Peek 读取最新采样快照（不消费，OLED/CAN 两任务可同时读） */
        if (xQueuePeek(xAdcQueue, &sample, 0) == pdPASS)
        {
            Lum  = sample.Lum;
            Temp = sample.Temp;
        }

        OLED_ShowString(1,1,"LUMIN:");
        OLED_ShowString(2,1,"TEMP:");
        OLED_ShowNum(1,8, Lum, 4);
        OLED_ShowNum(2,7, Temp, 4);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(70));
    }
}

void vTaskCAN_TX(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint16_t Lum = 0, Temp = 0;
    AdcSample_t sample;
    while(1)
    {
        /* CAN bus-off 自恢复：发送错误累计导致离线时，重新初始化 CAN */
        if (CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF))
        {
            CAN_DeInit(CAN1);
            MYCAN_Init();
            xLastWakeTime = xTaskGetTickCount();    /* 重新对齐调度周期 */
        }

        /* Peek 读取最新采样快照（不消费，两任务读到的都是同一份最新值） */
        if (xQueuePeek(xAdcQueue, &sample, 0) == pdPASS)
        {
            Lum  = sample.Lum;
            Temp = sample.Temp;
        }

        TxMsg_TEMP.Data[0] = (uint8_t)(Lum >> 8);
        TxMsg_TEMP.Data[1] = (uint8_t)(Lum & 0xFF);
        TxMsg_TEMP.Data[2] = (uint8_t)(Temp >> 8);
        TxMsg_TEMP.Data[3] = (uint8_t)(Temp & 0xFF);
        TxMsg_TEMP.Data[4] = g_tx_seq++;
        MYCAN_Transmit(&TxMsg_TEMP);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(g_report_ms));  /* 周期可由主机指令改 */
    }
}

/* CAN 接收任务: 认领主机命令 (0x500), 执行并回 ACK */
void vTaskCAN_RX(void *pvParameters)
{
    CanRxMsg rx;
    CanTxMsg ack = {0};
    uint16_t p;

    ack.StdId = ACK_ID;
    ack.IDE = CAN_Id_Standard;
    ack.RTR = CAN_RTR_Data;
    ack.DLC = 4;

    while(1)
    {
        if (MYCAN_ReceiveFlag())
        {
            MYCAN_Receive(&rx);
            if (rx.DLC >= 2 && rx.StdId == CMD_ID &&
                (rx.Data[0] == CMD_TARGET || rx.Data[0] == 0xFF))
            {
                ack.Data[0] = rx.Data[1];              /* 回显指令码 */
                ack.Data[1] = 0;                       /* 结果: 0=OK */
                if (rx.Data[1] == CMD_SET_PERIOD && rx.DLC >= 4)
                {
                    p = (uint16_t)((rx.Data[2] << 8) | rx.Data[3]);
                    if (p < 20)  p = 20;
                    if (p > 1000) p = 1000;
                    g_report_ms = p;
                }
                /* 附带本机 CAN 错误计数 (TEC/REC), 供主机诊断 */
                ack.Data[2] = (uint8_t)((CAN1->ESR >> 16) & 0xFF);  /* TEC */
                ack.Data[3] = (uint8_t)(CAN1->ESR >> 24);           /* REC */
                MYCAN_Transmit(&ack);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
