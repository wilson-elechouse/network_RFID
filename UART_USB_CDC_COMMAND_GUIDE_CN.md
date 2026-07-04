# ELECHOUSE Network RFID Reader UART、USB CDC 输出与命令配置说明

本文档说明 ELECHOUSE Network RFID Reader Mainboard `V0.1H` 的 USB CDC、产品 UART 输出和命令配置方式。

用户消息中提到的 `USB DCD` 在本文中按 `USB CDC` 理解。ESP32-S3 当前使用原生 USB CDC/JTAG 口进行烧录、调试和命令交互。

## 1. 适用范围

| 项目 | 说明 |
|---|---|
| 产品 | ELECHOUSE Network RFID Reader Mainboard |
| 当前版本 | `V0.1H` |
| 主控 | ESP32-S3 |
| HF 模块 | ST25R3916B |
| USB 调试 | USB CDC |
| 产品输出 | UART，后续可评估 Wiegand/ABA |

## 2. USB CDC 与产品 UART 的区别

| 通道 | 用途 | 默认参数 | 典型使用者 |
|---|---|---|---|
| USB CDC | 烧录、调试、生产配置、查看日志 | `115200 8N1` | 开发人员、生产测试人员 |
| 产品 UART | 对外输出读卡事件，也可输入命令 | 默认 `115200 8N1` | 外部 MCU、网关、上位机 |
| TCP | 网络输出读卡事件，也可输入命令 | 由 WiFi/TCP 配置决定 | 服务器、测试网页、局域网上位机 |

三种命令入口复用同一套命令格式。在哪个入口发送命令，回复通常从哪个入口返回。读卡事件可按配置分别输出到 USB CDC、产品 UART 和 TCP。

## 3. USB CDC 默认参数

| 项目 | 默认值 |
|---|---|
| 波特率 | `115200` |
| 数据位 | `8` |
| 校验 | `None` |
| 停止位 | `1` |
| 推荐用途 | 调试、生产配置、日志查看 |

ESP32-S3 Arduino 编译参数建议开启：

```text
CDCOnBoot=cdc
USBMode=hwcdc
```

否则程序中的 `Serial` 可能不会输出到当前 USB CDC 端口。

## 4. 产品 UART 默认参数

当前产品 UART 使用 ESP32-S3 的固定产品接口引脚。

| 信号 | GPIO | 说明 |
|---|---:|---|
| UART RX | GPIO44 | 外部主机 TX 接入 |
| UART TX | GPIO43 | 外部主机 RX 接入 |
| GND | GND | 必须共地 |

默认 UART 配置：

| 项目 | 默认值 |
|---|---|
| 波特率 | `115200` |
| 数据格式 | `8N1` |
| 输出格式 | JSON 或 Line |
| 命令输入 | 可配置开启/关闭 |

注意：

- ESP32-S3 GPIO 是 3.3V 逻辑。
- 不要把 GPIO43/GPIO44 直接接到 5V UART。
- 外部控制器为 5V 逻辑时，应增加电平转换或保护。

## 5. 命令输入规则

命令以文本行输入。

| 项目 | 说明 |
|---|---|
| 编码 | ASCII / UTF-8 文本 |
| 行结束 | `\n` 或 `\r\n` |
| 命令参数 | 使用空格分隔 |
| 保存配置 | 修改后执行 `save` |
| 重启生效 | 部分配置建议执行 `reboot` |

常用通用命令：

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

说明：

| 命令 | 功能 |
|---|---|
| `help` | 打印命令帮助 |
| `status` | 查看当前设备状态 |
| `pins` | 查看固定板级引脚 |
| `save` | 保存运行配置到 NVS |
| `load` | 从 NVS 读取配置 |
| `clear` | 清除保存配置 |
| `reboot` | 重启设备 |
| `test` | 输出一条测试读卡事件，并触发反馈 |

## 6. 读卡事件输出格式

设备支持 JSON 和 Line 两种文本输出格式。

切换 JSON：

```text
format json
save
```

JSON 示例：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

切换 Line：

```text
format line
save
```

Line 示例：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

字段说明：

| 字段 | 说明 |
|---|---|
| `band` | 当前产品主要为 `HF` |
| `type` | 卡片/协议类型，例如 `ISO14443A` |
| `id` | 读取到的卡号或 UID |
| `ms` | 设备启动后的毫秒时间戳 |

## 7. USB CDC 输出配置

USB CDC 常用于查看调试日志和读卡事件。

Reader 配置中通常使用 `USB CDC events` 控制 USB CDC 是否输出读卡事件。

相关命令：

```text
format json
format line
test
status
```

测试 USB CDC 输出：

```text
test
```

预期输出类似：

```json
{"band":"HF","type":"TEST","id":"12 34 56 78","ms":123456}
```

实际字段以当前固件实现为准。

## 8. 产品 UART 配置

启用 UART 模式：

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

配置项说明：

| 命令 | 说明 |
|---|---|
| `interface mode uart` | 将 GPIO44/GPIO43 设置为 UART 模式 |
| `interface baud 115200` | 设置 UART 波特率 |
| `interface events on` | 允许 UART 输出读卡事件 |
| `interface commands on` | 允许 UART 输入命令 |

关闭 UART 命令输入但保留事件输出：

```text
interface commands off
save
```

关闭 UART 事件输出：

```text
interface events off
save
```

查看接口状态：

```text
interface status
```

## 9. WiFi 和 TCP 常用命令

配置路由器 WiFi：

```text
wifi set <ssid> <password>
save
reboot
```

查看 WiFi：

```text
wifi status
wifi scan
```

配置 TCP client：

```text
tcp client 192.168.1.20 9000
tcp events on
tcp commands on
save
reboot
```

配置 TCP server：

```text
tcp server 9000
tcp events on
tcp commands on
save
reboot
```

关闭 TCP：

```text
tcp off
save
```

查看 TCP：

```text
tcp status
```

## 10. HF 常用命令

初始化 HF：

```text
hf init
```

进入扫描模式：

```text
hf mode scan
save
```

控制扫描协议：

```text
hf tech a on
hf tech b on
hf tech f on
hf tech v on
save
```

查看 HF 状态：

```text
hf status
```

配置 HF 模拟 URL 标签：

```text
hf mode card
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
save
```

停止模拟卡并回到扫描：

```text
hf mode scan
save
```

## 11. Buzzer 和反馈命令

查看反馈状态：

```text
feedback status
```

测试反馈：

```text
feedback test
```

设置蜂鸣器：

```text
feedback buzzer 2700 80
save
```

设置读卡成功保持时间：

```text
feedback success_ms 2000
save
```

如果硬件已经装配 WS2816C LED，可配置空闲和成功颜色：

```text
feedback idle 16384 0 0
feedback success 0 16384 0
save
```

## 12. 生产配置示例

### USB CDC 配置 WiFi + UART 输出

```text
wifi set MyWifi MyPassword
interface mode uart
interface baud 115200
interface events on
interface commands on
format json
save
reboot
```

### USB CDC 配置 WiFi + TCP Server

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
format json
save
reboot
```

### 只测试输出链路

```text
format json
test
```

应能在已开启事件输出的 USB CDC、UART 或 TCP 通道中看到测试事件。

## 13. USB CDC 调试建议

如果使用 Arduino CLI monitor，建议参数：

```powershell
arduino-cli monitor -p COM10 -c baudrate=115200,dtr=on,rts=off
```

如果上传成功但 USB CDC 没有应用日志：

1. 检查编译参数是否开启 `CDCOnBoot=cdc`。
2. 按一下板子的 Reset。
3. 确认板子没有停在 ROM download 模式。
4. 重新确认 Windows 设备管理器中的 COM 口。

正常启动日志通常包含：

```text
ESP32S3 LF/HF RFID boot
RFID begin
Portal AP ssid=ELECHOUSE_RFID ip=10.10.10.10 port=80 url=http://10.10.10.10/
HF init OK
```

## 14. 常见问题

### UART 没有输出

检查：

```text
interface status
interface status
```

确认：

- 当前接口模式是 UART。
- UART 事件输出已开启。
- 外部设备波特率一致。
- RX/TX 是否交叉连接。
- 外部设备和主板是否共地。

### UART 能输出但不能输入命令

检查：

```text
interface status
```

确认 `interface commands` 或 `tcp commands` 已开启。

### USB CDC 没有日志

检查：

- ESP32-S3 是否进入应用而不是下载模式。
- Arduino FQBN 是否开启 `CDCOnBoot=cdc`。
- 串口监视器 DTR/RTS 参数是否合适。
- 是否打开了正确 COM 口。

### 输出重复太快或重复太少

重复卡号抑制由 `dedupe` 控制：

```text
dedupe 1000
save
```

`1000` 表示同一张卡 1000 ms 内不会重复输出。
