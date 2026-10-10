#ifndef __MYI2C_H
#define __MYI2C_H

#include "Delay.h"

void MYI2C_Init(void);
void MYI2C_Start(void);
void MYI2C_Start(void);
void MYI2C_Stop(void);
void MYI2C_SendByte(uint8_t Byte);
uint8_t MYI2C_ReciveByte(void);
void MYI2C_SendACK(uint8_t AckBite);
uint8_t MYI2C_ReciveACK(void);

#endif
