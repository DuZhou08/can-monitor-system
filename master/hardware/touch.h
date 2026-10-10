#ifndef __TOUCH_H
#define __TOUCH_H

#include "stm32f4xx.h"

/* ==========================================================
 *  2.4寸电阻触摸屏 (HR2046 = XPT2046 兼容) 驱动
 *  与 TFT 共用 SPI2 总线, 独立片选
 * ----------------------------------------------------------
 *  T_CLK -> PB13 (SPI2_SCK, 与 TFT 共用)
 *  T_DIN -> PB15 (SPI2_MOSI, 与 TFT 共用)
 *  T_DO  -> PB14 (SPI2_MISO, 与 TFT 共用)
 *  T_CS  -> PC6  (GPIO, 低有效, 触摸片选)
 *  T_IRQ -> PC7  (GPIO, 低电平=手指按下)
 * ========================================================== */

#define TOUCH_CS_GPIO        GPIOC
#define TOUCH_CS_PIN         GPIO_Pin_6
#define TOUCH_IRQ_GPIO       GPIOC
#define TOUCH_IRQ_PIN        GPIO_Pin_7

#define TOUCH_CS_L()         GPIO_ResetBits(TOUCH_CS_GPIO, TOUCH_CS_PIN)
#define TOUCH_CS_H()         GPIO_SetBits(TOUCH_CS_GPIO, TOUCH_CS_PIN)
#define TOUCH_IRQ_READ()     GPIO_ReadInputDataBit(TOUCH_IRQ_GPIO, TOUCH_IRQ_PIN)  /* 0=按下 */

/* 契约: lv_port_indev.c 调用的是全大写 TOUCH_Init / TOUCH_GetXY, 名字不能改 */
void   TOUCH_Init(void);                        /* 初始化 T_CS/T_IRQ (SPI2 由 TFT_Init 配好) */
uint8_t TOUCH_GetXY(uint16_t *x, uint16_t *y);  /* 1=按下并输出坐标(0~319/0~239), 0=未按下 */

#endif
