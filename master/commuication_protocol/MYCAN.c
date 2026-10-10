#include "stm32f4xx.h"                  // Device header
#include "MYCAN.h"

void MYCAN_Init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = CAN_RX_PIN | CAN_TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; 
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(CAN_GPIO, &GPIO_InitStructure);

    /* F4 必须显式配置引脚复用功能 (AF9 = CAN1), 否则外设不生效 */
    GPIO_PinAFConfig(CAN_GPIO, GPIO_PinSource11, GPIO_AF_CAN1);
    GPIO_PinAFConfig(CAN_GPIO, GPIO_PinSource12, GPIO_AF_CAN1);

    CAN_InitTypeDef CAN_InitStructure;
    CAN_InitStructure.CAN_Prescaler = 56;//分频系数 42M/56/6tq = 125kbps
    CAN_InitStructure.CAN_Mode = MYCAN_TEST_LOOPBACK ? CAN_Mode_LoopBack : CAN_Mode_Normal;//工作模式
    CAN_InitStructure.CAN_ABOM = ENABLE;//自动离线管理（bus-off 自动恢复）
    CAN_InitStructure.CAN_AWUM = DISABLE;//自动唤醒
    CAN_InitStructure.CAN_BS1 = CAN_BS1_2tq;//时间段1
    CAN_InitStructure.CAN_BS2 = CAN_BS2_3tq;//时间段2
    CAN_InitStructure.CAN_NART = DISABLE;//报文自动重传
    CAN_InitStructure.CAN_RFLM = DISABLE;//接收FIFO锁定
    CAN_InitStructure.CAN_SJW = CAN_SJW_2tq;//同步跳转宽度
    CAN_InitStructure.CAN_TTCM = DISABLE;//时间触发通信模式
    CAN_InitStructure.CAN_TXFP = DISABLE;//发送FIFO优先级
    CAN_Init(CAN1, &CAN_InitStructure);

    /* 按"通信矩阵"精确过滤: 只收本机关心的 4 个 ID.
     * 0x300/0x301 = 从机周期数据, 0x310/0x311 = 从机 ACK 应答. */
    {
        static const uint16_t rx_ids[4] = {0x300u, 0x301u, 0x310u, 0x311u};
        CAN_FilterInitTypeDef CAN_FilterInitStructure;
        uint8_t i;

        for (i = 0; i < 4; i++)
        {
            CAN_FilterInitStructure.CAN_FilterNumber = i;
            CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
            CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
            /* 标准ID位于过滤器高11位(位28:18), 故左移5进入IdHigh */
            CAN_FilterInitStructure.CAN_FilterIdHigh = (uint16_t)(rx_ids[i] << 5);
            CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
            CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (uint16_t)(0x7FFu << 5); /* 11位全比=精确 */
            CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
            CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
            CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
            CAN_FilterInit(&CAN_FilterInitStructure);
        }
    }
}

void MYCAN_Transmit(CanTxMsg *TxMsg)
{
    uint8_t TxMsgMailBox = CAN_Transmit(CAN1, TxMsg);

    uint32_t TimeOut = 0;
    while (CAN_TransmitStatus(CAN1, TxMsgMailBox) != CAN_TxStatus_Ok)
    {
        TimeOut++;
        if (TimeOut > 10000)
        {
            break;
        }
    }
}

uint8_t MYCAN_ReceiveFlag(void)
{
    if(CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
    {
        return 1;
    }
    return 0;
}

void MYCAN_Receive(CanRxMsg *RxMsg)
{
    CAN_Receive(CAN1, CAN_FIFO0, RxMsg);
}

/* 直接读 CAN1->ESR: REC[31:24] TEC[23:16] LEC[6:4] BOFF[2] EPVF[1] */
void MYCAN_GetDiag(mycan_diag_t *d)
{
    uint32_t esr = CAN1->ESR;

    d->tec = (uint8_t)((esr >> 16) & 0xFFu);
    d->rec = (uint8_t)(esr >> 24);
    d->lec = (uint8_t)((esr >> 4) & 0x07u);
    if (esr & 0x04u)
        d->state = MYCAN_ST_BUSOFF;
    else if (esr & 0x02u)
        d->state = MYCAN_ST_PASSIVE;
    else
        d->state = MYCAN_ST_NORMAL;
}
