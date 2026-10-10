#ifndef __MYSPI_H
#define __MYSPI_H

#include "stm32f4xx.h"

/* ================================================================
 *  MYSPI —— W25Q64 Flash 硬件 SPI1 驱动 (F407VET6)
 *  --------------------------------------------------------------
 *  引脚: PA4 = CS (通用推挽, 低有效)
 *        PA5 = SCK / PA6 = MISO / PA7 = MOSI (SPI1, AF5)
 *  说明: F1 原版为软件模拟 SPI; F407 版改用硬件 SPI1 外设.
 *        模式0 (CPOL=0, CPHA=1Edge), 主模式, 5.25MHz.
 *        注意: TFT/触摸占用 SPI2(PB12~15), SPI1 空闲, 二者无冲突.
 * ================================================================ */

#define W25_SPI             SPI1
#define W25_SPI_RCC         RCC_APB2Periph_SPI1

#define W25_CS_GPIO         GPIOA
#define W25_CS_PIN          GPIO_Pin_4
#define W25_SCK_GPIO        GPIOA
#define W25_SCK_PIN         GPIO_Pin_5
#define W25_SCK_SRC         GPIO_PinSource5
#define W25_MISO_GPIO       GPIOA
#define W25_MISO_PIN        GPIO_Pin_6
#define W25_MISO_SRC        GPIO_PinSource6
#define W25_MOSI_GPIO       GPIOA
#define W25_MOSI_PIN        GPIO_Pin_7
#define W25_MOSI_SRC        GPIO_PinSource7

void MYSPI_Init(void);
void MYSPI_Start(void);
void MYSPI_Stop(void);
uint8_t MYSPI_SwapByte(uint8_t ByteValue);

#endif
