# ESP32S3 LF/HF Network RFID 配置说明

本文档适用于当前主板固件：ESP32-S3 + LF 125 kHz + ST25R3916B I2C + WS2816C + 蜂鸣器 + 板载按键。

## 1. 配置原则

硬件连接是板级固定信息，不作为用户配置项暴露。

用户可以配置的是运行参数，例如 WiFi、TCP、产品接口模式、UART 波特率、输出格式、轮询窗口、HF 工作角色、反馈时长、按键长按时间等。

板级固定连接集中在源码：

```text
src/NetworkRfidBoardProfile.h
```

如果后续更换硬件版本，应修改这个 profile 后重新编译固件，而不是让用户在网页或命令行里填写 GPIO。

## 2. 固定硬件连接

| 功能 | 固定配置 |
|---|---|
| USB 控制台 | USB CDC，`115200 8N1` |
| 路由器显示名称 | STA hostname：`ELECHOUSE_RFID` |
| 产品接口两线 | UART：`RX=GPIO44`、`TX=GPIO43`；Wiegand：`D0=GPIO44`、`D1=GPIO43`；ABA：`Clock=GPIO44`、`Data=GPIO43` |
| WS2816C LED | `GPIO11` |
| 蜂鸣器 | `GPIO12` |
| 配置按键 | `GPIO10`，低电平按下 |
| HF 接口 | ST25R3916B I2C |

LF 默认引脚：

| 信号 | GPIO |
|---|---:|
| LF OUT | 1 |
| LF DATA | 2 |
| LF CARRIER | 3 |
| LF PULL | 4 |
| LF ADC | 13 |

HF I2C 默认引脚：

| 信号 | GPIO |
|---|---:|
| SCL | 5 |
| SDA | 7 |
| IRQ | 9 |

按键默认逻辑：

| 操作 | 行为 |
|---|---|
| 长按 5 秒 | 进入默认 WiFi 配置热点 |
| 长按 10 秒 | 清除保存配置并重启 |

默认反馈：

| 状态 | 行为 |
|---|---|
| 空闲 | WS2816C 红灯 |
| 读卡成功 | WS2816C 绿灯 2 秒，蜂鸣器短鸣一次 |

## 3. 配置入口

固件支持四种配置入口：

| 入口 | 用途 |
|---|---|
| USB CDC 串口 | 调试、生产配置、查看日志 |
| 产品接口 UART | 对外产品 UART，可输出读卡事件，也可输入命令 |
| TCP Socket | WiFi 网络接口，可输出读卡事件，也可输入命令 |
| WiFi 网页 | 浏览器配置运行参数 |

三种命令入口使用同一套命令格式。在哪个入口发送命令，回复就从哪个入口返回。读卡事件按独立开关输出到 USB、产品接口、TCP。

产品接口模式可选 `UART`、`Wiegand D0/D1`、`ABA Clock/Data`。切到 Wiegand 或 ABA 后，GPIO44/GPIO43 用于脉冲输出，不再作为 UART 命令入口；USB CDC 和 TCP 命令入口仍可继续配置设备。

## 4. WiFi 网页配置

### 4.1 进入网页

有两种方式进入配置网页：

1. 设备没有可连接 WiFi 时，默认会启动配置热点。
2. 长按板载按键 5 秒，强制进入默认配置热点。

操作步骤：

1. 手机或电脑连接热点 `ELECHOUSE_RFID`。
2. 浏览器打开 `http://10.10.10.10/`。
3. 修改参数后点击 `Save`。
4. 固件保存到 NVS 后自动重启。

### 4.2 网页可配置项

`WiFi`：

| 项目 | 说明 |
|---|---|
| SSID | 设备要连接的路由器名称 |
| Password | 路由器密码 |

`TCP Socket`：

| 项目 | 说明 |
|---|---|
| Mode | `Off`、`Client`、`Server`、`ELECHOUSE Test` |
| ELECHOUSE code | 网站测试页分配的 session code；仅 `ELECHOUSE Test` 模式需要填写 |
| Client host | TCP client 模式下连接的服务器 IP/域名 |
| Client port | TCP client 模式下连接的端口 |
| Server port | TCP server 模式下监听的端口 |
| TCP events | 是否通过 TCP 输出读卡事件 |
| TCP commands | 是否允许 TCP 输入控制命令 |

`ELECHOUSE Test` 模式固定连接 `www.elechouse.com:9000`，设备连接成功后自动发送 `HELLO <session_code> <device_id>`，握手成功后读卡数据会实时显示在网站测试页。

注意：TCP server 端口不能和网页端口相同。网页默认端口是 `80`，TCP server 建议用 `9000`。

`Product Interface`：

| 项目 | 说明 |
|---|---|
| Mode | `UART`、`Wiegand D0/D1`、`ABA Clock/Data` |
| UART baud | UART 模式波特率，默认 `115200` |
| Wiegand bits | Wiegand 输出位宽，支持 `26`、`34`、`37`、`56` |
| Pulse us | Wiegand/ABA 拉低脉冲宽度 |
| Pulse gap us | Wiegand/ABA 位间隔 |
| ABA digits | ABA 输出数字位数；`0` 表示自动长度 |
| ABA source | `Raw` 使用卡号原始值，`CN` 优先使用 `CN=` 卡号字段 |
| Interface enabled | 是否启用产品接口 |
| Interface events | 是否通过产品接口输出读卡事件 |
| UART commands | UART 模式下是否允许产品串口输入控制命令 |

产品接口 GPIO 在板级 profile 中固定，网页只显示，不允许修改。

`Reader`：

| 项目 | 说明 |
|---|---|
| Output | `JSON` 或 `Line` 输出格式 |
| LF window ms | LF 轮询时间窗口 |
| HF window ms | HF 轮询时间窗口 |
| LF carrier Hz | LF 载波频率，默认 `125000` |
| Dedupe ms | 重复卡号抑制时间 |
| TCP reconnect ms | TCP/WiFi 重连间隔 |
| LF enabled | 开机自动启用 LF |
| HF enabled | 开机自动初始化 HF |
| USB CDC events | 是否通过 USB CDC 输出读卡事件 |

`Feedback and Button`：

| 项目 | 说明 |
|---|---|
| Buzzer Hz | 蜂鸣频率 |
| Buzzer ms | 蜂鸣时长 |
| Success ms | 读卡成功绿灯保持时间 |
| Idle RGB16 | 空闲颜色，16 位 R/G/B |
| Success RGB16 | 成功颜色，16 位 R/G/B |
| WiFi press ms | 长按进入 WiFi 配置热点时间 |
| Reset press ms | 长按恢复默认配置时间 |
| Feedback enabled | 是否启用 LED/蜂鸣反馈 |
| Button enabled | 是否启用板载按键 |

LED、蜂鸣器、按键 GPIO 以及按键有效电平在板级 profile 中固定，网页只显示，不允许修改。

`HF`：

| 项目 | 说明 |
|---|---|
| Role | `Scan`、`Card emulation` |
| SPI Hz | HF SPI 频率参数，当前 I2C 板型通常不用 |
| I2C Hz | HF I2C 频率 |
| Card UID | Card emulation 模式下的模拟卡 UID，4/7/10 字节 |
| Simulated card | Card emulation 固定为 `NFC-A Type 4 Tag / NDEF` |
| NDEF type | Card emulation 模式下的 NDEF 类型：URL、Text、vCard、WiFi |
| ISO14443A/B、NFC-F、ISO15693 | Scan 模式下的 HF 轮询技术开关 |

当前主板 HF 总线固定为 I2C。I2C/SPI 连接不作为网页配置项。

`Hotspot`：

| 项目 | 说明 |
|---|---|
| AP SSID | 配置热点名称 |
| AP password | 配置热点密码；空为开放热点 |
| Portal port | 网页配置端口，默认 `80` |

## 5. 命令端配置

### 5.1 命令入口

USB CDC：

```text
COM3
115200 8N1
```

产品接口 UART 模式：

```text
RX=GPIO44
TX=GPIO43
默认 115200 8N1
```

产品接口 Wiegand/ABA 模式：

```text
Wiegand D0=GPIO44, D1=GPIO43
ABA Clock=GPIO44, Data=GPIO43
```

TCP server 示例：

```text
tcp server 9000
tcp commands on
tcp events on
save
reboot
```

之后上位机连接设备 IP 的 `9000` 端口，按行发送命令。

TCP client 示例：

```text
tcp client 192.168.1.20 9000
tcp commands on
tcp events on
save
reboot
```

设备会主动连接 `192.168.1.20:9000`。同一个 TCP 连接既可收读卡事件，也可发送命令。

### 5.2 通用命令

```text
help
status
pins
save
load
clear
reboot
test
```

| 命令 | 说明 |
|---|---|
| `help` | 打印命令帮助 |
| `status` | 打印当前状态 |
| `pins` | 打印固定板级引脚 |
| `save` | 保存运行配置到 NVS |
| `load` | 从 NVS 读取运行配置 |
| `clear` | 清除保存配置 |
| `reboot` | 重启设备 |
| `test` | 输出一条测试读卡事件，并触发蜂鸣/绿灯 |

多数运行参数修改后会立即生效；需要掉电保持时必须执行 `save`。

### 5.3 WiFi 和网页命令

```text
wifi set <ssid> <password>
wifi status
wifi scan [ssid]
wifi reconnect
wifi clear
portal status
portal on
portal off
portal ssid <ssid> [password]
```

示例：

```text
wifi set MyWifi MyPassword
save
reboot
```

### 5.4 TCP 命令

```text
tcp status
tcp client <host> <port>
tcp server <port>
elechouse on <session_code>
tcp off
tcp events on|off
tcp commands on|off

elechouse status
elechouse on <session_code>
elechouse off
elechouse reconnect
elechouse clear
```

| 命令 | 说明 |
|---|---|
| `tcp client <host> <port>` | 设备主动连接服务器 |
| `tcp server <port>` | 设备监听指定端口 |
| `elechouse on <session_code>` | 启用 ELECHOUSE 在线测试模式，固定连接 `www.elechouse.com:9000` |
| `elechouse clear` | 清空已保存的网站 session code |
| `tcp events on|off` | 控制读卡事件是否输出到 TCP |
| `tcp commands on|off` | 控制 TCP 是否接受命令 |
| `tcp off` | 关闭 TCP socket |

WiFi 调试命令：

| 命令 | 说明 |
|---|---|
| `wifi status` | 显示保存的 SSID、密码长度、连接状态、IP、RSSI、最近一次断开原因 |
| `wifi scan [ssid]` | 扫描附近 2.4G WiFi；带 SSID 时只显示匹配项 |
| `wifi reconnect` | 按已保存的 SSID/密码重新连接 |

### 5.5 产品接口命令

```text
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
```

UART 模式示例：

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

Wiegand 34 位输出示例：

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

ABA Clock/Data 输出示例：

```text
interface mode aba
interface aba digits 10
interface aba source raw
interface pulse 80 1800
interface events on
save
```

说明：

| 项目 | 行为 |
|---|---|
| UART | GPIO44/GPIO43 为 `RX/TX`，可输出事件，也可输入命令 |
| Wiegand | GPIO44/GPIO43 为 `D0/D1`，开漏低脉冲输出读卡事件 |
| ABA | GPIO44/GPIO43 为 `Clock/Data`，Track-II 风格帧，开漏低脉冲输出读卡事件 |

Wiegand 当前使用通用格式：首位偶校验、末位奇校验，中间数据位取卡号原始值低位。ABA 默认把卡号原始值转十进制输出；`interface aba source cn` 会优先使用事件中的 `CN=` 字段。

产品接口 GPIO 固定在板级 profile 中，不提供用户配置命令。

### 5.6 输出格式和轮询参数

```text
format json
format line
window <lf_ms> <hf_ms>
dedupe <ms>
auto lf on|off
auto hf on|off
```

示例：

```text
format json
window 350 350
dedupe 1000
save
```

### 5.7 LF 命令

```text
lf init
lf off
lf status
lf freq <hz>
lf raw <count>
lf hid [ms]
lf indala [samples]
lf scan [start stop step ms]
```

常用：

```text
lf status
lf freq 125000
```

### 5.8 HF 命令

```text
hf probe
hf init
hf off
hf status
hf speed <hz>
hf mode scan|card
hf tech a|b|f|v on|off
hf card status
hf card uid <hex>
hf card ndef url <url>
hf card ndef text <text>
hf card ndef vcard <vcard_text>
hf card ndef wifi <ssid> <password>
```

当前主板 ST25R3916B 总线固定为 I2C：

```text
hf speed 100000
hf init
save
```

模拟卡入口：

```text
hf mode card
hf card uid 02 00 00 01
hf card ndef url https://www.elechouse.com/
```

说明：Card emulation 当前目标固定为 NFC-A Type 4 Tag / NDEF。NDEF 配置参数已经保留并可保存；当前本地 ST25R3916 RFAL 驱动的 listen mode 仍需要继续补底层支持，补完后手机/读卡器才能实际读出该 NDEF 内容。

### 5.9 蜂鸣器和 WS2816C 反馈命令

```text
feedback status
feedback on
feedback off
feedback buzzer <hz> <ms>
feedback success_ms <ms>
feedback idle <r16> <g16> <b16>
feedback success <r16> <g16> <b16>
feedback test
```

默认空闲红灯、读卡成功绿灯 2 秒：

```text
feedback buzzer 2700 80
feedback idle 16384 0 0
feedback success 0 16384 0
feedback success_ms 2000
save
```

LED 和蜂鸣器 GPIO 固定在板级 profile 中，不提供用户配置命令。

### 5.10 板载按键命令

```text
button status
button on
button off
button timing <wifi_ms> <reset_ms>
```

默认：

```text
button timing 5000 10000
save
```

按键 GPIO 和有效电平固定在板级 profile 中，不提供用户配置命令。

## 6. 读卡事件输出格式

JSON 格式：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

Line 格式：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

Wiegand/ABA 输出：

```text
interface mode wiegand
interface mode aba
```

切到 Wiegand 或 ABA 后，产品接口两线不输出 JSON/Line 文本，而是把同一条读卡事件编码成对应脉冲。USB CDC 和 TCP 仍按 `format json|line` 输出文本事件。

切换命令：

```text
format json
format line
```

## 7. 常用生产配置流程

### 7.1 通过 USB 配置 WiFi + TCP server

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
format json
save
reboot
```

### 7.2 配置产品 UART 输出和命令

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

### 7.3 配置 ELECHOUSE 在线测试

1. 打开 `https://www.elechouse.com/rfid-tcp-broker/`。
2. 网页生成 session code。
3. 在设备 USB CDC、产品 UART 或 TCP 命令端输入：

```text
elechouse on A7K3Q9
save
reboot
```

也可以用 TCP 命令入口：

```text
elechouse on A7K3Q9
save
```

设备会固定连接 `www.elechouse.com:9000`，握手成功后网页会显示读卡数据。网页回发的命令会按固件现有命令格式解析。

### 7.4 配置 Wiegand D0/D1 输出

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

### 7.5 配置 ABA Clock/Data 输出

```text
interface mode aba
interface aba digits 10
interface aba source raw
interface pulse 80 1800
interface events on
save
```

### 7.6 恢复默认配置

方法一：按键长按 10 秒。

方法二：命令清除：

```text
clear
reboot
```

### 7.7 进入网页配置

方法一：长按按键 5 秒。

方法二：命令打开：

```text
portal on
```

然后连接热点 `ELECHOUSE_RFID`，访问 `http://10.10.10.10/`。

## 8. 注意事项

- `save` 只保存运行配置，不保存板级 GPIO 连接。
- `clear` 或按键 10 秒会清空保存的运行配置，重启后恢复固件默认值。
- TCP server 端口不要和网页端口冲突。
- UART/TCP 的 `commands off` 会禁止该通道输入命令，但不影响事件输出。
- UART/TCP 的 `events off` 只关闭该通道事件输出，不影响命令回复。
- Wiegand/ABA 模式下，GPIO44/GPIO43 只做脉冲输出；需要用 USB CDC、TCP 或网页继续配置。
- 当前 ESP32-S3 GPIO 不是 5V 容忍输入输出；接 5V 控制器时需要外部电平转换或开漏接口保护。
- 改 HF speed 后建议执行 `hf init` 或重启。
- 如需修改硬件连接，请改 `src/NetworkRfidBoardProfile.h` 并重新编译烧录。
