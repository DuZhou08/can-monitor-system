#include "stm32f10x.h"

/* Cortex-M3 内核调试寄存器
 * 注意：start/core_cm3.h 为 CMSIS V1.30 旧版，未定义 DWT 类型，
 * 此处直接使用内核固定地址手动声明，不受头文件版本影响。 */
#define DWT_CTRL            (*(volatile uint32_t *)0xE0001000)   /* DWT 控制寄存器 */
#define DWT_CYCCNT          (*(volatile uint32_t *)0xE0001004)   /* 周期计数器 */
#define CoreDebug_DEMCR     (*(volatile uint32_t *)0xE000EDFC)   /* 调试异常与监控控制寄存器 */
#define DEMCR_TRCENA        (1UL << 24)                          /* 使能 DWT 跟踪位 */
#define DWT_CTRL_CYCCNTENA  (1UL << 0)                           /* 使能周期计数位 */

/**
  * @brief  初始化DWT周期计数器
  * @param  无
  * @retval 无
  * @note   必须在main()最开始调用一次；复位或退出STOP模式后需重新调用。
  *         本延时方案基于Cortex-M3内核DWT周期计数器，不占用SysTick，
  *         可与FreeRTOS调度器安全共存。
  */
void Delay_Init(void)
{
	CoreDebug_DEMCR |= DEMCR_TRCENA;	//使能DWT跟踪
	DWT_CYCCNT = 0;						//清零周期计数器
	DWT_CTRL |= DWT_CTRL_CYCCNTENA;		//使能CPU周期计数器
}

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  * @note   基于DWT周期计数器，72MHz下1us=72个周期。
  *         uint32差值运算自动处理计数器回绕。
  */
void Delay_us(uint32_t xus)
{
	uint32_t start = DWT_CYCCNT;				//记录起始计数值
	uint32_t ticks = 72 * xus;					//72MHz下1us=72个周期
	while ((DWT_CYCCNT - start) < ticks);		//等待计数差值达到目标
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while (xms--)
	{
		Delay_us(1000);
	}
}

/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while (xs--)
	{
		Delay_ms(1000);
	}
}
