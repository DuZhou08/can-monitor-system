#include "stm32f10x.h"                  // Device header
#include "LED.h"
#include "OLED.h"
#include "MPU6050.h"
#include "MYApp.h"
#include "MYCAN.h"
#include "Delay.h"


int main(void)
{
	/* FreeRTOS 要求：NVIC 优先级分组必须为全抢占优先级（无子优先级）。
	 * 必须在任何外设中断使能/调度器启动之前设置。
	 * UART1(USART1) 为预留调试端口（MYUART.c），其分组同样由本处统一设置。 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	Delay_Init();
	LED_Init();
	OLED_Init();
	MPU6050_Init();
	MYCAN_Init();
	FreeRTOS_Start();			//创建任务并启动调度器
	
	while (1)
	{
		
	}
}
