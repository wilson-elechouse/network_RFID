# ELECHOUSE Network RFID Reader WiFi 网页配置选项说明

本文档说明 ELECHOUSE Network RFID Reader Mainboard `V0.1H` 的 WiFi 网页配置入口和页面中的主要配置项。

当前 `V0.1H` 产品定义为主控 + HF 版本。本文档以 HF 网络读卡器为主，LF、双频、BLE、PoE、12V 等能力作为后续版本规划，不作为当前 WiFi 页面必须开放的产品规格。

## 1. 适用范围

| 项目 | 说明 |
|---|---|
| 产品 | ELECHOUSE Network RFID Reader Mainboard |
| 当前版本 | `V0.1H` |
| 主控 | ESP32-S3 |
| HF 模块 | ST25R3916B |
| 网络 | 2.4G WiFi |
| 配置方式 | WiFi 热点网页、USB CDC 命令、产品 UART 命令、TCP 命令 |

## 2. WiFi 配置入口

设备可以通过内置 WiFi 配置热点进入网页配置。

默认配置入口：

| 项目 | 默认值 |
|---|---|
| 配置热点 SSID | `ELECHOUSE_RFID` |
| 配置页地址 | `http://10.10.10.10/` |
| 默认网页端口 | `80` |

进入配置页的方式：

1. 设备没有可连接的路由器 WiFi 时，会自动启动配置热点。
2. 如果硬件包含配置按键，可长按按键进入配置热点。
3. 通过命令执行 `portal on` 后进入配置热点。

基本操作流程：

1. 给设备接入 5V 电源。
2. 手机或电脑连接设备热点 `ELECHOUSE_RFID`。
3. 浏览器打开 `http://10.10.10.10/`。
4. 修改配置项。
5. 点击 `Save`。
6. 设备保存配置到 NVS 后自动重启。

## 3. WiFi 连接配置

WiFi 区域用于配置设备要连接的路由器。

| 配置项 | 说明 |
|---|---|
| `SSID` | 设备要连接的 2.4G WiFi 名称 |
| `Password` | 路由器 WiFi 密码 |

注意事项：

- ESP32-S3 当前使用 2.4G WiFi。
- 如果路由器只开启 5G WiFi，设备无法连接。
- SSID 和密码保存后，设备重启会优先连接路由器。
- 如果连接失败，设备会保留或重新进入配置热点，方便重新配置。
- 设备连接路由器后，路由器中显示的 hostname 建议为 `ELECHOUSE_RFID`。

命令行等价配置：

```text
wifi set <ssid> <password>
save
reboot
```

查看 WiFi 状态：

```text
wifi status
```

扫描附近 WiFi：

```text
wifi scan
```

重新连接：

```text
wifi reconnect
```

清除已保存的 WiFi 配置：

```text
wifi clear
```

## 4. TCP Socket 配置

TCP Socket 区域用于配置网络数据输出和远程命令。

| 配置项 | 可选值/说明 |
|---|---|
| `Mode` | `Off`、`Client`、`Server`、`ELECHOUSE Test` |
| `Client host` | TCP client 模式下连接的服务器 IP 或域名 |
| `Client port` | TCP client 模式下连接的服务器端口 |
| `Server port` | TCP server 模式下设备监听的端口 |
| `ELECHOUSE code` | ELECHOUSE 在线测试网页生成的 session code |
| `TCP events` | 是否通过 TCP 输出读卡事件 |
| `TCP commands` | 是否允许 TCP 输入命令 |

### TCP Off

关闭 TCP 网络输出和 TCP 命令入口。设备仍可通过 USB CDC 或产品 UART 输出/配置。

### TCP Client

设备主动连接外部服务器。

适用场景：

- 服务器有固定 IP 或域名。
- 设备部署后主动把读卡数据上传到服务器。

命令行等价配置：

```text
tcp client 192.168.1.20 9000
tcp events on
tcp commands on
save
reboot
```

### TCP Server

设备监听指定端口，外部上位机主动连接设备。

适用场景：

- 设备和电脑在同一个局域网。
- 上位机主动连接设备 IP 和端口读取数据。

命令行等价配置：

```text
tcp server 9000
tcp events on
tcp commands on
save
reboot
```

注意：TCP server 端口不能和网页端口冲突。网页默认端口是 `80`，TCP server 建议使用 `9000`。

### ELECHOUSE Test

设备固定连接 ELECHOUSE 在线测试服务器。

| 项目 | 固定值 |
|---|---|
| Host | `www.elechouse.com` |
| Port | `9000` |
| Protocol | Plain TCP |
| 用户需要填写 | 在线测试网页生成的 session code |

命令行等价配置：

```text
elechouse on <session_code>
save
reboot
```

## 5. Product Interface 配置

Product Interface 区域用于配置产品两线接口的输出模式。

`V0.1H` 当前建议主推 UART。Wiegand 和 ABA 在固件中已有相关配置方向，但是否作为当前版本正式对外接口，需要结合硬件电平保护和产品化测试确认。

| 配置项 | 说明 |
|---|---|
| `Mode` | `UART`、`Wiegand D0/D1`、`ABA Clock/Data` |
| `UART baud` | UART 波特率，默认建议 `115200` |
| `Interface enabled` | 是否启用产品接口 |
| `Interface events` | 是否通过产品接口输出读卡事件 |
| `UART commands` | UART 模式下是否允许输入命令 |
| `Wiegand bits` | Wiegand 位宽，常见为 `26`、`34`、`37`、`56` |
| `Pulse us` | Wiegand/ABA 脉冲宽度 |
| `Pulse gap us` | Wiegand/ABA 位间隔 |
| `ABA digits` | ABA 输出数字位数，`0` 表示自动 |
| `ABA source` | `Raw` 使用原始卡号，`CN` 优先使用 CN 字段 |

当前产品建议：

- `V0.1H` 对外集成优先使用 UART。
- 如果连接 5V 门禁控制器，不能直接把 ESP32-S3 GPIO 当作 5V 容忍接口使用。
- Wiegand/ABA 正式开放前，应确认开漏输出、电平转换、ESD 和浪涌保护。

## 6. Reader 配置

Reader 区域用于配置读卡事件输出格式和轮询参数。

| 配置项 | 说明 |
|---|---|
| `Output` | `JSON` 或 `Line` |
| `Dedupe ms` | 重复卡号抑制时间 |
| `HF enabled` | 开机是否启用 HF |
| `LF enabled` | 软件中可能存在该项，`V0.1H` 产品资料不作为 LF 能力宣称 |
| `HF window ms` | HF 轮询时间窗口 |
| `LF window ms` | LF 轮询时间窗口；`V0.1H` 可隐藏或保留为后续版本 |
| `USB CDC events` | 是否通过 USB CDC 输出读卡事件 |

推荐默认值：

| 项目 | 建议值 |
|---|---|
| 输出格式 | `JSON` |
| 重复卡号抑制 | `1000 ms` |
| HF 开机启用 | `On` |
| USB CDC 事件输出 | 开发/测试阶段建议开启 |

JSON 输出示例：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

Line 输出示例：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

## 7. HF 配置

HF 区域用于配置 HF 工作角色和扫描协议。

| 配置项 | 说明 |
|---|---|
| `Role` | `Scan` 或 `Card emulation` |
| `I2C Hz` | HF I2C 速率 |
| `Card UID` | Card emulation 模式下的模拟卡 UID |
| `Simulated card` | 模拟卡类型 |
| `NDEF type` | URL、Text、vCard、WiFi |
| `ISO14443A/B` | Scan 模式下是否扫描对应技术 |
| `NFC-F` | Scan 模式下是否扫描 NFC-F |
| `ISO15693` | Scan 模式下是否扫描 ISO15693 |

`V0.1H` 当前建议默认使用：

```text
hf mode scan
hf tech a on
hf tech b on
hf tech f on
hf tech v on
save
```

模拟 URL 标签示例：

```text
hf mode card
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
save
```

注意：HF 模拟卡的实际兼容性和手机弹窗行为受手机型号、读卡器、天线位置、Tag 类型和 NDEF 内容影响，产品资料中应以测试报告为准。

## 8. Feedback and Button 配置

Feedback and Button 区域用于配置蜂鸣器、LED 和按键行为。

| 配置项 | 说明 |
|---|---|
| `Buzzer Hz` | 蜂鸣器频率 |
| `Buzzer ms` | 蜂鸣持续时间 |
| `Success ms` | 读卡成功状态保持时间 |
| `Idle RGB16` | 空闲 LED 颜色 |
| `Success RGB16` | 读卡成功 LED 颜色 |
| `WiFi press ms` | 长按进入 WiFi 配置热点时间 |
| `Reset press ms` | 长按恢复默认配置时间 |
| `Feedback enabled` | 是否启用 LED/蜂鸣反馈 |
| `Button enabled` | 是否启用板载按键 |

当前 `V0.1H` 已实现 Buzzer 反馈。LED 和按键是否作为正式产品能力，应以当前硬件装配情况为准。

推荐反馈行为：

| 状态 | 行为 |
|---|---|
| 空闲 | LED 红色，若硬件已装配 |
| 读卡成功 | LED 绿色保持一段时间，Buzzer 短鸣 |
| 长按进入 WiFi 配置 | Buzzer 一声 |
| 长按恢复默认配置 | Buzzer 两声 |

## 9. Hotspot 配置

Hotspot 区域用于配置设备自己的配置热点。

| 配置项 | 说明 |
|---|---|
| `AP SSID` | 配置热点名称 |
| `AP password` | 配置热点密码，空密码表示开放热点 |
| `Portal port` | 网页配置端口，默认 `80` |

命令行等价配置：

```text
portal status
portal on
portal off
portal ssid <ssid> [password]
```

建议：

- 生产默认 SSID 使用统一可识别名称，例如 `ELECHOUSE_RFID`。
- 如果现场有多台设备，可在页面或串口状态中查看 MAC/IP 区分设备。
- TCP server 端口不要和 portal port 相同。

## 10. 保存和恢复默认

网页点击 `Save` 后，设备会保存配置到 NVS 并重启。

命令行保存：

```text
save
reboot
```

恢复默认配置：

```text
clear
reboot
```

如果硬件包含按键，也可以通过长按恢复默认配置。

注意：

- `save` 保存的是运行参数，不保存板级 GPIO。
- `clear` 会清除保存的运行配置。
- 硬件引脚和板级能力应通过固件 profile 管理。

## 11. 常见问题

### 网页打不开

检查：

- 手机或电脑是否连接到设备热点。
- 浏览器地址是否为 `http://10.10.10.10/`。
- 设备是否已经连接路由器并关闭配置热点。
- 是否通过命令执行了 `portal off`。

### 保存 WiFi 后仍回到热点

常见原因：

- SSID 或密码错误。
- 路由器不是 2.4G WiFi。
- 信号弱。
- 路由器限制新设备接入。

建议通过 USB CDC 执行：

```text
wifi status
wifi scan
```

### TCP server 无法连接

检查：

- 设备是否成功连接路由器。
- 上位机和设备是否在同一局域网。
- TCP server 端口是否和网页端口冲突。
- 防火墙是否拦截连接。

### ELECHOUSE Test 不显示数据

检查：

```text
elechouse status
tcp status
```

应确认：

- session code 是网页最新生成的。
- TCP 已连接。
- broker handshake 成功。
- TCP events 已开启。
