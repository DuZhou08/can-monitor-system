#!/bin/bash
# ============================================================
# fix_stlink.sh - 修复 WSL2 中 ST-Link USB 设备权限
# 用法: bash fix_stlink.sh
# 原因: WSL2 中 udev 规则默认不生效, 手动修复设备权限
# ============================================================

echo ">>> 检查 ST-Link 是否已透传到 WSL2..."
if ! lsusb | grep -qi "0483:3748\|0483:374b\|ST-LINK\|STLink"; then
    echo "!!! 未检测到 ST-Link 设备!"
    echo "    请先在 Windows PowerShell(管理员)执行:"
    echo "      usbipd list          # 查看 BUSID"
    echo "      usbipd attach --wsl --busid <BUSID>"
    echo "    然后重新运行本脚本"
    exit 1
fi

lsusb | grep -i "0483\|ST-LINK\|STLink"

echo
echo ">>> 安装 udev 规则..."
sudo tee /etc/udev/rules.d/49-stlinkv2.rules > /dev/null << 'EOF'
# ST-Link V2 (0483:3748)
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", MODE="0666", TAG+="uaccess"
# ST-Link V2.1 (0483:374b)
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374b", MODE="0666", TAG+="uaccess"
EOF

echo ">>> 重启 udev 服务..."
sudo service udev restart 2>/dev/null
sudo udevadm control --reload-rules 2>/dev/null
sudo udevadm trigger 2>/dev/null

echo
echo ">>> 手动修复设备节点权限 (WSL2 下 udev 可能不生效)..."
for dev in /dev/bus/usb/*/*; do
    [ -e "$dev" ] && sudo chmod 666 "$dev"
done

echo
echo ">>> 验证 OpenOCD 能否识别 ST-Link..."
timeout 10 openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init; exit" 2>&1 | grep -E "STLINK|Info.*srst|Error" | head -5

echo
echo ">>> 完成! 如果看到 'STLINK V2' 信息则 ST-Link 就绪, 可以烧录了。"
