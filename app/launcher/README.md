# launcher（LVGL 演示）

队伍 349 FedoraVForce 的 LVGL 触控演示应用，映射到 openvela
`packages/demos/contest2026_349_launcher`。

- 锁屏页：运行时钟，任意位置上滑或点击圆形手柄解锁
- 桌面页：时钟、heap/FPS 状态、About / Paint 两个入口
- About 页：S6 移植信息（GICv3、MIPI DSI、GT9XX）与实时 heap/FPS
- Paint 页：全屏手写画板，四色画笔 + Clear

运行：NSH 下执行 `launcher`。
