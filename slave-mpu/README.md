# slave-mpu — 从机 1 (STM32F103C8T6)

三节点 CAN 系统的从机 1：MPU6050 六轴姿态采集 + 自检自恢复，OLED 显示，周期上报主机。

**简体中文** | [English](README.en.md)

> 共享文档：[根 README](../README.md) · [协议](../docs/PROTOCOL.md) · [引脚](../docs/PINOUT.md) · [硬件](../docs/HARDWARE.md)

## 功能

- MPU6050 六轴采集（软件 I2C）
- 自检 / 自恢复：`WHO_AM_I` 失联重初始化；六轴全 0（SLEEP）自动唤醒；异常连续 3 次去抖
- 周期上报 `0x301`（DLC=6）：AX i16 · AY i16 · 状态 u8 · 帧序号
- 命令 `0x500` → ACK `0x311`（含 TEC / REC）
- bus-off 自恢复；OLED 实时显示

## 任务

| 任务          | 优先级 | 周期  | 职责                       |
| ------------- | ------ | ----- | -------------------------- |
| `vTaskLED`    | 1      | 500ms | 心跳灯（板载 PC13）        |
| `vTaskOLED`   | 1      | 70ms  | 显示六轴 + 状态            |
| `vTaskMPU`    | 2      | 70ms  | 自检 / 读取 / 异常判定     |
| `vTaskCAN_TX` | 1      | 70ms  | 上报 `0x301`（临界区快照） |
| `vTaskCAN_RX` | 3      | 5ms   | 认领命令 + 回 ACK          |

> `vTaskCAN_TX` 周期可被主机 SET_PERIOD 修改（20~1000 ms）。

## 并发设计

`AX` / `AY` / `MCU_State` 由 `vTaskMPU` 写，`vTaskCAN_TX` 在**临界区快照**读取，防止跨任务读撕裂。

## 硬件

STM32F103C8T6 · MPU6050（I2C 0x68）· OLED（I2C 0x78）· TJA1050（5V）。详见 [`HARDWARE.md`](../docs/HARDWARE.md)。

![从机 1 实物](../docs/images/hardware/Slave_mpu.jpg)

*从机 1：STM32F103C8T6 + MPU6050 + OLED + TJA1050*

## 构建 & 烧录

VSCode + **EIDE** 构建；或命令行 `bash scripts/flash.sh`（OpenOCD + ST-Link）。

## 目录

```
app/                   应用层（任务 / 组帧）
commuication_protocol/ 通信驱动（CAN / I2C；UART 为预留调试口）
hardware/              板级驱动（MPU6050 / OLED / LED）
```
