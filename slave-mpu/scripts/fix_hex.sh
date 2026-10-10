#!/usr/bin/env bash
# EIDE 内置 hex 输出在路径含空格时失败，此脚本由 afterBuildTasks 调用补生成
set -e

cd "$(dirname "$0")/.."

arm-none-eabi-objcopy -O ihex "build/Target 1/code.elf" "build/Target 1/code.hex"

echo "hex generated: build/Target 1/code.hex"
