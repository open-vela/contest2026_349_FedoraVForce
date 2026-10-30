# miio（米家局域网协议）

公共库，不是独立应用，由 `app/mihome` `select LVX_MIIO`。只管协议：hello
握手、AES-128-CBC 加解密（mbedtls）、发一条 JSON-RPC、收回包里 `id` 对得上
的 JSON。不认识任何具体设备，调什么方法、怎么解析见 `app/mihome`。

- `miio_open()` 读 `/tmp/miio.conf`：第一行 `<ip>`，第二行 32 位十六进制
  token，第三行型号（可选）。token 只在内存里，不打印
- `miio_call()` 每条命令先 hello 一次取 device id 和 stamp，收包超时 2 秒，
  失败重试 1 次

协议说明见 `docs/miio/README.md` 第四节。
