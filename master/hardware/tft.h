#ifndef __TFT_H
#define __TFT_H

#include "stm32f4xx.h"

/* ==========================================================
 *  TFT 2.4寸 SPI (ILI9341) 驱动
 *  硬件 SPI2 + 板载 TFT 排针
 * ----------------------------------------------------------
 *  TFT 模块引脚    F407 排针      MCU 引脚
 *  ------------------------------------------------
 *  SCK         ->  SCL        ->  PB13 (SPI2_SCK)
 *  MOSI/SDI    ->  SDI        ->  PB15 (SPI2_MOSI)
 *  MISO/SDO    ->  SDO        ->  PB14 (SPI2_MISO)
 *  CS          ->  CS         ->  PB12 (GPIO, 低有效)
 *  DC          ->  D/C        ->  PC5  (GPIO, 0=命令 1=数据)
 *  LED(背光)   ->  BLK        ->  PB1  (GPIO)
 *  RESET       ->  (排针无此脚)  ->  接板子 NRST 或用软件复位
 *  VCC/GND     ->  3.3V / GND
 *  Touch(5脚)  ->  不接(仅显示)
 * ========================================================== */

/* --- SPI2 数据引脚 (硬件复用, 板载排针) --- */
#define TFT_SPI             SPI2
#define TFT_SPI_RCC         RCC_APB1Periph_SPI2
#define TFT_SCK_GPIO        GPIOB
#define TFT_SCK_PIN         GPIO_Pin_13
#define TFT_SCK_SRC         GPIO_PinSource13
#define TFT_MOSI_GPIO       GPIOB
#define TFT_MOSI_PIN        GPIO_Pin_15
#define TFT_MOSI_SRC        GPIO_PinSource15
#define TFT_MISO_GPIO       GPIOB
#define TFT_MISO_PIN        GPIO_Pin_14
#define TFT_MISO_SRC        GPIO_PinSource14

/* --- 控制引脚 --- */
#define TFT_CS_GPIO         GPIOB
#define TFT_CS_PIN          GPIO_Pin_12
#define TFT_DC_GPIO         GPIOC
#define TFT_DC_PIN          GPIO_Pin_5
#define TFT_BKL_GPIO        GPIOB
#define TFT_BKL_PIN         GPIO_Pin_1

/* RESET: 排针无 RST 脚 -> 用 GPIO 控制复位, 接 PB0 */
#define TFT_RST_GPIO        GPIOB
#define TFT_RST_PIN         GPIO_Pin_0

#define TFT_CS_L()          GPIO_ResetBits(TFT_CS_GPIO, TFT_CS_PIN)
#define TFT_CS_H()          GPIO_SetBits(TFT_CS_GPIO, TFT_CS_PIN)
#define TFT_DC_L()          GPIO_ResetBits(TFT_DC_GPIO, TFT_DC_PIN)   /* 写命令 */
#define TFT_DC_H()          GPIO_SetBits(TFT_DC_GPIO, TFT_DC_PIN)     /* 写数据 */
#define TFT_RST_L()         GPIO_ResetBits(TFT_RST_GPIO, TFT_RST_PIN)
#define TFT_RST_H()         GPIO_SetBits(TFT_RST_GPIO, TFT_RST_PIN)
#define TFT_BKL_H()         GPIO_SetBits(TFT_BKL_GPIO, TFT_BKL_PIN)   /* 背光亮 */
#define TFT_BKL_L()         GPIO_ResetBits(TFT_BKL_GPIO, TFT_BKL_PIN) /* 背光灭 */

/* --- 屏幕参数 --- */
#define TFT_WIDTH           320
#define TFT_HEIGHT          240

/* MADCTL 方向/颜色序设置:
 *   0x48 = 常用默认(竖屏+MX+BGR)   0x08 = 不翻转+BGR
 *   0xC8 = 上下翻转+BGR             0x28 = 横屏(需交换宽高)
 *   0x00 = 不翻转+RGB               0x40 = 不翻转+MX+RGB
 */
#define TFT_MADCTL          0x28

/* --- 常用颜色 (RGB565) --- */
#define TFT_WHITE           0xFFFF
#define TFT_BLACK           0x0000
#define TFT_RED             0xF800
#define TFT_GREEN           0x07E0
#define TFT_BLUE            0x001F
#define TFT_YELLOW          0xFFE0
#define TFT_CYAN            0x07FF
#define TFT_MAGENTA         0xF81F
#define TFT_ORANGE          0xFD20
#define TFT_GRAY            0x8410

/* --- API --- */
void TFT_Init(void);                                    /* 引脚+SPI+屏幕初始化, 使用前必须调用一次 */
void TFT_FillScreen(uint16_t color);                    /* 全屏填充 */
void TFT_FillColor(uint16_t color, uint32_t count);     /* 连续填充 count 个像素(需先 SetWindow) */
void TFT_WritePixels(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *pixels); /* LVGL flush 专用, 批量写矩形像素区 */
void TFT_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2); /* 设定绘图窗口 */
void TFT_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void TFT_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);       /* 8x16 ASCII (江协 F8x16) */
void TFT_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg); /* 8x16 字符串 */

#endif
