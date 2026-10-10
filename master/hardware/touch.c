#include "stm32f4xx.h"
#include "touch.h"
#include "tft.h"

/* ================= 标定参数 =================
 * 实测四角(2026-08): X≈240~3550, Y≈180~3850(顶部rawY大, 底部小)
 * 映射公式见 TOUCH_GetXY, 已含 Y 镜像. */
#define TOUCH_X_MIN     240
#define TOUCH_X_MAX     3700
#define TOUCH_Y_MIN     180
#define TOUCH_Y_MAX     3850

/* 手动微调: 触摸点整体偏移修正(单位: 像素).
 * 触摸点偏左上 → 两个都加正值; 偏右下 → 减; 只偏左 → 只加 X. */
#define TOUCH_X_TRIM    0
#define TOUCH_Y_TRIM    0

/* XPT2046 标称时钟 ≤2.5MHz, TFT_Init 配的是 10.5MHz (BR=4).
 * 触摸读取时临时降速, 读完恢复, 保证采样稳定. */
#define TOUCH_SPI_SLOW_BR    SPI_BaudRatePrescaler_32   /* 42M/32 = 1.3MHz */
#define TOUCH_SPI_FAST_BR    SPI_BaudRatePrescaler_4    /* 42M/4  = 10.5MHz */

static void TOUCH_SPI_SetBaud(uint16_t br)
{
	SPI2->CR1 &= ~SPI_CR1_SPE;                            /* 关 SPI 才能改分频 */
	SPI2->CR1 = (SPI2->CR1 & ~SPI_CR1_BR) | br;
	SPI2->CR1 |= SPI_CR1_SPE;
}

static uint8_t SPI_WriteRead(uint8_t byte)                /* SPI2 全双工: 发一字节同时收一字节 */
{
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {}
	SPI_I2S_SendData(SPI2, byte);
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {}
	return SPI_I2S_ReceiveData(SPI2);
}

static uint16_t XPT_ReadAD(uint8_t cmd)
{
	uint8_t b1, b2, b3;

	SPI_WriteRead(cmd);                                   /* 发命令 */
	b1 = SPI_WriteRead(0x00);                             /* 转换期垃圾字节, 丢弃 */
	(void)b1;                                             /* 故意读取不用于取数 */
	b2 = SPI_WriteRead(0x00);                             /* 高字节 */
	b3 = SPI_WriteRead(0x00);                             /* 低字节 */
	return (uint16_t)(((b2 & 0x7F) << 5) | (b3 >> 3));    /* 拼出 12bit (0~4095) */
}

void TOUCH_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE); /* tft.c 已使能, 重复无害 */

	GPIO_InitStructure.GPIO_Pin = TOUCH_CS_PIN;           /* T_CS 推挽输出, 初始不选中 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(TOUCH_CS_GPIO, &GPIO_InitStructure);
	TOUCH_CS_H();

	GPIO_InitStructure.GPIO_Pin = TOUCH_IRQ_PIN;          /* T_IRQ 上拉输入, 读取电平 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(TOUCH_IRQ_GPIO, &GPIO_InitStructure);
}

uint8_t TOUCH_GetXY(uint16_t *x, uint16_t *y)
{
	uint16_t rawX, rawY;
	int32_t sx, sy;

	if (TOUCH_IRQ_READ() != 0)                            /* IRQ 高 = 未按下 */
	{
		return 0;
	}

	TOUCH_SPI_SetBaud(TOUCH_SPI_SLOW_BR);				  /* 读取时切换低速*/
	TOUCH_CS_L();
	rawX = XPT_ReadAD(0xD0);                              /* 0xD0 = 读 X 通道 */
	rawY = XPT_ReadAD(0x90);                              /* 0x90 = 读 Y 通道 */
	TOUCH_CS_H();
	TOUCH_SPI_SetBaud(TOUCH_SPI_FAST_BR);

	sx = (int32_t)rawX - TOUCH_X_MIN;                     /* 标定映射 + 边界钳位 */
	if (sx < 0) sx = 0;
	sx = sx * (TFT_WIDTH - 1) / ((int32_t)TOUCH_X_MAX - TOUCH_X_MIN);
	if (sx >= TFT_WIDTH) sx = TFT_WIDTH - 1;

	sy = (int32_t)rawY - TOUCH_Y_MIN;
	if (sy < 0) sy = 0;
	sy = sy * (TFT_HEIGHT - 1) / ((int32_t)TOUCH_Y_MAX - TOUCH_Y_MIN);
	if (sy >= TFT_HEIGHT) sy = TFT_HEIGHT - 1;
	sy = (TFT_HEIGHT - 1) - sy;               /* 上下镜像: 触摸面板 Y 轴与屏幕反向 */

	*x = (uint16_t)(sx + TOUCH_X_TRIM);
	*y = (uint16_t)(sy + TOUCH_Y_TRIM);
	return 1;
}
