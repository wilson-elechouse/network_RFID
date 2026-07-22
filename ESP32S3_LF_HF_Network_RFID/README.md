# ESP32S3 LF/HF Network RFID

中文配置说明：[`CONFIGURATION_CN.md`](CONFIGURATION_CN.md)

## Configuration portal

The firmware starts a lightweight hotspot configuration portal by default.

- Default AP IP: `10.10.10.10`
- Default AP SSID: `RFID-xxxxxx`, using the last 3 bytes of the ESP32-S3 MAC
- Default AP password: `rfid123456`
- Open `http://10.10.10.10/` after joining the AP.

The page can configure WiFi station credentials, TCP client/server mode, ELECHOUSE online TCP test session code, product interface mode, output format, LF/HF windows, duplicate suppression, HF speed, HF technologies, USB CDC events, feedback, button timing, and portal settings.

Serial portal commands:

```text
portal status
portal on
portal off
portal ssid <ssid> [password]
```

If TCP server mode uses the same port as the portal, the TCP server will not start. Keep the portal on port `80` and use a different TCP server port such as `9000`.

这个 Arduino 库面向当前 Network RFID Reader 主板：ESP32-S3 + LF 125 kHz + ST25R3916B HF I2C + WS2816C + 蜂鸣器 + 板载按键。

## 功能

- LF/HF 交替读卡。
- LF 支持 EM4100/TK4100、HID Prox H10301、Generic HIDProx 初步解码。
- HF 通过 `ST25R3916_ELECHOUSE` 的 RFAL discovery 读取 ISO14443A、ISO14443B、NFC-F、ISO15693 UID。
- 读到卡后输出卡号和卡类型。
- 产品两线接口支持 UART、Wiegand D0/D1、ABA Clock/Data 三种模式。
- TCP socket 支持 client 和 server 两种模式。
- ELECHOUSE 在线测试模式固定连接 `www.elechouse.com:9000`，只需填写网站 session code。
- 运行配置支持 USB CDC、产品 UART、TCP 命令和 WiFi 网页，并可保存到 ESP32 NVS。

## 默认接线

LF:

| Signal | ESP32-S3 GPIO |
|---|---:|
| `RFID_OUT` | GPIO1 |
| `RFID_DATA` | GPIO2 |
| `RFID_CARRIER` | GPIO3 |
| `RFID_PULL` | GPIO4 |
| `RFID_ADC` | GPIO13 |

HF I2C:

| Signal | ESP32-S3 GPIO |
|---|---:|
| `SCL` | GPIO5 |
| `SDA` | GPIO7 |
| `IRQ/INT` | GPIO9 |

Product interface:

| Mode | ESP32-S3 GPIO |
|---|---|
| UART | RX=GPIO44, TX=GPIO43 |
| Wiegand | D0=GPIO44, D1=GPIO43 |
| ABA | Clock=GPIO44, Data=GPIO43 |

## 依赖

需要安装同一套参考库：

- `ELECHOUSE ST25R3916 for ESP32`
- `ELECHOUSE NFC-RFAL for ESP32`

参考仓库：

- https://github.com/wilson-elechouse/ST25R3916
- https://github.com/wilson-elechouse/ProxFlex-Multi-LF

## 示例

打开 Arduino 示例：

```text
ESP32S3_LF_HF_Network_RFID/examples/ESP32S3_SPI_LF_HF_TCP
```

Arduino CLI 示例上传命令，端口按当前板子使用 COM3：

```powershell
arduino-cli compile --upload -p COM3 `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,PSRAM=disabled,PartitionScheme=default,CPUFreq=240" `
  .\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP
```

## 串口命令

串口波特率：`115200`。

```text
help
status
pins
wifi set <ssid> <password>
wifi clear
tcp client <host> <port>
tcp server <port>
elechouse on <session_code>
tcp off
tcp status
elechouse status
elechouse off
elechouse reconnect
elechouse clear
interface mode uart|wiegand|aba
interface wiegand bits 26|34|37|56
interface aba digits <0..32>
interface aba source raw|cn
hf init
hf off
hf status
format json
format line
window <lf_ms> <hf_ms>
dedupe <ms>
save
load
clear
reboot
test
```

常用配置示例：

```text
wifi set MyWifi MyPassword
tcp client 192.168.1.20 9000
format json
window 350 350
save
reboot
```

TCP server 模式：

```text
wifi set MyWifi MyPassword
tcp server 9000
save
reboot
```

如果上传后串口没有应用输出，并且 `esptool --before no-reset chip-id` 能直接连接，说明 ESP32-S3 还停在下载模式。请确认 BOOT/IO0 没有被按住或被外部拉低，然后按 RST/EN 让应用从 flash 启动。

## 输出格式

JSON，一行一个事件：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

文本行格式：

```text
LF,EM4100,06 00 7C A4 C4,123456
```

## 说明

- 交替读卡时，HF 窗口会关闭 LF 载波和 LF 数据中断；LF 窗口会关闭 HF field 并打开 LF 125 kHz 载波。
- 默认 `window 350 350`，即 LF 350 ms、HF 350 ms 轮询。
- 默认 `dedupe 1000`，同一张卡 1 秒内重复读到不会重复上传。
- 当前主板 GPIO 连接固定在 `src/NetworkRfidBoardProfile.h`，网页和命令行只配置运行参数。
