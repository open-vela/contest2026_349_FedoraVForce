# mihome（米家设备控制）

aircon、画板联动和 NSH 命令 `miio` 共用的后端接口：读状态
`mihome_get()`、开关 `mihome_power()`、设温度 `mihome_temp()`。

## 型号表

`mihome.c` 里的 `g_models`，一行一个型号：读哪些属性、怎么开关和设温度。
老款 miio 设备按属性名 `get_prop`，每项设置一个方法（`set_power`
`set_tar_temp`）；MIoT 设备按 siid/piid `get_properties` /
`set_properties`。两种都能写进表里，目前只有空调伴侣
`lumi.acpartner.mcn02` 一行，MIoT 的路径还没对真设备跑过。

`/tmp/miio.conf` 第三行写型号，不写按第一行（空调伴侣）。米家里开了速冷时，
空调伴侣开机会把设定温度改成 20°C，`mihome_power()` 不改回去，界面如实显示，
要别的温度用 `mihome_temp()`。

## 后端

| 后端 | 什么时候用 |
| --- | --- |
| 真设备 | 有 `/tmp/miio.conf`，经 `app/miio` 走以太网 |
| mock | 没有 `/tmp/miio.conf`：板上内存里的模拟空调伴侣，不走网络 |

`miio mode real` / `miio mode mock` 强制切换，`miio mode auto` 恢复按文件
选，`miio mode` 显示当前。模式对所有任务生效（flat build 共用一份），
已打开的句柄下一次调用就换后端。

mock 回包和真机一样是 JSON，解析、型号表都和真机走同一套代码。行为照
10-29 实测：关机先 96 W、15 秒后 24 W 待机；开机设定温度变成 20°C（速冷），
前 20 秒压缩机保护 30 W，之后 `120 + (30 − 设定温度) × 30` W，
都有 ±3 W 抖动；每次调用随机延迟 50–150 ms。

用到 mock 的地方必须标出来：NSH 每行带 `[mock]`，aircon 和画板结果卡显示
「演示模式 · 模拟设备」（`ui_demo_badge()`）。

## NSH

```
miio get                      读状态
miio on | off                 开关（会真的开关空调）
miio temp <度>                设温度（16–30）
miio mode [auto|real|mock]    选后端
```

每条命令最后都读一次状态打出来。
