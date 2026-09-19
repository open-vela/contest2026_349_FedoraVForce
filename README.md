# contest2026_349_FedoraVForce

## 一、介绍

本项目是 **2026 首届 openvela AI 硬件开发者大赛 · 新硬件适配赛道**（队伍 349
FedoraVForce）的参赛作品：为 **VSAI LogicPi A1** 开发板适配 NuttX/openvela。

完成内容：

- 芯片级支持 `arch/arm64/src/s6`（最小启动、GICv3 中断、UART、MIPI DSI、GPIO、I2C、TOUCH），
  位于 nuttx 分仓 [U2FsdGVkX1/vela-nuttx](https://github.com/U2FsdGVkX1/vela-nuttx)
  的 `dev-ai-contest-2026` 分支
- 板级适配（本仓 `board/contest_board/`，经 manifest `<linkfile>` 软链到
  `vendor/openvela/boards/contest2026_349_board`）
- LVGL 桌面应用 `app/launcher`（开机自动运行）

目录结构：

```text
board/contest_board/   # 板级适配（映射到 vendor/openvela/boards/contest2026_349_board）
app/launcher/          # LVGL 桌面应用
logs/                  # AI Coding 日志
```

## 二、硬件信息

以下为开发板官方规格：

| 项目 | 参数 |
| ---- | ---- |
| SoC | Amlogic A311Y2，6nm AI SoC |
| CPU | 四核 ARM Cortex-A510 @ 2.4GHz，集成 RISC-V 控制核 |
| GPU | Mali-G310 V5 |
| NPU | 4 TOPS（INT4/INT8/FP16，本次适配未使用） |
| 内存 | 8GB LPDDR5 |
| 存储 | 16GB eMMC 5.2 |
| 供电 | 12V DC 单电源；核心板与底板经 Hirose 板对板连接器互联 |

底板初始化（DDR/时钟/pinmux）由板载 BL31/U-Boot 完成，NuttX 以
bare-metal 方式从 `0x03080000` 启动（U-Boot `booti` 重定位后的入口地址，
见 `board/contest_board/scripts/dramboot.ld`）。

## 三、编译过程

本项目**必须配合特制 nuttx 分仓**编译：`repo sync` 得到的 `nuttx/` 不含
s6 芯片支持，编译前需先切换到分仓分支：

```bash
cd <openvela 工作区>/nuttx
git remote add fork https://github.com/U2FsdGVkX1/vela-nuttx.git
git fetch fork dev-ai-contest-2026
git checkout -B dev-ai-contest-2026 FETCH_HEAD
```

然后在 openvela 工作区根目录（本仓上一级）编译：

```bash
./build.sh vendor/openvela/boards/contest2026_349_board/configs/nsh -j
```

产物为 `nuttx.bin`，复制到 U 盘即可部署。

## 四、启动方法

准备 U 盘：第一个分区放编译得到的 `nuttx.bin`

在 A1 的 U-Boot 命令行中执行：

```text
usb start
load usb 0:1 0x03000000 /nuttx.bin
fdt rm /reserved-memory/linux,mte
setenv bootargs
booti 0x03000000 - 0x01000000
```

说明：

- `booti` 将镜像加载到 `0x03000000`，由于 ARM64 Image 头的 `0x480000`
  文本偏移，实际入口被重定位到 `0x03080000`（与链接脚本一致）
- `setenv bootargs` 清空 Linux 启动参数，NuttX 不需要它

启动后在 UART0（921600）串口进入 NSH，屏幕自动显示 LVGL 桌面。
