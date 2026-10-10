# master — Master Console (STM32F407VET6)

STM32F407 + FreeRTOS + LVGL CAN console: aggregates two slaves (light/temperature, MPU6050 attitude), with touch display / alarms / diagnostics, bidirectional commands, and power-loss persistence.

**English** | [简体中文](README.md)

> Shared docs: [root README](../README.en.md) · [protocol](../docs/PROTOCOL.en.md) · [pinout](../docs/PINOUT.en.md) · [hardware](../docs/HARDWARE.en.md)

## Features

- Touch UI, 4 pages: Overview / Node / Diagnostics / Settings (live values + charts + alarm coloring)
- Aggregates `0x300` / `0x301`: frame-loss stats, 2 s disconnect detection
- Bidirectional commands: sends `0x500` (SET_PERIOD / PING), receives `0x310` / `0x311` ACK
- Bus error diagnostics: TEC / REC / LEC / BOFF
- Threshold persistence (W25Q64 SPI flash)

## Tasks

| Task        | Priority | Stack | Responsibility                            |
| ----------- | -------- | ----- | ----------------------------------------- |
| `vTaskLVGL` | 1        | 2048  | LVGL render + touch + heartbeat LED       |
| `CAN_Task`  | 2        | 256   | poll CAN FIFO -> data layer / diagnostics |

## Hardware

STM32F407VET6 · ILI9341 320x240 (SPI2) + HR2046 touch · W25Q64 (SPI1) · TJA1050 (5 V). See [`HARDWARE.md`](../docs/HARDWARE.en.md).

![Master hardware](../docs/images/hardware/Master.jpg)

*Master: STM32F407VET6 + ILI9341 TFT + W25Q64 + TJA1050*

## Build & Flash

VSCode + **EIDE** one-click build / flash (arm-none-eabi-gcc).

## Layout

```
MYapp/                 application (LVGL UI / CAN monitor / tasks)
commuication_protocol/ comm drivers (CAN / SPI)
hardware/              board drivers (TFT / touch / flash / LED)
user/  lvgl/  FreeRtos/  system/  start/  library/
```
