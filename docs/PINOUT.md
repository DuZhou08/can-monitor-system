# 引脚分配 (Pinout)

> v1.0 · 2026-10 · DuZhou

三块板的引脚分配。接线与原理图见 [`HARDWARE.md`](HARDWARE.md)，CAN 协议见 [`PROTOCOL.md`](PROTOCOL.md)。

**简体中文** | [English](PINOUT.en.md)

> `AF` = 复用功能编号（STM32F4 需显式配置）。STM32F1 无 AF 编号，按 GPIO 模式配置即可。

## 主机 — STM32F407VETx

| 引脚 | 功能      | 外设 / 信号     | 模式 / AF    | 备注          |
| ---- | --------- | --------------- | ------------ | ------------- |
| PB13 | SPI2_SCK  | TFT + 触摸 共用 | AF5          |               |
| PB15 | SPI2_MOSI | TFT + 触摸 共用 | AF5          |               |
| PB14 | SPI2_MISO | TFT + 触摸 共用 | AF5          |               |
| PB12 | TFT_CS    | ILI9341         | GPIO 推挽    | 低有效        |
| PC5  | TFT_DC    | ILI9341         | GPIO 推挽    | 0=命令 1=数据 |
| PB1  | TFT_BKL   | ILI9341 背光    | GPIO 推挽    |               |
| PB0  | TFT_RST   | ILI9341 复位    | GPIO 推挽    |               |
| PC6  | TOUCH_CS  | HR2046          | GPIO 推挽    | 低有效        |
| PC7  | TOUCH_IRQ | HR2046          | GPIO 输入    | 低=按下       |
| PA4  | W25_CS    | W25Q64          | GPIO 推挽    | 低有效        |
| PA5  | SPI1_SCK  | W25Q64          | AF5          |               |
| PA6  | SPI1_MISO | W25Q64          | AF5          |               |
| PA7  | SPI1_MOSI | W25Q64          | AF5          |               |
| PA11 | CAN1_RX   | TJA1050 RXD     | AF9 上拉输入 |               |
| PA12 | CAN1_TX   | TJA1050 TXD     | AF9 推挽     |               |
| PA1  | LED1      | 工作灯          | GPIO 推挽    | 低亮 · 心跳   |

## 从机 0 — slave-sensors（STM32F103C8T6）

| 引脚 | 功能     | 外设 / 信号 | 模式      | 备注            |
| ---- | -------- | ----------- | --------- | --------------- |
| PA0  | ADC1_IN0 | 光敏模块 AO | 模拟输入  | GL5516          |
| PA1  | ADC1_IN1 | 热敏模块 AO | 模拟输入  | NTC             |
| PA11 | CAN1_RX  | TJA1050 RXD | 上拉输入  |                 |
| PA12 | CAN1_TX  | TJA1050 TXD | 复用推挽  |                 |
| PB8  | I2C_SCL  | OLED        | 软件 I2C  |                 |
| PB9  | I2C_SDA  | OLED        | 软件 I2C  |                 |
| PC13 | 工作灯   | 板载 LED    | GPIO 推挽 | 心跳闪烁(500ms) |

## 从机 1 — slave-mpu（STM32F103C8T6）

| 引脚 | 功能    | 外设 / 信号 | 模式      | 备注            |
| ---- | ------- | ----------- | --------- | --------------- |
| PA11 | CAN1_RX | TJA1050 RXD | 上拉输入  |                 |
| PA12 | CAN1_TX | TJA1050 TXD | 复用推挽  |                 |
| PB10 | I2C_SCL | MPU6050     | 软件 I2C  |                 |
| PB11 | I2C_SDA | MPU6050     | 软件 I2C  |                 |
| PB8  | I2C_SCL | OLED        | 软件 I2C  | 独立于 MPU6050  |
| PB9  | I2C_SDA | OLED        | 软件 I2C  | 独立于 MPU6050  |
| PC13 | 工作灯  | 板载 LED    | GPIO 推挽 | 心跳闪烁(500ms) |

## 总线汇总

| 总线     | 板    | 引脚                    | 速率                            | 设备             |
| -------- | ----- | ----------------------- | ------------------------------- | ---------------- |
| SPI1     | 主机  | PA5 / PA6 / PA7 + PA4   | 5.25 MHz                        | W25Q64           |
| SPI2     | 主机  | PB13 / PB14 / PB15      | 10.5 MHz（触摸降至 1.3125 MHz） | ILI9341 + HR2046 |
| 软件 I2C | 从机0 | PB8 / PB9               | ~                               | OLED             |
| 软件 I2C | 从机1 | PB10 / PB11 · PB8 / PB9 | ~                               | MPU6050 · OLED   |
| CAN1     | 全部  | PA11 / PA12             | 125 kbps                        | TJA1050          |

## 保留 / 未使用引脚

### 保留（预留功能）

| 板      | 引脚     | 说明                                            |
| ------- | -------- | ----------------------------------------------- |
| 从机0/1 | PA9/PA10 | **USART1 调试口**（`MYUART.c`，预留、暂未调用） |

### 未使用（空闲，可复用）

| 板      | 引脚    | 说明                          |
| ------- | ------- | ----------------------------- |
| 从机0/1 | PB0/PB1 | 空闲（原告警/错误灯已删除）   |
| 从机1   | PA0     | 空闲（原 `MYADC` 已删除）     |
| 从机1   | PA6/PA7 | 空闲（原 `MYENCODER` 已删除） |
