# ELECHOUSE Network RFID Reader 命令配置书

适用产品：Network RFID Reader Mainboard V0.1H  
适用固件：ESP32-S3 + ST25R3916B HF 版本  
文档日期：2026-07-04

## 1. 文档目的

本文档定义 V0.1H 固件对外公开的标准命令集，用于 USB CDC、UART、TCP 以及 ELECHOUSE 在线测试场景下的参数配置、状态查询和功能测试。

本文档只保留用户手册中的标准命令。短别名、历史兼容命令和调试期临时命令不作为产品命令公开。

## 2. 命令输入通道

| 通道 | 用途 | 默认状态 | 说明 |
|---|---|---|---|
| USB CDC 串口 | 出厂配置、调试、烧录后验证 | 启用 | 默认 115200 bps，连接到 PC 后可直接输入命令 |
| UART 产品接口 | 对接主机、控制器或上位机 | 可配置 | 通过 `interface commands on` 允许输入命令 |
| TCP Client/Server | 网络配置与远程调试 | 可配置 | 通过 `tcp commands on` 允许 TCP 输入命令 |
| ELECHOUSE Broker | 在线测试网页 | 可配置 | 通过 `elechouse on <session_code>` 连接网站测试服务 |

## 3. 基本语法规则

- 每条命令以换行结束，建议发送 `\n` 或 `\r\n`。
- 命令关键字大小写不敏感，例如 `wifi status` 和 `WIFI STATUS` 等价。
- 参数内容保留原始大小写，例如 WiFi SSID、密码、session code、NDEF 文本。
- 命令和参数以空格分隔。SSID、session code、UID 等参数不建议包含空格。
- 部分命令会立即生效，但断电后是否保留取决于是否执行 `save`。
- 恢复已保存配置使用 `load`，清除已保存配置使用 `clear`。
- `reboot` 会立即重启设备，生产测试脚本中应放在最后执行。

## 4. 配置保存原则

| 类型 | 是否立即生效 | 是否需要 `save` | 说明 |
|---|---|---|---|
| WiFi 参数 | 是 | 是 | `wifi set` 会立即重新连接，断电保留需 `save` |
| TCP 参数 | 是 | 是 | `tcp client/server/off` 会立即重启 TCP socket |
| ELECHOUSE 测试 | 是 | 是 | 在线演示设备如需保留 session code，执行 `save` |
| 产品接口 | 是 | 是 | UART/Wiegand/ABA 切换立即生效 |
| HF/LF 启停 | 是 | 是 | 自动启动策略需 `save` 后长期保留 |
| 输出格式和去重 | 是 | 是 | `format`、`window`、`dedupe` 建议保存 |
| Portal SSID | 部分 | 是并建议重启 | AP 名称和密码通常保存重启后完整生效 |

## 5. 全局命令

| 命令 | 作用 | 示例 |
|---|---|---|
| `help` | 打印标准命令列表 | `help` |
| `status` | 打印整机状态摘要 | `status` |
| `pins` | 打印当前板级 GPIO 分配 | `pins` |
| `save` | 保存当前配置到 NVS | `save` |
| `load` | 从 NVS 重新加载配置 | `load` |
| `clear` | 清除已保存配置 | `clear` |
| `reboot` | 重启设备 | `reboot` |
| `test` | 输出一条测试读卡事件 | `test` |

### 5.1 输出格式

| 命令 | 作用 | 参数 |
|---|---|---|
| `format json` | 读卡事件输出为 JSON | 无 |
| `format line` | 读卡事件输出为单行文本 | 无 |

JSON 输出示例：

```text
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

Line 输出示例：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

### 5.2 轮询窗口和重复抑制

| 命令 | 作用 | 范围 |
|---|---|---|
| `window <lf_ms> <hf_ms>` | 设置 LF/HF 分时窗口 | 每项最小 20 ms |
| `dedupe <ms>` | 设置重复卡号抑制时间 | 0 表示不抑制 |
| `auto lf on|off` | 控制 LF 自动启动 | V0.1H 可作为预留 |
| `auto hf on|off` | 控制 HF 自动初始化 | V0.1H 推荐开启 |

推荐 V0.1H：

```text
auto hf on
format json
dedupe 800
save
```

## 6. WiFi 命令

| 命令 | 作用 | 说明 |
|---|---|---|
| `wifi status` | 查看 SSID、连接状态、IP、RSSI、最近断开原因 | 不显示明文密码，只显示密码长度 |
| `wifi scan [ssid]` | 扫描附近 AP，可按 SSID 过滤 | 扫描会短暂占用 WiFi 资源 |
| `wifi set <ssid> <password>` | 设置路由器 SSID 和密码 | 立即尝试连接 |
| `wifi reconnect` | 使用已保存参数重新连接 | SSID 为空时返回错误 |
| `wifi clear` | 清空 WiFi SSID 和密码 | 同时断开当前连接 |

首次配置示例：

```text
wifi scan
wifi set MyWifi MyPassword
wifi status
save
```

排查建议：

- `status=CONNECTED` 且有 `ip=` 表示已经连上路由器。
- `rssi` 低于约 `-75` dBm 时，网络表现可能不稳定。
- 更换路由器或密码后，使用 `wifi set` 后执行 `save`。

## 7. TCP 命令

| 命令 | 作用 | 说明 |
|---|---|---|
| `tcp status` | 查看 TCP 模式、目标、端口、事件输出和命令输入开关 | 推荐测试前先查看 |
| `tcp client <host> <port>` | 设备作为 TCP Client 主动连接服务器 | 适合对接云端或局域网服务 |
| `tcp server <port>` | 设备作为 TCP Server 监听端口 | 适合上位机主动连接设备 |
| `tcp off` | 关闭 TCP socket | 不影响 WiFi |
| `tcp events on|off` | 控制读卡事件是否发送到 TCP | 只影响事件输出 |
| `tcp commands on|off` | 控制 TCP 是否接受命令 | 关闭后 TCP 只输出事件 |

TCP Client 示例：

```text
wifi set MyWifi MyPassword
tcp client 192.168.1.100 9000
tcp events on
tcp commands on
save
```

TCP Server 示例：

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
save
```

## 8. ELECHOUSE 在线测试命令

| 命令 | 作用 | 说明 |
|---|---|---|
| `elechouse status` | 查看 Broker 地址、端口、session、连接状态 | 固定目标为 `www.elechouse.com:9000` |
| `elechouse on <session_code>` | 启用在线测试模式并写入 session code | 同时启用 TCP events 和 commands |
| `elechouse off` | 关闭 ELECHOUSE 在线测试模式 | TCP mode 置为 off |
| `elechouse reconnect` | 使用当前 session code 重新连接 Broker | session code 为空时返回错误 |
| `elechouse clear` | 清空已保存 session code | 适合生产测试后清理 |

在线测试流程：

```text
wifi status
elechouse on ABCD1234
elechouse status
test
save
```

生产测试后清理：

```text
elechouse clear
save
```

## 9. 配置 Portal 命令

| 命令 | 作用 | 说明 |
|---|---|---|
| `portal status` | 查看配置热点状态 | 显示 AP SSID、IP 和端口 |
| `portal on` | 启动配置热点 | 默认 IP 为 `10.10.10.10` |
| `portal off` | 关闭配置热点 | 如已有 WiFi 参数会尝试回到 STA |
| `portal ssid <ssid> [password]` | 设置配置热点名称和密码 | 建议 `save` 后重启 |

示例：

```text
portal ssid ELECHOUSE_RFID config1234
save
reboot
```

## 10. 产品接口命令

GPIO44/GPIO43 是产品输出接口，可在 UART、Wiegand D0/D1、ABA Clock/Data 三种模式之间切换。

| 命令 | 作用 | 说明 |
|---|---|---|
| `interface status` | 查看接口模式、使能、波特率、脉冲参数 | 推荐每次配置后确认 |
| `interface mode uart|wiegand|aba` | 切换产品接口模式 | 切换后立即重启接口 |
| `interface on` | 启用产品接口 | 使用当前模式 |
| `interface off` | 关闭产品接口输出 | 不影响 USB CDC |
| `interface events on|off` | 控制接口是否输出读卡事件 | UART/Wiegand/ABA 均适用 |
| `interface commands on|off` | 控制 UART 是否允许输入命令 | 仅 UART 模式有实际意义 |
| `interface baud <baud>` | 设置 UART 波特率并切到 UART 模式 | 范围 1200 到 3000000 |
| `interface pulse <us> <gap_us>` | 设置 Wiegand/ABA 脉冲宽度和间隔 | `us` 范围 20 到 1000，`gap_us` 范围 200 到 20000 |
| `interface wiegand bits <26|34|37|56>` | 设置 Wiegand 位数 | 默认 34 |
| `interface aba digits <0..32>` | 设置 ABA 输出位数 | 0 表示自动 |
| `interface aba source raw|cn` | 设置 ABA 数据来源 | `raw` 为原始 ID，`cn` 为卡号数值 |

UART 输出和命令输入：

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

Wiegand 34 位输出：

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

ABA Clock/Data 输出：

```text
interface mode aba
interface aba digits 10
interface aba source raw
interface pulse 80 1800
interface events on
save
```

## 11. HF RFID 命令

V0.1H 当前产品重点是 HF 读卡和基础 NFC-A 模拟卡能力。

| 命令 | 作用 | 说明 |
|---|---|---|
| `hf status` | 查看 HF 总线、角色、初始化、发现和卡模拟状态 | 推荐调试首选 |
| `hf init` | 初始化 HF 前端 | 通常由 `auto hf on` 自动完成 |
| `hf off` | 关闭 HF 前端 | 会释放 HF 资源 |
| `hf probe` | 读取 ST25R3916B 芯片识别信息 | 用于硬件调试 |
| `hf speed <hz>` | 设置 HF I2C/SPI 速度 | 当前主板 I2C 常用 100000 |
| `hf mode scan|card` | 切换 HF 扫描或模拟卡模式 | `scan` 为读卡，`card` 为模拟卡 |
| `hf tech a|b|f|v on|off` | 控制扫描协议 | A/B/F/V 分别对应 ISO14443A/B、NFC-F、ISO15693 |

HF 扫描推荐配置：

```text
hf init
hf mode scan
hf tech a on
hf tech b on
hf tech f on
hf tech v on
save
```

## 12. HF 模拟卡命令

| 命令 | 作用 | 说明 |
|---|---|---|
| `hf card status` | 查看模拟卡 UID、Tag 类型、NDEF 类型和活动状态 | 不改变模式 |
| `hf card uid <hex>` | 设置模拟卡 UID | 只接受 4 或 7 字节 HEX |
| `hf card type nfc-a-t4t` | 设置 NFC-A Type 4 Tag | 手机兼容性通常更好 |
| `hf card type nfc-a-t2t` | 设置 NFC-A Type 2 Tag | 适合部分 PN532/普通 NFC 读卡器 |
| `hf card ndef url <url>` | 设置 URL NDEF | 未填 URL 时默认 ELECHOUSE 官网 |
| `hf card ndef text <text>` | 设置 Text NDEF | 文本作为整行剩余内容 |
| `hf card ndef vcard <text>` | 设置 vCard NDEF | 推荐长内容通过网页配置 |
| `hf card ndef wifi <ssid> <password>` | 设置 WiFi NDEF | SSID 为第一个参数，密码为剩余内容 |

Type 4 URL 模拟示例：

```text
hf mode card
hf card type nfc-a-t4t
hf card uid 02 00 00 01
hf card ndef url https://www.elechouse.com/
hf card status
save
```

Type 2 URL 模拟示例：

```text
hf mode card
hf card type nfc-a-t2t
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
hf card status
save
```

回到读卡模式：

```text
hf mode scan
save
```

注意：`hf mode card` 是进入模拟卡模式的标准命令，`hf card ...` 只用于配置模拟卡参数和查看状态。

## 13. LF RFID 命令

V0.1H 产品以 HF 版本为主。固件中保留 LF 命令，供 LF 版本或双频版本复用。

| 命令 | 作用 | 说明 |
|---|---|---|
| `lf status` | 查看 LF 载波、频率、采集和当前 slot | 未装 LF 板时仅用于状态确认 |
| `lf init` | 启动 LF 采集和载波 | 会切到 LF slot |
| `lf off` | 关闭 LF 采集和载波 | HF 可重新成为活动 slot |
| `lf freq <hz>` | 设置 LF 载波频率 | 范围 100000 到 150000，默认 125000 |
| `lf scan [start stop step ms]` | 扫描 LF 频率响应 | 开发和调试用 |
| `lf raw <count>` | 输出原始采样 | 开发和调试用 |
| `lf hid [ms]` | 采集 HID Prox 数据 | LF 板相关 |
| `lf indala [samples]` | 采集 Indala 数据 | LF 板相关 |

## 14. 反馈与按键命令

### 14.1 Buzzer 和 LED 反馈

| 命令 | 作用 | 说明 |
|---|---|---|
| `feedback status` | 查看反馈状态和参数 | 包含 LED、buzzer、成功提示时间 |
| `feedback on` | 启用反馈 | 读卡成功有声光提示 |
| `feedback off` | 关闭反馈 | LED 和 buzzer 停止输出 |
| `feedback buzzer <hz> <ms>` | 设置蜂鸣器频率和时长 | 频率 100 到 20000 Hz，时长 0 到 5000 ms |
| `feedback success_ms <ms>` | 设置成功状态保持时间 | 最大 60000 ms |
| `feedback idle <r> <g> <b>` | 设置空闲 LED 颜色 | 每项 0 到 65535 |
| `feedback success <r> <g> <b>` | 设置成功 LED 颜色 | 每项 0 到 65535 |
| `feedback test` | 触发一次成功反馈 | 用于生产测试 |

### 14.2 板载按键

| 命令 | 作用 | 说明 |
|---|---|---|
| `button status` | 查看按键状态和长按时间 | GPIO 由板级 profile 固定 |
| `button on` | 启用配置按键 | 默认启用 |
| `button off` | 禁用配置按键 | 防止误触 |
| `button timing <wifi_ms> <reset_ms>` | 设置进入配置热点和恢复出厂的长按时间 | `reset_ms` 必须大于 `wifi_ms` |

默认推荐：

```text
button timing 5000 10000
save
```

## 15. 常用配置流程

### 15.1 出厂基础配置

```text
format json
auto hf on
hf init
hf mode scan
dedupe 800
interface mode uart
interface baud 115200
interface events on
interface commands on
feedback on
save
```

### 15.2 局域网 TCP 测试

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
save
```

### 15.3 ELECHOUSE 在线网页测试

```text
wifi status
elechouse on ABCD1234
elechouse status
test
save
```

### 15.4 生产测试后清理在线 session

```text
elechouse clear
tcp off
save
```

## 16. 错误和排查

| 现象 | 建议检查 |
|---|---|
| 命令返回 `ERR unknown command` | 确认使用的是本文档标准命令，不使用旧别名 |
| WiFi 无 IP | 运行 `wifi status` 查看状态和最近断开原因 |
| TCP 没有事件 | 确认 `tcp events on`，并确认 `tcp status` 中 mode 不为 off |
| TCP 不能输入命令 | 确认 `tcp commands on` |
| UART 没有事件 | 确认 `interface mode uart`、`interface events on` 和波特率 |
| UART 不能输入命令 | 确认 `interface commands on` |
| Wiegand/ABA 没有脉冲 | 确认 `interface mode wiegand` 或 `interface mode aba`，并确认 `interface events on` |
| HF 没有读卡事件 | 运行 `hf status`，确认 ready 和 discovery 状态 |
| 模拟卡不工作 | 确认 `hf mode card`，再运行 `hf card status` 查看 active/state |

## 17. 标准命令总表

```text
help
status
pins
wifi status
wifi scan [ssid]
wifi set <ssid> <password>
wifi reconnect
wifi clear
tcp status
tcp client <host> <port>
tcp server <port>
tcp off
tcp events on|off
tcp commands on|off
elechouse status
elechouse on <session_code>
elechouse off
elechouse reconnect
elechouse clear
portal status
portal on
portal off
portal ssid <ssid> [password]
interface status
interface mode uart|wiegand|aba
interface on
interface off
interface events on|off
interface commands on|off
interface baud <baud>
interface pulse <us> <gap_us>
interface wiegand bits <26|34|37|56>
interface aba digits <0..32>
interface aba source raw|cn
feedback status
feedback on
feedback off
feedback buzzer <hz> <ms>
feedback success_ms <ms>
feedback idle <r> <g> <b>
feedback success <r> <g> <b>
feedback test
button status
button on
button off
button timing <wifi_ms> <reset_ms>
lf status
lf init
lf off
lf freq <hz>
lf scan [start stop step ms]
lf raw <count>
lf hid [ms]
lf indala [samples]
hf status
hf init
hf off
hf probe
hf speed <hz>
hf mode scan|card
hf tech a|b|f|v on|off
hf card status
hf card uid <hex>
hf card type nfc-a-t4t|nfc-a-t2t
hf card ndef url <url>
hf card ndef text <text>
hf card ndef vcard <vcard_text>
hf card ndef wifi <ssid> <password>
format json
format line
window <lf_ms> <hf_ms>
dedupe <ms>
auto lf|hf on|off
save
load
clear
reboot
test
```

## 18. 不纳入用户手册的历史命令

为保持产品资料简洁，以下旧写法不作为标准命令公开：短别名、顶层 `uart`、顶层 `wiegand`、顶层 `aba`、`tcp echo`、`tcp elechouse`、`elechouse code`、`hf card on/off`、`hf card payload`、`wifi <ssid> <password>`。

用户手册、测试脚本和网页说明应统一使用第 17 节的标准命令。
