#!/bin/bash
# flash.sh - 使用 OpenOCD + ST-Link 烧录固件
# 用法: bash flash.sh [elf文件路径]  (默认 build/firmware.elf)

ELF="${1:-build/firmware.elf}"

if [ ! -f "$ELF" ]; then
    echo "!!! 固件不存在: $ELF"
    echo "    请先编译生成 .elf 文件"
    exit 1
fi

echo ">>> 烧录 $ELF 到 STM32F103C8 @ 0x08000000 ..."
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "program $ELF verify reset exit 0x08000000"
