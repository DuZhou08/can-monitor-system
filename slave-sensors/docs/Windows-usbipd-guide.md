# Windows 侧配置:USB 透传 ST-Link 到 WSL2

> 目的:让 WSL2 能访问 ST-Link 烧录器(USB 设备)。
> 原理:WSL2 是虚拟机,默认看不到 USB 设备。用微软官方工具 usbipd-win 把 Windows 的 USB 设备"借"给 WSL2。
> **以下所有操作都在 Windows 的 PowerShell 中进行,不是 WSL!**

---

## 第一步:安装 usbipd-win(一次)

1. 按 `Win` 键,输入 `PowerShell`,右键 **"以管理员身份运行"**
2. 在管理员 PowerShell 里执行:

```powershell
winget install dorssel.usbipd-win
```

3. 看到 "Successfully installed" 即完成。若提示重启,重启一次。
4. 关闭当前窗口,**重新打开一个管理员 PowerShell**(让 PATH 生效)。

---

## 第二步:插入 ST-Link 并查看设备(每次烧录前)

1. 把 **ST-Link 插入电脑 USB 口**(不用管 Windows 是否识别驱动,usbipd 能看到就行)
2. 管理员 PowerShell 执行:

```powershell
usbipd list
```

3. 会看到类似输出:

```
BUSID  VID:PID     DEVICE                         STATE
1-8    0483:3748   STMicroelectronics ST-LINK/V2  Not attached
```

4. **记下 BUSID**——上例中是 `1-8`(你的可能不同)。VID `0483:3748` 就是 ST-Link V2。

> ⚠️ 如果列表里没有 ST-Link:检查 USB 线/口是否正常,换一个 USB 口重试。

---

## 第三步:绑定设备(只需一次)

管理员 PowerShell 执行(把 `1-8` 换成你实际的 BUSID):

```powershell
usbipd bind --busid 1-8
```

执行后 ST-Link 在 Windows 设备管理器里可能"消失",这是**正常现象**(已由 usbipd 接管)。

验证:

```powershell
usbipd list
```

此时该行 STATE 应变为 **Shared**。

---

## 第四步:附加到 WSL2(每次开机/重启 WSL 后执行)

管理员 PowerShell 执行:

```powershell
usbipd attach --wsl --busid 1-8
```

看到类似输出即成功:

```
usbipd: info: Using default distribution 'Ubuntu-26.04'
```

验证(可选,在 WSL 终端里):

```bash
lsusb
```

应能看到 `0483:3748 STMicroelectronics ST-LINK/V2`。

---

## 第五步:回到 WSL 修复权限并烧录

在 **VSCode 的 WSL 终端**里:

```bash
cd ~/.project/0-2\ first\ version
bash scripts/fix_stlink.sh      # 修复 USB 权限, 验证 OpenOCD 识别
```

看到 `STLINK V2 ... Info : clock speed` 之类信息即全部就绪,可以烧录了。

---

## 日常使用速查表

| 场景 | 命令(Windows 管理员 PowerShell) |
|---|---|
| 查看 ST-Link 是否可见 | `usbipd list` |
| 绑定(只需一次) | `usbipd bind --busid <BUSID>` |
| 附加到 WSL(每次开机) | `usbipd attach --wsl --busid <BUSID>` |
| 解除附加(可选) | `usbipd detach --busid <BUSID>` |

## 常见问题

| 现象 | 原因与解决 |
|---|---|
| `usbipd` 不是内部或外部命令 | 新开管理员 PowerShell 窗口再试 |
| usbipd list 看不到 ST-Link | USB 口问题,换口;或用 `lsusb` 确认 |
| 重启 WSL 后烧录失败 | 需要重新 `usbipd attach --wsl --busid <BUSID>` |
| OpenOCD 报 `LIBUSB_ERROR_ACCESS` | 运行 `bash scripts/fix_stlink.sh` 修复权限 |
| OpenOCD 报 `unable to find a matching device` | ST-Link 未 attach,回到第四步 |

## 一键附加脚本(可选,Windows 上保存为 attach_stlink.ps1)

```powershell
$d = usbipd list | Select-String "0483:3748|0483:374b|ST-LINK"
if ($d) {
    $busid = ($d -split "\s+")[0]
    usbipd attach --wsl --busid $busid
    Write-Host "ST-Link 已附加到 WSL2 (BUSID: $busid)"
} else {
    Write-Host "未找到 ST-Link,请检查 USB 连接"
}
```

用法(管理员 PowerShell):`powershell -ExecutionPolicy Bypass -File attach_stlink.ps1`
