/* USART1 —— 预留调试端口
 * 引脚: PA9=TX / PA10=RX, 波特率 9600, 8N1
 * 现状: 尚未调用 MYUART_init(); 预留用于调试输出
 * 注意: 不在本文件设置 NVIC 优先级分组, 由 main.c 统一设为 Group_4 */
#include "MYUART.h"

void MYUART_init()
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //硬件复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 9600; //波特率
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流控制
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; //发送和接收
    USART_InitStructure.USART_Parity = USART_Parity_No; //奇偶校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1; //停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; //数据位
    USART_Init(USART1, &USART_InitStructure); //初始化

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); //使能接收中断

    /* 注意: 不在此处设置 NVIC 优先级分组。分组须保持"全抢占(Group_4)",
     * 已由 main.c 统一设置; 若在此改为 Group_2 会破坏 FreeRTOS 中断管控 */

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE); //使能UART1
}

void MYUART_Send_Byte(uint8_t data)
{
    USART_SendData(USART1, data);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void MYUART_Send_Array(uint8_t *Array, uint8_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        MYUART_Send_Byte(Array[i]);
    }
}

void MYUART_Send_String(char *String)
{
    uint16_t i;
    for (i = 0; String[i] != 0; i++)
    {
        MYUART_Send_Byte(String[i]);
    }
}

uint32_t MYUART_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;
    while (y--)
    {
        result *= x;
    }
    return result;
}

void MYUART_SendNum(uint32_t num, uint8_t len)
{
    uint8_t i;
    for (i = len; i > 0; i--)
    {
        MYUART_Send_Byte(num / MYUART_Pow(10, i - 1) % 10 + '0');
    }
}

void UART_Printf(char *format, ...)
{
    char string[100];
    va_list args;
    va_start(args, format);
    vsprintf(string, format, args);
    va_end(args);
    MYUART_Send_String(string);
}

void UART_SendPacket(void)
{
    MYUART_Send_Byte(0xFF);
    MYUART_Send_Array(UART_TxPacket, 8);
    MYUART_Send_Byte(0xFE);
}

uint8_t UART_GetRxFlag(void)
{
    if (UART_RxFlag == 1)
    {
        UART_RxFlag = 0;
        return 1;
    }
    return 0;
}

void USART1_IRQHandler(void)
{
    static uint8_t RxState = 0;
    static uint8_t pRxPacket = 0;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = USART_ReceiveData(USART1);
        if (RxState == 0)
        {
            if (data == 0xFF)
            {
                RxState = 1;
                pRxPacket = 0;
            }
        }
        else if (RxState == 1)
        {
            UART_RxPacket[pRxPacket++] = data;
            pRxPacket ++;
            if (pRxPacket >= 4)
            {
                RxState = 2;
            }
        }
        else if (RxState == 2)
        {
            if (data == 0xFE)
            {
                UART_RxFlag = 1;
                RxState = 0;
            }
        }
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}


