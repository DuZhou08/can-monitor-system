#ifndef __ADC_H
#define __ADC_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"

/* ADC 采样快照结构体：一次 DMA 传输完成时打包的一对值 */
typedef struct
{
    uint16_t Lum;   /* 光照：ADC_Value[0] (PA0, ADC1_CH0) */
    uint16_t Temp;  /* 温度：ADC_Value[1] (PA1, ADC1_CH1) */
} AdcSample_t;

/* 原声明：extern uint16_t ADC_Value[2]; */
/* 改为 volatile：该数组由 DMA（硬件）持续写入，volatile 防止编译器
 * 将其缓存到寄存器/提升出循环，保证每次都从内存重新读取最新值 */
extern volatile uint16_t ADC_Value[2];

/* 方案A：最新采样邮箱队列（长度 1，ISR 覆盖写，任务 Peek 读）。
 * 在 MYADC_Init() 中创建 */
extern QueueHandle_t xAdcQueue;

void MYADC_Init(void);

#endif
