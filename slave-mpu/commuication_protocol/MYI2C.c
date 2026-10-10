#include "stm32f10x.h"                  // Device header
#include "MYI2C.h"

/**
 * @brief 软件 I2C 初始化 (MPU6050 通信)
 * @param None
 * @return void
 */
void MYI2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
}

void MYI2C_W_SCL(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_10, (BitAction)BitValue);
    Delay_us(10);
}

void MYI2C_W_SDA(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)BitValue);
    Delay_us(10);
}

uint8_t MYI2C_R_SDA(void)
{
    Delay_us(10);
    return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11);
}

void MYI2C_Start(void)
{
    MYI2C_W_SDA(1);
    MYI2C_W_SCL(1);
    MYI2C_W_SDA(0);
    MYI2C_W_SCL(0);
}

void MYI2C_Stop(void)
{
    MYI2C_W_SDA(0); 
    MYI2C_W_SCL(1);
    MYI2C_W_SDA(1);
}

void MYI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        MYI2C_W_SDA(Byte & 0x80);
        Byte <<= 1;
        MYI2C_W_SCL(1);
        MYI2C_W_SCL(0);
    }
    
}

uint8_t MYI2C_ReciveByte(void)
{
    uint8_t i;
    uint8_t Byte = 0x00;
    MYI2C_W_SDA(1);
    for (i = 0; i < 8; i++)
    {
        MYI2C_W_SCL(1);
        if (MYI2C_R_SDA() == 1)
        {
            Byte |= (0x80 >> i);
        }
        MYI2C_W_SCL(0);
    }
    return Byte;
}

void MYI2C_SendACK(uint8_t AckBite)
{
    MYI2C_W_SDA(AckBite);
    MYI2C_W_SCL(1);
    MYI2C_W_SCL(0);
}

uint8_t MYI2C_ReciveACK(void)
{
    uint8_t AckBite;
    MYI2C_W_SDA(1);
    MYI2C_W_SCL(1);
    AckBite = MYI2C_R_SDA();
    MYI2C_W_SCL(0);
    return AckBite;
}
