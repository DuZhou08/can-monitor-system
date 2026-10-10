#include "stm32f4xx.h"
#include "MYSPI.h"

void MYSPI_Start(void)
{
    GPIO_ResetBits(W25_CS_GPIO, W25_CS_PIN);   /* CS 拉低选中 Flash */
}

void MYSPI_Stop(void)
{
    GPIO_SetBits(W25_CS_GPIO, W25_CS_PIN);     /* CS 拉高释放 */
}

void MYSPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(W25_SPI_RCC, ENABLE);

    /* CS: 普通推挽输出 */
    GPIO_InitStructure.GPIO_Pin = W25_CS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(W25_CS_GPIO, &GPIO_InitStructure);

    /* SCK / MISO / MOSI: 复用功能 AF5 = SPI1 */
    GPIO_InitStructure.GPIO_Pin = W25_SCK_PIN | W25_MISO_PIN | W25_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, W25_SCK_SRC, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, W25_MISO_SRC, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, W25_MOSI_SRC, GPIO_AF_SPI1);

    MYSPI_Stop();                               /* CS 空闲拉高 */

    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;  /* SPI 模式 0 (W25Q64) */
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;  /* 84MHz/16 = 5.25MHz */
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(W25_SPI, &SPI_InitStructure);
    SPI_Cmd(W25_SPI, ENABLE);
}

/* 全双工交换一字节: 主模式发送同时接收, 轮询 TXE/RXNE 标志 */
uint8_t MYSPI_SwapByte(uint8_t ByteValue)
{
    while (SPI_I2S_GetFlagStatus(W25_SPI, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(W25_SPI, ByteValue);
    while (SPI_I2S_GetFlagStatus(W25_SPI, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(W25_SPI);
}
