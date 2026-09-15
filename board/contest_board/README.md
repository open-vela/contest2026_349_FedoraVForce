# contest2026_349_board（VSAI LogicPi A1）

映射到 openvela `vendor/openvela/boards/contest2026_349_board`，为
VSAI LogicPi A1（Amlogic Meson S6，Cortex-A55，GICv3）提供板级适配。

- 芯片级支持位于 nuttx `arch/arm64/src/s6`（dev-ai-contest-2026 分支）。
- DDR/时钟/pinmux/显示通路由 BL31/U-Boot 初始化，NuttX 从 `0x03080000`
  启动（U-Boot `booti` 重定位后的地址），控制台为 UART0（UART-B）921600。
- 构建配置：`configs/nsh/defconfig`（自定义板机制 `ARCH_BOARD_CUSTOM`）：
  `./build.sh vendor/openvela/boards/contest2026_349_board/configs/nsh -j`
