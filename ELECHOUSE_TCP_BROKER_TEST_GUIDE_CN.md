# ELECHOUSE RFID TCP Broker 在线测试说明

本文档说明如何使用 ELECHOUSE 在线测试页面验证 Network RFID Reader Mainboard `V0.1H` 的 WiFi、TCP、命令回传和读卡数据上传功能。

在线测试页面：

```text
https://www.elechouse.com/rfid-tcp-broker/
```

当前 `V0.1H` 产品定义为主控 + HF，因此本文测试重点是 HF 读卡事件上传。固件中可能包含 LF 字段或 LF/HF 统一格式，但 `V0.1H` 对外测试应以 HF 为主。

## 1. 测试目标

通过 ELECHOUSE RFID TCP Broker 测试以下能力：

| 测试项 | 目标 |
|---|---|
| WiFi 连接 | 设备能连接 2.4G 路由器 |
| TCP 连接 | 设备能连接 `www.elechouse.com:9000` |
| Broker 绑定 | 设备能用 session code 和网页测试会话绑定 |
| 读卡上传 | HF 读卡事件能实时显示在网页 |
| 命令回传 | 网页能向设备发送命令并收到回复 |
| 断线重连 | 网络断开后设备能按配置重连 |

## 2. 测试环境

| 项目 | 要求 |
|---|---|
| 设备 | ELECHOUSE Network RFID Reader Mainboard `V0.1H` |
| 供电 | 5V |
| WiFi | 2.4G 路由器，可访问公网 |
| 电脑/手机 | 能打开测试网页 |
| 测试卡 | HF RFID/NFC 卡片 |
| 调试接口 | 推荐 USB CDC，`115200 8N1` |

网络要求：

- 设备所在 WiFi 网络可以访问 `www.elechouse.com`。
- 网络允许设备连接 TCP `9000` 端口。
- 如果现场网络限制出站 TCP 端口，需要先放行。

## 3. 工作原理

ELECHOUSE Test 模式使用 plain TCP，不需要设备支持 HTTP、HTTPS 或 WebSocket。

流程：

1. 用户打开在线测试页面。
2. 页面生成一个临时 `session_code`。
3. 设备连接 `www.elechouse.com:9000`。
4. 设备发送：

```text
HELLO <session_code> <device_id>
```

5. Broker 根据 `session_code` 把设备 TCP 连接绑定到网页会话。
6. 设备上传读卡事件。
7. 网页显示读卡数据，也可以通过同一连接向设备发送命令。

`session_code` 不是固定值。刷新网页、关闭网页、过期或重新生成后，旧 code 可能失效。

## 4. 设备端固定连接参数

在 ELECHOUSE Test 模式下，以下参数由固件固定：

| 项目 | 固定值 |
|---|---|
| TCP Host | `www.elechouse.com` |
| TCP Port | `9000` |
| Protocol | Plain TCP |
| Encoding | ASCII / UTF-8 text lines |
| Line ending | `\n` 或 `\r\n` |

用户只需要配置：

- 路由器 WiFi SSID
- 路由器 WiFi Password
- 网页生成的 `session_code`

## 5. 测试前检查

通过 USB CDC 打开串口，默认参数：

```text
115200 8N1
```

检查设备状态：

```text
status
pins
hf status
wifi status
```

如果设备还没有连接 WiFi，可先进入配置热点：

```text
portal on
```

然后连接热点 `ELECHOUSE_RFID`，打开：

```text
http://10.10.10.10/
```

## 6. 网页测试流程

### 6.1 打开测试页面

在电脑或手机浏览器中打开：

```text
https://www.elechouse.com/rfid-tcp-broker/
```

页面会生成一个用于本次测试的 `session_code`。每次测试应使用页面当前显示的最新 code。

### 6.2 配置设备 WiFi

通过 USB CDC 输入：

```text
wifi set <ssid> <password>
save
reboot
```

示例：

```text
wifi set MyWifi MyPassword
save
reboot
```

重启后检查：

```text
wifi status
```

期望看到：

```text
status=CONNECTED
ip=...
```

实际输出格式以当前固件为准。

### 6.3 启用 ELECHOUSE Test

把网页上的 `session_code` 填入命令：

```text
elechouse on <session_code>
```

示例：

```text
elechouse on EKQWQ4XZ
```

如果需要重启后自动连接：

```text
save
reboot
```

### 6.4 检查连接状态

输入：

```text
elechouse status
tcp status
```

期望状态：

```text
mode=elechouse
connected=yes
brokerOk=yes
```

串口日志可能出现：

```text
ELECHOUSE broker OK <session_code>
```

这表示设备已经和网页会话绑定成功。

### 6.5 上传测试事件

先用固件内置测试事件验证链路：

```text
test
```

网页应显示一条测试读卡事件。

如果测试事件正常，再使用真实 HF 卡靠近天线。网页应显示类似：

```json
{"type":"card","band":"HF","card_type":"ISO14443A","uid":"1C 2C FB 10","ms":1002132}
```

字段说明：

| 字段 | 说明 |
|---|---|
| `type` | 固定为 `card` |
| `band` | 当前 `V0.1H` 主要为 `HF` |
| `card_type` | 卡类型，例如 `ISO14443A` |
| `uid` | 卡片 UID 或读到的 ID |
| `ms` | 设备启动后的毫秒时间戳 |

### 6.6 网页发送命令

网页测试页面如果提供命令输入框，可以发送：

```text
status
```

或：

```text
wifi status
tcp status
test
```

设备收到命令后，应通过 TCP 返回命令结果。网页应能显示返回内容。

注意：需要启用 TCP commands。

```text
tcp commands on
save
```

## 7. 使用 WiFi 网页配置 ELECHOUSE Test

除了 USB CDC 命令，也可以通过设备 WiFi 配置页完成。

步骤：

1. 连接设备热点 `ELECHOUSE_RFID`。
2. 打开 `http://10.10.10.10/`。
3. 在 WiFi 区域填写路由器 SSID 和 Password。
4. 在 TCP Socket 区域选择 `ELECHOUSE Test`。
5. 填入测试页面生成的 `session_code`。
6. 打开 `TCP events`。
7. 如需网页发送命令，打开 `TCP commands`。
8. 点击 `Save`。
9. 设备保存并重启。
10. 设备连接路由器后自动连接 `www.elechouse.com:9000`。

## 8. Heartbeat 和重连

连接成功后，设备会周期性发送 heartbeat。

典型行为：

| 行为 | 说明 |
|---|---|
| 设备发送 `PING` | 用于保持连接 |
| Broker 回复 `PONG` | 表示连接仍有效 |
| 连接断开 | 设备按重连间隔重新连接 |
| session code 失效 | Broker 返回错误，设备继续按旧 code 重试 |

如果网页刷新或 code 过期，需要重新在设备中配置最新 `session_code`：

```text
elechouse on <new_session_code>
save
```

## 9. 生产测试流程建议

推荐生产测试按以下步骤执行：

1. 打开在线测试页面并获取 `session_code`。
2. 通过 USB CDC 配置 WiFi：

```text
wifi set <ssid> <password>
save
reboot
```

3. 启用在线测试：

```text
elechouse on <session_code>
```

4. 等待连接成功：

```text
elechouse status
tcp status
```

5. 执行测试事件：

```text
test
```

6. 读取一张 HF 卡，确认网页显示 HF 读卡事件。
7. 从网页发送 `status`，确认设备有响应。
8. 测试完成后清除 session code：

```text
elechouse clear
save
```

如果该设备要保留在线测试模式用于演示，则不要清除 code，但需要注意 session code 可能会过期。

## 10. 最小验证命令

```text
wifi status
elechouse on <session_code>
elechouse status
tcp status
test
```

期望结果：

```text
wifi ... status=CONNECTED ... ip=...
ELECHOUSE broker OK <session_code>
mode=elechouse connected=yes brokerOk=yes
```

网页应显示测试事件或真实 HF 读卡事件。

## 11. 常见错误和处理

### `ERR unknown_or_expired_code`

含义：

- 网页 session code 已过期。
- 网页刷新后生成了新 code。
- 设备仍在使用旧 code。
- 输入的 code 和网页不一致。

处理：

```text
elechouse on <new_session_code>
save
```

### `code_already_has_device`

含义：

- 同一个 session code 已经绑定了另一台设备。

处理：

- 断开另一台设备。
- 或在网页重新生成新的 code。

### `connected=yes` 但 `brokerOk=no`

含义：

- TCP 已连接，但 HELLO 握手没有成功。

检查：

```text
elechouse status
tcp status
```

确认：

- session code 不为空。
- code 是网页当前最新 code。
- Broker 没有返回 `ERR ...`。

### 网页没有读卡数据

检查：

```text
tcp status
elechouse status
hf status
```

确认：

- `mode=elechouse`
- `connected=yes`
- `brokerOk=yes`
- `tcp events on`
- HF 已初始化
- 使用的是 HF 卡，并靠近 HF 天线

也可以先执行：

```text
test
```

如果 `test` 能显示，但真实卡不能显示，问题通常在 HF 读卡、天线距离或卡片类型。

### WiFi 已配置但设备连不上 Broker

检查：

- 路由器是否能访问公网。
- 网络是否允许出站 TCP `9000`。
- DNS 是否能解析 `www.elechouse.com`。
- WiFi 信号是否稳定。

### 同一张卡不重复显示

设备有重复卡号抑制。默认同一张卡在一定时间内不会重复上传。

调整：

```text
dedupe 1000
save
```

`1000` 表示 1000 ms 内同一卡号不重复输出。

## 12. 测试记录模板

| 项目 | 结果 |
|---|---|
| 测试日期 |  |
| 固件版本 |  |
| 产品版本 | `V0.1H` |
| 设备 MAC |  |
| WiFi SSID |  |
| 设备 IP |  |
| session code |  |
| Broker 连接 | Pass / Fail |
| 网页命令回传 | Pass / Fail |
| `test` 事件显示 | Pass / Fail |
| HF 读卡显示 | Pass / Fail |
| 备注 |  |
