#include "stm32f10x.h"                  // Device header
#include "MYADC.h"

/* 原定义：uint16_t ADC_Value[2]; */
/* 改为 volatile：DMA 硬件持续写入，必须防止编译器缓存优化 */
volatile uint16_t ADC_Value[2];

/* 最新采样邮箱队列（长度 1，ISR 覆盖写，任务 Peek 读） */
QueueHandle_t xAdcQueue;

void MYADC_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // ADC clock = PCLK2/6 = 72/6 = 12MHz

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_55Cycles5);

    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 2;
    ADC_Init(ADC1, &ADC_InitStructure);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_BufferSize = 2;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&ADC_Value;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* 创建最新采样邮箱队列。
     * 必须在使能 DMA 中断之前创建，避免中断触发时队列未就绪 */
    xAdcQueue = xQueueCreate(1, sizeof(AdcSample_t));

    /* 使能 DMA 传输完成中断（每轮两通道转换完成触发一次）。
     * 触发时刻 ADC_Value[0]/[1] 均为 DMA 刚写入的同轮快照，打包入队 */
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    /* NVIC 使能 DMA1_Channel1 中断。
     * 抢占优先级 5 = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(5)，
     * 即中断优先级数值 >=5，处于 FreeRTOS 管控范围，可在 ISR 中安全调用
     * xQueueOverwriteFromISR（优先级数值越大优先级越低） */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    ADC_DMACmd(ADC1, ENABLE);

    ADC_Cmd(ADC1, ENABLE);
    
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/* DMA1_Channel1 传输完成中断。
 * 每轮 2 通道转换完成时触发，此刻 ADC_Value[0]/[1] 是 DMA 刚写入的
 * 同一轮完整快照（下一次写入要等下一轮转换 ~11.3us 才发生，ISR 拷贝
 * 仅 ~100ns，不会读到撕裂数据）。打包后覆盖写入邮箱，任务侧 Peek 读。 */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC1);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        AdcSample_t sample;

        sample.Lum  = ADC_Value[0];
        sample.Temp = ADC_Value[1];

        /* 覆盖写：队列满也直接更新为最新值，始终保留最近一轮快照 */
        xQueueOverwriteFromISR(xAdcQueue, &sample, &xHigherPriorityTaskWoken);

        /* 若 Peek 等待的任务被唤醒，则请求上下文切换 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

