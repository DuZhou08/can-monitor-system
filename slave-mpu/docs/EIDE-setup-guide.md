# EIDE 配置图文步骤(VSCode 内操作)

> 前置条件(已完成,无需重复):
> - ✅ WSL2 内已装 arm-none-eabi-gcc、openocd、gdb-multiarch、.NET 6
> - ✅ 工程启动文件/FreeRTOS 移植已切换为 GCC 版
> - ✅ 已验证工程可在 GCC 下完整编译
> - ⚠️ 未完成:ST-Link USB 透传(见 `docs/Windows-usbipd-guide.md`)

---

## 第一步:安装 VSCode 插件(在 WSL 远端)

1. 通过 VSCode **Remote-WSL** 连接进入工程目录
2. 按 `Ctrl+Shift+X` 打开扩展面板(确保左上角显示 **WSL: Ubuntu**)
3. 安装以下插件:
   - **Embedded IDE** (CL.eide) ← 核心
   - **C/C++** (ms-vscode.cpptools) ← 代码补全
   - **Cortex-Debug** (marus25.cortex-debug) ← 调试(推荐)
4. 安装 EIDE 后**重启 VSCode**,等它初始化。打开"输出"面板(查看 → 输出),下拉选 **eide-log**,应看到初始化成功日志。

> 如果 eide-log 报 .NET 找不到:
> ```bash
> # 在 WSL 终端确认 dotnet 6 可用
> ~/.dotnet/dotnet --version   # 应输出 6.0.36
> # 若不行, 把 ~/.dotnet 加进 PATH
> echo 'export PATH="$HOME/.dotnet:$PATH"' >> ~/.bashrc && source ~/.bashrc
> # 然后完全关闭 VSCode, 重新 code .
> ```

---

## 第二步:设置工具链路径

1. 点击 VSCode 左侧栏的 **EIDE 图标**(芯片形状)
2. 下方 **OPERATIONS(操作)** 区域 → 点击 **"设置工具链路径"(Set ToolChain Path)**
3. 选择 **ARM-GCC**,填写:
   - 工具链安装目录:`/usr/bin`(Ubuntu 的 gcc-arm-none-eabi 装在 /usr/bin)
   - 工具链前缀:默认 `arm-none-eabi-`
4. 确认设置项前出现 **✓**

---

## 第三步:导入 Keil 工程

1. OPERATIONS → **"导入项目"(Import Project)**
2. 选择 **Keil MDK Project**
3. 浏览选择工程文件:`code.uvprojx`
4. 弹出"是否与原有 KEIL 项目共存于同一目录"→ 选 **是**(源文件原地使用)
5. 等待导入完成,如提示切换工作区 → 确认

> ⚠️ 如果导入时报"找不到 Keil"或要求 Keil 路径:直接跳过/忽略即可——我们不用 Keil,编译器选项稍后手动补。

---

## 第四步:配置构建器(编译器)

在 EIDE 项目面板展开项目:

1. **构建配置(Builder Configurations)** → 点击切换按钮 → 选择 **ARM-GCC**
2. 展开构建配置,检查/设置:
   - **CPU 类型**:`Cortex-M3`
   - **浮点类型**:`None`(M3 无 FPU)
   - **链接器脚本(RAM/ROM 布局)**:将 `使用自定义链接脚本` 设为 **false**,然后点 **"更改"** 打开存储器布局编辑器,填入:
     - ROM 起始:`0x08000000`,大小:`0x10000`(64KB)
     - RAM 起始:`0x20000000`,大小:`0x5000`(20KB)
3. **项目属性 → 预处理宏定义(Preprocessor Definitions)**,确认包含:
   - `USE_STDPERIPH_DRIVER`
   - `STM32F10X_MD`(已在 stm32f10x.h 中激活,可留可删)
4. **项目属性 → 头文件路径(Include Directories)**,确认包含:
   - `start`
   - `library`
   - `user`
   - `system`
   - `hardware`
   - `app`
   - `commuication_protocol`
   - `FreeRtos/include`
   - `FreeRtos/portable`
   - `FreeRtos/portable/ARM_CM3`
   - `FreeRtos/source`

> 导入成功后这些路径通常已自动填好,重点核对 `FreeRtos/portable/ARM_CM3` 在列表中。

---

## 第五步:编译

1. 打开任意 .c 文件(如 `app/application.c`)
2. 点击 VSCode 右上角 **Build(构建)** 按钮,或 `Ctrl+Shift+B`
3. 底部终端应出现编译过程,最终输出类似:
   ```
   [build] build finished successfully
   ```

如果报错,对照文档末尾"常见编译错误"排查。

---

## 第六步:配置烧录(OpenOCD + ST-Link)

**前置:先完成 Windows 侧 USB 透传(见 `docs/Windows-usbipd-guide.md`),并运行 `bash scripts/fix_stlink.sh` 修复权限。**

1. EIDE 项目面板 → **烧录配置(Programmer/Download Configuration)**
2. 点击切换烧录器 → 选择 **OpenOCD**
3. 展开配置:
   - **OpenOCD 路径**:`/usr/bin/openocd`(或插件设置里填)
   - **接口配置文件(Interface)**:`interface/stlink.cfg`
   - **目标配置文件(Target)**:`target/stm32f1x.cfg`
   - **下载算法**:`STM32F10x_128`
4. 点击右上角 **下载/烧录(Download)** 按钮

烧录成功后终端会显示类似:
```
Info : STLINK V2 ...
Info : stm32f1x.cpu: hardware has 4 breakpoints, 2 watchpoints
** Programming Started **
** Programming Finished **
```

---

## 第七步:调试(可选,Cortex-Debug)

1. 点击左侧 **运行与调试** → 创建 `launch.json`
2. 填入:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "ST-Link Debug",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "openocd",
            "cwd": "${workspaceRoot}",
            "executable": "${workspaceRoot}/build/Debug/firmware.elf",
            "device": "STM32F103C8",
            "configFiles": [
                "interface/stlink.cfg",
                "target/stm32f1x.cfg"
            ],
            "svdFile": "${workspaceRoot}/STM32F103xx.svd",
            "runToEntryPoint": "main"
        }
    ]
}
```

3. 按 `F5` 开始调试,可下断点、看变量、看寄存器

> SVD 文件(EIDE 安装芯片支持包后)位于 `~/.eide/tools/.../SVD/STM32F103xx.svd`,路径以实际为准。

---

## 日常工作流

```
1. Windows: usbipd attach --wsl --busid <BUSID>   (开机后一次)
2. WSL:     bash scripts/fix_stlink.sh            (权限修复, 可选)
3. VSCode:  Ctrl+Shift+B 编译
4. VSCode:  点烧录按钮 下载到板子
5. VSCode:  F5 调试
```

---

## 常见编译错误排查

| 错误 | 原因 | 解决 |
|---|---|---|
| `arm-none-eabi-gcc: No such file` | 工具链路径没配好 | 第二步重新设置工具链路径为 /usr/bin |
| `selected FPU does not support` | CPU 类型/FPU 配置错 | CPU 选 Cortex-M3,FPU 选 None |
| `undefined reference to 'main'` | 源文件没加全 | 检查 user/main.c 在项目中 |
| `cannot find -lc` / 链接脚本错 | RAM/ROM 布局错误 | 第四步核对 0x08000000/0x10000、0x20000000/0x5000 |
| `LIBUSB_ERROR_ACCESS` | ST-Link 权限 | `bash scripts/fix_stlink.sh` |
| `unable to find a matching device` | ST-Link 没透传 | Windows 侧 usbipd attach |
| `Info : Target voltage` 但烧录失败 | 目标板未上电/接线 | 检查 ST-Link 与板子的 SWD 接线(VCC/GND/SWDIO/SWCLK) |

---

## 参考

- EIDE 官方文档:https://em-ide.com/zh-cn/docs/intro/
- 本工程已验证:42 个源文件全部 GCC 编译通过,固件 ~24.6KB(Flash 64KB)
- 备份:原 Keil 版文件在 `/tmp/0-2-first-version-backup.tar.gz`(WSL 重启后失效,如需长期保留请移到工程外)
