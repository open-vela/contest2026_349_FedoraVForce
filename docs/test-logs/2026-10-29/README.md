# 上板测试日志（2026-10-29）

板子：VSAI LogicPi A1。镜像：nuttx `5a359897`（s6-eth）+ 比赛仓 `launcher-v2`，`-O2`、LVGL 刷新周期 16 ms。从 U-Boot 经 TFTP 启动。

串口 921600，经板载 CP2102N 接到主机；测试命令通过 telnet（板上 telnetd）执行。日志里的主机地址、板子地址、MAC、eMMC 序列号、芯片 OTP license 值已替换成占位符，其他内容是原样输出。

| 文件 | 内容 | 结果 |
|---|---|---|
| `boot.log` | 复位后完整的串口启动日志：Amlogic BL2 → U-Boot → TFTP → `booti` → NuttX（`Boot from EL2` → `Boot from EL1`）→ launcher、NSH | launcher 打开 framebuffer 时板上时间为 27.9 s（计数从复位开始）；触摸 `maxpoint 5`；`eth0: link up, 100 Mbps full duplex` |
| `system-info.log` | `uname -a`、`uptime`、`free`、`ps` | 4 个 CPU 的 IDLE 线程都在，launcher / telnetd 运行 |
| `smp-test.log` | `smp`（`CONFIG_TESTING_SMP`） | 8 个线程在 CPU0–3 之间迁移，全部 `completed with result=0` |
| `iperf-board-rx.log` | 板上 `iperf -s`，主机发数据 12 s | 板子接收 94.47 Mbit/s |
| `iperf-board-tx.log` | 板上 `iperf -c`，主机接收 10 s | 板子发送 57.99 Mbit/s |
| `paint-recognition.log` | 画板手写识别 24 次的串口输出 | 全部 `on TFLite Micro`；24 次对 20 次（错误 3 次是 5 写得像 8，另 1 次是 9 或 2） |

说明：

- 主机没有装 iperf，对端用的是 `ncat`，所以吞吐以板上 iperf 的报告为准。
- `boot.log` 里有几行字符交错（例如 `N ut[tSLheVllG`），那是两个 CPU 同时往串口打印造成的，原样保留。
- 并行跑多份 `coremark` 的结果没有放进来：flat build 下同一个内置应用的多个实例共用全局变量，分数不可信。单核 CoreMark 见 `docs/launcher-v2/fps.md`。
