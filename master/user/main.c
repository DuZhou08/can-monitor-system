#include "stm32f4xx.h"
#include "MYapp.h"

int main(void)
{
	FreeRTOS_Start();   /* 创建应用任务并启动调度器，永不返回 */
	while (1)
	{
	}
}

