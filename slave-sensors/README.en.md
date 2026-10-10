# slave-sensors — Slave 0 (STM32F103C8T6)

Slave 0 of the three-node CAN system: ADC acquisition of light / temperature, local OLED display, periodic report to the master, command handling with ACK.

**English** | [简体中文](README.md)

> Shared docs: [root README](../README.en.md) · [protocol](../docs/PROTOCOL.en.md) · [pinout](../docs/PINOUT.en.md) · [hardware](../docs/HARDWARE.en.md)

## Features

- Dual-channel ADC (photoresistor GL5516 / NTC), ADC1 + DMA circular
- Periodic report `0x300` (DLC=5): light u16 · temp u16 · seq u8
- Command `0x500` (SET_PERIOD / PING) -> ACK `0x310` (with TEC / REC)
- Frame-loss sequence, bus-off self-recovery; live OLED display

## Tasks

| Task          | Priority | Period | Responsibility               |
| ------------- | -------- | ------ | ---------------------------- |
| `vTaskLED`    | 1        | 500 ms | heartbeat LED (onboard PC13) |
| `vTaskOLED`   | 1        | 70 ms  | show light / temperature     |
| `vTaskCAN_TX` | 1        | 70 ms  | pack & report `0x300`        |
| `vTaskCAN_RX` | 2        | 5 ms   | claim command + send ACK     |

> The `vTaskCAN_TX` period can be changed by the master via SET_PERIOD (20~1000 ms).

## Concurrency

ADC writes `ADC_Value[]` continuously via DMA; the DMA-complete ISR packs the **same-round snapshot** into a **mailbox queue** (length 1); the OLED / CAN tasks read the latest value with `xQueuePeek`. Every shared variable has a single writer.

## Hardware

STM32F103C8T6 · light (ADC) + NTC (ADC) · OLED (I2C) · TJA1050 (5 V). See [`HARDWARE.md`](../docs/HARDWARE.en.md).

![Slave 0 hardware](../docs/images/hardware/Slave_sensors.jpg)

*Slave 0: STM32F103C8T6 + light/temperature sensors + OLED + TJA1050*

## Build & Flash

VSCode + **EIDE** build; or `bash scripts/flash.sh` (OpenOCD + ST-Link).

## Layout

```
app/                   application (tasks / framing)
commuication_protocol/ comm drivers (CAN; UART reserved for debug)
hardware/              board drivers (ADC / OLED / LED)
```
