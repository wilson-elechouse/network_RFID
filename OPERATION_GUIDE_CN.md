# ELECHOUSE Network RFID Reader 操作指导与功能说明

本文适用于当前软件目录：

```text
C:\AI\ELECHOUSE_PRODUCTS\network_RFID_Reader_Mainboard\software\network_RFID_upload_worktree
```

当前固件目标硬件为 ESP32-S3 SuperMini + ST25R3916B I2C + LF 125 kHz + WS2816C LED + 蜂鸣器 + 板载按键。

## 1. 功能总览

本固件实现一套 LF/HF 网络 RFID 读卡器功能：

| 功能 | 当前支持 |
|---|---|
| LF 125 kHz 读卡 | EM4100/TK4100、HID Prox H10301、Generic HID Prox 初步解码、Indala 辅助解码 |
| HF 13.56 MHz 扫描 | ISO14443A、ISO14443B、NFC-F、ISO15693 UID 扫描 |
| HF 模拟卡 | NFC-A Type 4 Tag / NDEF，NFC-A Type 2 Tag / NDEF |
| NDEF 内容 | URL、Text、vCard、WiFi 配置 |
| 网络 | 2.4G WiFi、TCP client、TCP server、ELECHOUSE 在线测试 |
| 配置入口 | USB CDC 串口、产品 UART、TCP 命令、WiFi 网页 |
| 产品输出接口 | UART 文本输出、Wiegand D0/D1、ABA Clock/Data |
| 输出格式 | JSON 或文本行格式 |
| 反馈 | 空闲红灯，读卡成功绿灯 2 秒，蜂鸣器短鸣 |
| 按键 | 长按 5 秒进入 WiFi 配置，长按 10 秒恢复默认配置 |

P2P 当前已暂停，不作为产品功能开放。

## 2. 固定硬件连接

硬件连接是板级固定信息，不作为用户配置项。若后续硬件改版，应修改：

```text
ESP32S3_LF_HF_Network_RFID/src/NetworkRfidBoardProfile.h
```

当前固定连接如下：

| 功能 | GPIO |
|---|---|
| LF OUT | GPIO1 |
| LF DATA | GPIO2 |
| LF CARRIER | GPIO3 |
| LF PULL | GPIO4 |
| LF ADC | GPIO13 |
| HF I2C SCL | GPIO5 |
| HF I2C SDA | GPIO7 |
| HF IRQ | GPIO9 |
| WS2816C LED DIN | GPIO11 |
| 蜂鸣器 | GPIO12 |
| 板载按键 | GPIO10，低电平按下 |
| 产品接口 RX / Wiegand D0 / ABA Clock | GPIO44 |
| 产品接口 TX / Wiegand D1 / ABA Data | GPIO43 |

WiFi 连接路由器后，路由器中显示的设备名为：

```text
ELECHOUSE_RFID
```

## 3. 默认运行行为

出厂或清除配置后：

| 项目 | 默认值 |
|---|---|
| WiFi 配置热点 SSID | `ELECHOUSE_RFID` |
| 配置页地址 | `http://10.10.10.10/` |
| USB CDC 串口 | `115200 8N1` |
| HF 总线 | ST25R3916B I2C |
| HF I2C 速率 | `400000` |
| LF/HF 轮询窗口 | LF 350 ms，HF 350 ms |
| 重复卡号抑制 | 1000 ms |
| 输出格式 | JSON |
| 产品接口 | UART，115200 |
| Wiegand | 34 bit |
| ABA | 自动位数，使用 raw card id |
| 空闲反馈 | 红灯 |
| 读卡成功反馈 | 绿灯 2 秒，蜂鸣器短鸣 |

如果设备已经保存 WiFi SSID 和密码，上电后会优先连接路由器。连接失败时会启动配置热点。

## 4. WiFi 网页配置

### 4.1 进入配置页

可通过以下方式进入网页：

1. 设备没有可连接 WiFi 时，自动启动配置热点。
2. 长按板载按键 5 秒，听到一声蜂鸣后进入默认 WiFi 配置。
3. 通过命令执行 `portal on`。

操作步骤：

1. 手机或电脑连接热点 `ELECHOUSE_RFID`。
2. 浏览器打开 `http://10.10.10.10/`。
3. 修改需要的参数。
4. 点击 `Save`。
5. 固件保存配置到 NVS 后自动重启。

网页端会根据选项自动显示相关填写项。例如 TCP 选择 ELECHOUSE Test 时只显示 session code，HF 选择 Card emulation 时才显示模拟卡 UID 和 NDEF 内容。

### 4.2 网页可配置项

网页保留产品功能相关配置，复杂调试参数放在命令行中。

| 分类 | 配置项 |
|---|---|
| WiFi | 路由器 SSID、密码 |
| TCP Socket | Off、Client、Server、ELECHOUSE Test、主机、端口、TCP events、TCP commands |
| Product Interface | UART、Wiegand D0/D1、ABA Clock/Data，以及对应关键通信参数 |
| Reader | LF/HF 开机启用、USB 事件输出 |
| HF | Scan 或 Card emulation、扫描协议、模拟卡类型、UID、NDEF 类型与内容 |
| Hotspot | 配置热点 SSID、密码、网页端口 |

硬件 GPIO、HF SPI/I2C 引脚、LED/蜂鸣器/按键引脚不在网页中配置。

## 5. 命令行入口

同一套命令可通过以下入口使用：

| 入口 | 用途 |
|---|---|
| USB CDC 串口 | 生产配置、调试、查看日志 |
| 产品 UART | 外部主机读取事件，也可输入命令 |
| TCP client/server | 网络输出事件，也可输入命令 |

USB CDC 默认：

```text
COM3
115200 8N1
```

命令修改运行参数后，如需掉电保持，必须执行：

```text
save
```

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

## 6. WiFi 和 TCP 操作

### 6.1 连接路由器

```text
wifi set YOU B20150127
save
reboot
```

查看 WiFi 状态：

```text
wifi status
wifi scan
wifi reconnect
wifi clear
```

### 6.2 TCP server 模式

设备监听端口，外部上位机主动连接设备：

```text
tcp server 9000
tcp events on
tcp commands on
save
reboot
```

上位机连接设备 IP 的 `9000` 端口后，可接收读卡事件，也可按行发送命令。

注意：TCP server 端口不要和网页端口相同。网页默认端口是 `80`，TCP server 建议使用 `9000`。

### 6.3 TCP client 模式

设备主动连接服务器：

```text
tcp client 192.168.1.20 9000
tcp events on
tcp commands on
save
reboot
```

### 6.4 ELECHOUSE 在线测试

网站测试功能固定连接：

```text
www.elechouse.com:9000
```

用户只需要填写网页分配的 session code：

```text
elechouse on BEQAB6WH
save
reboot
```

读卡事件会显示在 ELECHOUSE 测试网页。若网页提示 `unknown_or_expired_code`，说明 session code 错误或已过期，需要从网页重新生成。

## 7. 读卡输出

### 7.1 JSON 输出

```text
format json
save
```

示例：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

### 7.2 文本行输出

```text
format line
save
```

示例：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

### 7.3 重复卡号抑制

```text
dedupe 1000
save
```

同一张卡在 1000 ms 内重复读到时不会重复输出。

## 8. LF/HF 扫描模式

### 8.1 LF

```text
lf init
lf status
lf freq 125000
lf off
```

调试命令：

```text
lf raw <count>
lf hid [ms]
lf indala [samples]
lf scan [start stop step ms]
```

### 8.2 HF 扫描

```text
hf init
hf mode scan
hf status
```

控制扫描协议：

```text
hf tech a on
hf tech b on
hf tech f on
hf tech v on
save
```

HF 扫描协议开关只影响 scan 模式，不影响模拟卡模式。

## 9. HF 模拟卡

当前已实现并通过实测：

| 模式 | 说明 |
|---|---|
| `nfc-a-t4t` | NFC Forum Type 4 Tag / NDEF，适合手机读取 |
| `nfc-a-t2t` | NFC Forum Type 2 Tag / NDEF，兼容 PN532 和常见 NFC 读卡器，手机可弹出 URL |

UID 支持 4 或 7 字节。Type 2 推荐使用 7 字节 UID。

### 9.1 Type 4 Tag URL 模拟

```text
hf mode card
hf card type nfc-a-t4t
hf card uid 02 00 00 01
hf card ndef url https://www.elechouse.com/
save
```

查看状态：

```text
hf card status
hf status
```

### 9.2 Type 2 Tag URL 模拟

```text
hf mode card
hf card type nfc-a-t2t
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
save
```

### 9.3 Text / vCard / WiFi NDEF

Text：

```text
hf mode card
hf card ndef text Hello ELECHOUSE
save
```

vCard 建议通过网页 textarea 配置。命令行也可以写入一行文本：

```text
hf mode card
hf card ndef vcard ELECHOUSE https://www.elechouse.com/
save
```

WiFi 配置：

```text
hf mode card
hf card ndef wifi MyWifi MyPassword
save
```

### 9.4 停止模拟卡

```text
hf mode scan
save
```

## 10. 产品接口

GPIO44/GPIO43 可在三种模式中切换：

| 模式 | GPIO44 | GPIO43 | 说明 |
|---|---|---|---|
| UART | RX | TX | 文本事件输出，也可输入命令 |
| Wiegand | D0 | D1 | 开漏风格低脉冲输出 |
| ABA | Clock | Data | Clock/Data 脉冲输出 |

### 10.1 UART

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

### 10.2 Wiegand D0/D1

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

支持位宽：

```text
26, 34, 37, 56
```

### 10.3 ABA Clock/Data

```text
interface mode aba
interface aba digits 10
interface aba source raw
interface pulse 80 1800
interface events on
save
```

说明：

- `interface aba digits 0` 表示自动长度。
- `interface aba source raw` 使用原始卡号转换为十进制。
- `interface aba source cn` 优先使用事件中的 `CN=` 字段。
- 切到 Wiegand 或 ABA 后，GPIO44/GPIO43 不再作为 UART 命令入口。此时可继续用 USB CDC、TCP 或网页配置设备。

## 11. LED、蜂鸣器和按键

默认反馈：

| 状态 | 行为 |
|---|---|
| 空闲 | 红灯 |
| 读卡成功 | 绿灯 2 秒，蜂鸣器短鸣一次 |
| 按键按到 5 秒 | 蜂鸣器一声，进入 WiFi 默认配置 |
| 按键超过 10 秒 | 蜂鸣器两声，恢复默认配置 |

命令：

```text
feedback status
feedback test
feedback buzzer 2700 80
feedback success_ms 2000
feedback idle 16384 0 0
feedback success 0 16384 0
save
```

按键：

```text
button status
button timing 5000 10000
save
```

恢复默认配置：

```text
clear
reboot
```

或长按板载按键 10 秒。

## 12. 常用生产配置流程

### 12.1 配置 WiFi + ELECHOUSE 在线测试

```text
wifi set YOU B20150127
elechouse on BEQAB6WH
format json
save
reboot
```

### 12.2 配置 WiFi + TCP server

```text
wifi set YOU B20150127
tcp server 9000
tcp events on
tcp commands on
format json
save
reboot
```

### 12.3 配置 UART 输出

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

### 12.4 配置 Wiegand 输出

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

### 12.5 配置 NFC URL 模拟卡

Type 4：

```text
hf mode card
hf card type nfc-a-t4t
hf card uid 02 00 00 01
hf card ndef url https://www.elechouse.com/
save
```

Type 2：

```text
hf mode card
hf card type nfc-a-t2t
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
save
```

## 13. 构建与烧录

仓库已包含固件库和本地依赖库：

```text
ESP32S3_LF_HF_Network_RFID/
libraries/NFC-RFAL/
libraries/ST25R3916_ELECHOUSE/
```

Arduino CLI 编译：

```powershell
arduino-cli compile `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,PSRAM=disabled,PartitionScheme=default,CPUFreq=240" `
  --libraries ".\ESP32S3_LF_HF_Network_RFID" `
  --libraries ".\libraries\NFC-RFAL" `
  --libraries ".\libraries\ST25R3916_ELECHOUSE" `
  ".\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP"
```

烧录到 COM3：

```powershell
arduino-cli upload -p COM3 `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,PSRAM=disabled,PartitionScheme=default,CPUFreq=240" `
  ".\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP"
```

## 14. 常见问题

### 14.1 网页保存 WiFi 后仍进入热点

检查：

```text
wifi status
```

常见原因：

- SSID 或密码错误。
- 路由器不是 2.4G WiFi。
- 信号弱。
- 保存后没有正常重启。
- 设备连接路由器失败后自动回到配置热点。

### 14.2 ELECHOUSE 网站显示 unknown_or_expired_code

session code 错误或过期，重新在网页生成 code 后执行：

```text
elechouse on NEWCODE
save
```

### 14.3 烧录后一直显示 waiting for download

ESP32-S3 仍停在下载模式。检查 BOOT/IO0 是否被按住或被外部拉低，然后按 RST/EN 让应用从 flash 启动。

### 14.4 Wiegand/ABA 没有输出

检查：

```text
interface status
interface events on
```

确认外部设备已接 GPIO44/GPIO43，并注意当前 ESP32-S3 GPIO 不是 5V 容忍接口，接 5V 控制器时需要外部电平转换。

### 14.5 手机读不到模拟卡

检查：

```text
hf card status
hf status
```

推荐先测试 Type 2 URL：

```text
hf mode card
hf card type nfc-a-t2t
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
```

手机 NFC 天线位置差异很大，测试时需要让手机 NFC 天线区域贴近 ST25R3916B 天线。
