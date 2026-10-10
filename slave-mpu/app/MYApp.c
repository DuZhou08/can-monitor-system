#include "MYApp.h"

void vTaskLED(void *pvParameters);
void vTaskOLED(void *pvParameters);
void vTaskMPU(void *pvParameters);
void vTaskCAN_TX(void *pvParameters);
void vTaskCAN_RX(void *pvParameters);

/* 双向协议 (详见 PROTOCOL.md):
 *  上报帧:   StdId=0x301, DLC=6: [AX 2B][AY 2B][健康标志1B][帧序号1B]
 *            帧序号 0~255 循环, 主机按差值统计丢帧
 *  主机命令帧: StdId=0x500, Data[0]=目标 0x01=本机/0xFF=广播, Data[1]=指令码
 *    0x01 SET_PERIOD: Data[2..3]=周期ms(大端, 20~1000)  0x02 PING
 *  本机应答帧: StdId=0x311, Data[0]=回显指令码 Data[1]=结果 Data[2]=TEC Data[3]=REC */
#define CMD_ID          0x500u
#define CMD_TARGET      0x01u        /* 本机地址 (0x301 & 0xFF) */
#define ACK_ID          0x311u
#define CMD_SET_PERIOD  0x01u
#define CMD_PING        0x02u

volatile uint16_t g_report_ms = 70;  /* 上报周期, CAN_RX 任务可改 */
uint8_t g_tx_seq = 0;                /* 上报帧序号 (丢帧统计依据) */

CanTxMsg TxMsg_TEMP = {
    .StdId = 0x301,
    .ExtId = 0x00000000,
    .IDE = CAN_Id_Standard,
    .RTR = CAN_RTR_Data,
    .DLC = 6,
};

uint8_t ID = 0;
int16_t AX, AY, AZ, GX, GY, GZ;
uint8_t MCU_State = 0;

#define TASK_LED_STACK_SIZE 128
#define TASK_LED_PRIORITY 1
TaskHandle_t xLEDTaskHandle;
#define TASK_OLED_STACK_SIZE 128
#define TASK_OLED_PRIORITY 1
TaskHandle_t xOLEDTaskHandle;
#define TASK_MPU_STACK_SIZE 256
#define TASK_MPU_PRIORITY 2
TaskHandle_t xMPUTaskHandle;
#define TASK_CAN_TX_STACK_SIZE 128
#define TASK_CAN_TX_PRIORITY 1
TaskHandle_t xCAN_TXTaskHandle;
#define TASK_CAN_RX_STACK_SIZE 256
#define TASK_CAN_RX_PRIORITY 3
TaskHandle_t xCAN_RXTaskHandle;



void FreeRTOS_Start(void)
{
    xTaskCreate(vTaskLED, "Task LED", TASK_LED_STACK_SIZE, NULL, TASK_LED_PRIORITY, &xLEDTaskHandle);
    xTaskCreate(vTaskOLED, "Task OLED", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_PRIORITY, &xOLEDTaskHandle);
    xTaskCreate(vTaskMPU, "Task MPU", TASK_MPU_STACK_SIZE, NULL, TASK_MPU_PRIORITY, &xMPUTaskHandle);
    xTaskCreate(vTaskCAN_TX, "Task CAN_TX", TASK_CAN_TX_STACK_SIZE, NULL, TASK_CAN_TX_PRIORITY, &xCAN_TXTaskHandle);
    xTaskCreate(vTaskCAN_RX, "Task CAN_RX", TASK_CAN_RX_STACK_SIZE, NULL, TASK_CAN_RX_PRIORITY, &xCAN_RXTaskHandle);
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
    OLED_ShowString(1, 1, "State");
    while(1)
    {
		OLED_ShowSignedNum(2, 1, AX, 5);					//OLED显示数据
		OLED_ShowSignedNum(3, 1, AY, 5);
		OLED_ShowSignedNum(4, 1, AZ, 5);
		OLED_ShowSignedNum(2, 8, GX, 5);
		OLED_ShowSignedNum(3, 8, GY, 5);
		OLED_ShowSignedNum(4, 8, GZ, 5);
        OLED_ShowNum(1, 7, MCU_State, 2);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(70));
    }
}

void vTaskMPU(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t ZeroCount = 0;
    uint8_t MCU_Count = 0;
    while(1)
    {
        ID = MPU6050_GetID();						//自检：读取WHO_AM_I，检测I2C总线连通性
        if (ID != 0x68)								//失联（读回0x00/0xFF）
        {
            MPU6050_Init();							//重新初始化I2C与MPU6050
            ZeroCount = 0;
        }
        else
        {
            MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);	//获取MPU6050的数据
            if (AX >= 1000 || AX <= -1000 || AY >= 1000 || AY <= -1000 || AZ <= 0)
            {
                if (++MCU_Count >= 3)				//连续3次(约300ms)异常
                {
                    MCU_State = 1;							//异常状态
                    MPU6050_Init();						//重新初始化I2C与MPU6050
                }
            }
            else
            {
                MCU_Count = 0;
                MCU_State = 0;							//正常状态
            }

            /* 自检：芯片掉电复位后进入SLEEP模式，六轴输出全0。
               静止时Z轴应有重力分量(约±16384)，六轴持续全0判定为异常，重新唤醒 */
            if (AX==0 && AY==0 && AZ==0 && GX==0 && GY==0 && GZ==0)
            {
                if (++ZeroCount >= 3)				//连续3次(约300ms)全0
                {
                    MPU6050_Init();					//重新唤醒并配置MPU6050
                    ZeroCount = 0;
                }
            }
            else
            {
                ZeroCount = 0;
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(70));
    }
}

void vTaskCAN_TX(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int16_t ax = 0, ay = 0;
    uint8_t st = 0;
    while(1)
    {
        /* CAN bus-off 自恢复：发送错误累计导致离线时，重新初始化 CAN */
        if (CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF))
        {
            CAN_DeInit(CAN1);
            MYCAN_Init();
            xLastWakeTime = xTaskGetTickCount();    /* 重新对齐调度周期 */
        }

        /* 临界区快照: vTaskMPU 每 70ms 更新 AX/AY/MCU_State, 防止跨任务读到撕裂值 */
        taskENTER_CRITICAL();
        ax = AX;
        ay = AY;
        st = MCU_State;
        taskEXIT_CRITICAL();

        TxMsg_TEMP.Data[0] = (uint8_t)(ax >> 8);    /* AX 大端 int16 */
        TxMsg_TEMP.Data[1] = (uint8_t)(ax & 0xFF);
        TxMsg_TEMP.Data[2] = (uint8_t)(ay >> 8);    /* AY 大端 int16 */
        TxMsg_TEMP.Data[3] = (uint8_t)(ay & 0xFF);
        TxMsg_TEMP.Data[4] = st;                    /* MCU_State 健康标志 */
        TxMsg_TEMP.Data[5] = g_tx_seq++;
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
