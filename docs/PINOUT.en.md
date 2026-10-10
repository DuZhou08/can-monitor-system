# Pinout

> v1.0 · 2026-10 · DuZhou

Pin assignments for all three boards. Wiring and schematic: [`HARDWARE.md`](HARDWARE.md); CAN protocol: [`PROTOCOL.md`](PROTOCOL.md).

[简体中文](PINOUT.md) | **English**

> `AF` = alternate-function number (must be set explicitly on STM32F4). STM32F1 has no AF number — configure by GPIO mode.

## Master — STM32F407VETx

| Pin  | Function  | Peripheral / signal  | Mode / AF      | Notes                  |
| ---- | --------- | -------------------- | -------------- | ---------------------- |
| PB13 | SPI2_SCK  | TFT + touch (shared) | AF5            |                        |
| PB15 | SPI2_MOSI | TFT + touch (shared) | AF5            |                        |
| PB14 | SPI2_MISO | TFT + touch (shared) | AF5            |                        |
| PB12 | TFT_CS    | ILI9341              | GPIO push-pull | active low             |
| PC5  | TFT_DC    | ILI9341              | GPIO push-pull | 0=cmd 1=data           |
| PB1  | TFT_BKL   | ILI9341 backlight    | GPIO push-pull |                        |
| PB0  | TFT_RST   | ILI9341 reset        | GPIO push-pull |                        |
| PC6  | TOUCH_CS  | HR2046               | GPIO push-pull | active low             |
| PC7  | TOUCH_IRQ | HR2046               | GPIO input     | low=pressed            |
| PA4  | W25_CS    | W25Q64               | GPIO push-pull | active low             |
| PA5  | SPI1_SCK  | W25Q64               | AF5            |                        |
| PA6  | SPI1_MISO | W25Q64               | AF5            |                        |
| PA7  | SPI1_MOSI | W25Q64               | AF5            |                        |
| PA11 | CAN1_RX   | TJA1050 RXD          | AF9 pull-up in |                        |
| PA12 | CAN1_TX   | TJA1050 TXD          | AF9 push-pull  |                        |
| PA1  | LED1      | work LED             | GPIO push-pull | active low · heartbeat |

## Slave 0 — slave-sensors (STM32F103C8T6)

| Pin  | Function | Peripheral / signal | Mode           | Notes              |
| ---- | -------- | ------------------- | -------------- | ------------------ |
| PA0  | ADC1_IN0 | light module AO     | analog in      | GL5516             |
| PA1  | ADC1_IN1 | NTC module AO       | analog in      | NTC                |
| PA11 | CAN1_RX  | TJA1050 RXD         | pull-up in     |                    |
| PA12 | CAN1_TX  | TJA1050 TXD         | AF push-pull   |                    |
| PB8  | I2C_SCL  | OLED                | soft I2C       |                    |
| PB9  | I2C_SDA  | OLED                | soft I2C       |                    |
| PC13 | work LED | onboard LED         | GPIO push-pull | heartbeat (500 ms) |

## Slave 1 — slave-mpu (STM32F103C8T6)

| Pin  | Function | Peripheral / signal | Mode           | Notes                  |
| ---- | -------- | ------------------- | -------------- | ---------------------- |
| PA11 | CAN1_RX  | TJA1050 RXD         | pull-up in     |                        |
| PA12 | CAN1_TX  | TJA1050 TXD         | AF push-pull   |                        |
| PB10 | I2C_SCL  | MPU6050             | soft I2C       |                        |
| PB11 | I2C_SDA  | MPU6050             | soft I2C       |                        |
| PB8  | I2C_SCL  | OLED                | soft I2C       | independent of MPU6050 |
| PB9  | I2C_SDA  | OLED                | soft I2C       | independent of MPU6050 |
| PC13 | work LED | onboard LED         | GPIO push-pull | heartbeat (500 ms)     |

## Bus Summary

| Bus      | Board  | Pins                    | Speed                           | Device           |
| -------- | ------ | ----------------------- | ------------------------------- | ---------------- |
| SPI1     | master | PA5 / PA6 / PA7 + PA4   | 5.25 MHz                        | W25Q64           |
| SPI2     | master | PB13 / PB14 / PB15      | 10.5 MHz (1.3125 MHz for touch) | ILI9341 + HR2046 |
| soft I2C | slave0 | PB8 / PB9               | ~                               | OLED             |
| soft I2C | slave1 | PB10 / PB11 · PB8 / PB9 | ~                               | MPU6050 · OLED   |
| CAN1     | all    | PA11 / PA12             | 125 kbps                        | TJA1050          |

## Reserved / Unused Pins

### Reserved

| Board    | Pins     | Notes                                                        |
| -------- | -------- | ------------------------------------------------------------ |
| slave0/1 | PA9/PA10 | **USART1 debug port** (`MYUART.c`, reserved, not yet called) |

### Unused (free, reusable)

| Board    | Pins    | Notes                                  |
| -------- | ------- | -------------------------------------- |
| slave0/1 | PB0/PB1 | free (former alarm/error LEDs removed) |
| slave1   | PA0     | free (former `MYADC` removed)          |
| slave1   | PA6/PA7 | free (former `MYENCODER` removed)      |
