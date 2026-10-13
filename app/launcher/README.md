# launcher（LVGL 桌面）

LVGL 桌面，开机自动运行。

- 锁屏：上滑或点击底部按钮解锁，点 Lock 磁贴可以重新锁屏
- 桌面：磁贴风格，顶部大磁贴轮流显示运行时间和剩余内存，点击进入系统信息
- 应用：每个应用一个磁贴，点击启动，退出后回到桌面

桌面显示哪些应用由 `CONFIG_LVX_LAUNCHER_APPS` 配置，由板级 defconfig 指定
例如 `CONFIG_LVX_LAUNCHER_APPS="paint bench"`。没有编进镜像的应用不会显示。

启动应用前 launcher 会先释放 LVGL，等应用退出后再重新初始化。同一时间只能
运行一个 LVGL 应用，所以应用要从桌面打开。新加的应用退出前需要调用
`lv_nuttx_deinit` 和 `lv_deinit`。
