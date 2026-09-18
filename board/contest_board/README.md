# contest2026_349_board（VSAI LogicPi A1）

映射到 openvela `vendor/openvela/boards/contest2026_349_board`，为
VSAI LogicPi A1（Amlogic Meson S6，Cortex-A55，GICv3）提供板级适配。

## 分仓说明

本仓只包含板级适配。芯片级支持在独立的 nuttx 分仓，本项目必须配合
它编译：

- <https://github.com/U2FsdGVkX1/vela-nuttx>（`dev-ai-contest-2026` 分支，
  基于 `open-vela/nuttx` 同名分支）

`repo sync` 得到的 `nuttx/` 不含芯片支持，编译前需把工作区 `nuttx/`
切换到上述分仓：

```bash
cd <openvela 工作区>/nuttx
git remote add fork https://github.com/U2FsdGVkX1/vela-nuttx.git
git fetch fork dev-ai-contest-2026
git checkout -B dev-ai-contest-2026 FETCH_HEAD
```

## 硬件

- 底板初始化（DDR/时钟/pinmux）由 BL31/U-Boot 完成，NuttX 从
  `0x03080000` 启动（U-Boot `booti` 重定位后的地址）。
- 控制台：UART0，921600 波特。

## 构建

在 openvela 工作区根目录执行：

```bash
./build.sh vendor/openvela/boards/contest2026_349_board/configs/nsh -j    # NSH 基线
```

部署：U-Boot 将 `nuttx.bin` 读到 `0x03080000` 后 `booti`。
