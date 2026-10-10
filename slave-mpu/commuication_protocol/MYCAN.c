#include "stm32f10x.h"                  // Device header
#include "MYCAN.h"

void MYCAN_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; //上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    CAN_InitTypeDef CAN_InitStructure;
    CAN_InitStructure.CAN_Prescaler = 48;//分频系数
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;//工作模式
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

    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask; 
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    /* 精确匹配主机命令帧 0x500 (标准ID位于32位过滤器高11位, 故 id<<5):
     * 本机周期性上报帧不会回环进自身 RX, 只需认领命令. */
    CAN_FilterInitStructure.CAN_FilterIdHigh = (0x500u << 5);      /* = 0xA000 */
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (0x7FFu << 5);  /* = 0xFFE0, 11位全比 */
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000; 
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0; 
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE; 
    CAN_FilterInit(&CAN_FilterInitStructure);
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
