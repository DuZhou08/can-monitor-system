# CAN Communication Protocol

> v1.0 · 2026-10 · DuZhou

Three-node bus: STM32F407 master + 2× STM32F103 slaves, **125 kbps standard frames**.

[简体中文](PROTOCOL.md) | **English**

## Nodes

| Node    | Project         | Address | Role                          |
| ------- | --------------- | ------- | ----------------------------- |
| Master  | `master`        | —       | aggregate / display / command |
| Slave 0 | `slave-sensors` | `0x00`  | light / temperature           |
| Slave 1 | `slave-mpu`     | `0x01`  | MPU6050 attitude              |

## Bus

- 125 kbps: PSC 56 (master, 42M) / 48 (slaves, 36M)
- Bit timing 1 + 2 + 3 = 6 tq, sample point 50%, SJW 2 tq
- Transceiver TJA1050, **5V supply**; 120Ω at both ends; common ground across all nodes
- Pins PA11 (RX) / PA12 (TX)

## Wiring & Schematic

Full wiring, BOM and schematic: see [`HARDWARE.md`](HARDWARE.en.md).

## Messages (multi-byte = big-endian)

| ID              | Direction              | Payload                                   | DLC   |
| --------------- | ---------------------- | ----------------------------------------- | ----- |
| `0x300`         | slave-sensors → master | `light u16` `temp u16` `seq u8`           | 5     |
| `0x301`         | slave-mpu → master     | `AX i16` `AY i16` `state u8` `seq u8`     | 6     |
| `0x310` `0x311` | slave → master         | `opcode u8` `result u8` `TEC u8` `REC u8` | 4     |
| `0x500`         | master → slave         | `target u8` `opcode u8` `param u16`       | 2 / 4 |

- Slave address = `ID & 0xFF`
- Temperature: `0.1°C = raw × 260 / 1750`
- State: `0` = normal, `1` = MPU6050 fault
- Commands: `0x01` SET_PERIOD (param = period in ms, 20~1000) | `0x02` PING

## Mechanisms

- **Frame loss**: master counts by sequence-number delta; re-syncs after >2s offline
- **Disconnect**: no frame for >2s → offline
- **RX filter**: master accepts 0x300 / 0x301 / 0x310 / 0x311; slaves accept 0x500 (exact match, ID << 5)
- **Error diagnostics**: read `CAN1->ESR` → `REC[31:24]` `TEC[23:16]` `LEC[6:4]` `BOFF/EPVF/EWGF[2:0]`

## Appendix: LEC Codes

0 none · 1 stuff · 2 form · 3 ack · 4 bit-recessive · 5 bit-dominant · 6 CRC · 7 software
