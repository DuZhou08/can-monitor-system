/* USART1 (PA9=TX / PA10=RX) —— 预留调试端口 */
#ifndef __MYUART_H
#define __MYUART_H

#include "stm32f10x.h"
#include "stdio.h"
#include "stdarg.h"

uint8_t UART_TxPacket[4];
uint8_t UART_RxPacket[4];
uint8_t UART_RxFlag;

void MYUART_init(void);
void MYUART_Send_Byte(uint8_t data);
void MYUART_Send_Array(uint8_t *Array, uint8_t len);
void MYUART_Send_String(char *String);
uint32_t MYUART_Pow(uint32_t x, uint32_t y);
void MYUART_SendNum(uint32_t num, uint8_t len);
void UART_Printf(char *format, ...);
void UART_SendPacket(void);
uint8_t UART_GetRxFlag(void);



#endif
