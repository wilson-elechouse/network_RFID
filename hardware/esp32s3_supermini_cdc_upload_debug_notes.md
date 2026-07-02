# ESP32-S3 SuperMini CDC 串口上传与调试经验总结

本文记录 NoLogo ESP32-S3 SuperMini 在本项目中的 Arduino CLI 上传、CDC 串口调试和常见问题处理经验。

## 1. COM 口类型判断

本次测试的板子在 Windows 下枚举为 `COM10`：

```text
USB 串行设备 (COM10)
USB\VID_303A&PID_1001&MI_00
USB JTAG/serial debug unit
```

判断结论：

- `VID_303A&PID_1001` 是 Espressif ESP32-S3 原生 USB Serial/JTAG。
- 这是 ESP32-S3 原生 USB CDC/JTAG 口，不是外置 USB 转串口。
- 不是 CH340：`VID_1A86&PID_7523`
- 不是 CP210x：`VID_10C4&PID_EA60`
- 不是 FTDI：`VID_0403`

可以用下面命令确认：

```powershell
Get-CimInstance Win32_SerialPort |
  Where-Object { $_.DeviceID -eq 'COM10' } |
  Select-Object DeviceID,Name,Description,Manufacturer,PNPDeviceID |
  Format-List

Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match '\(COM10\)' -or $_.PNPDeviceID -match 'VID_303A&PID_1001' } |
  Select-Object Name,Manufacturer,PNPClass,PNPDeviceID |
  Format-List
```

Arduino CLI 也会识别为 USB Serial：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' board list
```

## 2. 推荐 Arduino FQBN

本项目在 `COM10` 上推荐使用：

```text
esp32:esp32:esp32s3:
  CDCOnBoot=cdc,
  USBMode=hwcdc,
  UploadMode=default,
  FlashSize=4M,
  FlashMode=dio,
  PSRAM=disabled,
  PartitionScheme=default,
  CPUFreq=240
```

写成一行：

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,FlashMode=dio,PSRAM=disabled,PartitionScheme=default,CPUFreq=240
```

关键点：

- `CDCOnBoot=cdc`：让程序里的 `Serial` 输出到原生 USB CDC。
- `USBMode=hwcdc`：使用 ESP32-S3 原生 USB Serial/JTAG。
- `FlashMode=dio`：本板实测比默认 QIO 更稳。之前用默认 QIO 时出现过上传成功但运行串口无输出、应用疑似未启动的问题。
- `PSRAM=disabled`：当前固件不依赖 PSRAM，禁用更保守。

不要用 `CDCOnBoot=default`，否则 `Serial` 可能映射到 UART0，COM10 看不到应用日志。

## 3. 上传命令

本项目主程序上传命令：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' compile --upload -p COM10 `
  --fqbn 'esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,FlashMode=dio,PSRAM=disabled,PartitionScheme=default,CPUFreq=240' `
  --libraries '.\ESP32S3_LF_HF_Network_RFID' `
  --libraries "$env:TEMP\rfid_refs\ST25R3916\ST25R3916_ELECHOUSE" `
  --libraries "$env:TEMP\rfid_refs\ST25R3916\NFC-RFAL" `
  '.\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP'
```

本项目编译可能比较久，等待 10 到 20 分钟是可以接受的。实测一次上传后资源占用大致为：

```text
Sketch uses about 1.05 MB, around 80% of 1.31 MB app partition.
Global variables use about 50 KB, leaving about 277 KB RAM.
```

## 4. 上传后复位问题

Arduino CLI 上传结束通常显示：

```text
Hard resetting via RTS pin...
```

但这块 ESP32-S3 SuperMini 实测存在一个现象：

- 上传成功后，自动 RTS 复位不一定让应用正常跑起来。
- 表现为：COM10 没有应用日志，热点也不出现。
- 按一下板子的 Reset 后通常可以进入应用。

无需手按时，可以用 esptool 的 watchdog reset：

```powershell
& "$env:USERPROFILE\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" `
  --chip esp32s3 -p COM10 -b 115200 `
  --before default-reset --after watchdog-reset run
```

实测 `watchdog-reset` 能让应用启动，并看到类似日志：

```text
ESP-ROM:esp32s3-20210327
rst:0x15 (USB_UART_CHIP_RESET),boot:0x28 (SPI_FAST_FLASH_BOOT)
mode:DIO, clock div:1
ESP32S3 LF/HF RFID boot
RFID begin
Portal AP ssid=ELECHOUSE_RFID ip=10.10.10.10 port=80 url=http://10.10.10.10/
HF init OK
```

注意：

- `--after soft-reset` 对 ESP32-S3 不适用。
- 错误的 DTR/RTS 组合可能让芯片进入 ROM 下载模式。

进入下载模式时会看到：

```text
ESP-ROM:esp32s3-20210327
boot:0x21 (DOWNLOAD(USB/UART0))
waiting for download
```

此时重新上传，或复位到应用模式即可。

## 5. 串口监视参数

推荐监视命令：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' monitor -p COM10 -c baudrate=115200,dtr=on,rts=off
```

原因：

- `baudrate=115200`：本项目串口波特率。
- `dtr=on`：让 CDC 连接状态有效。
- `rts=off`：避免 RTS 把板子保持在异常复位/下载状态。

Arduino CLI monitor 默认会列出这些参数：

```text
baudrate
dtr
rts
bits
parity
stop_bits
```

可以查看：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' monitor --describe -p COM10
```

## 6. PowerShell 直接读写 COM10

调试时也可以用 PowerShell 直接发命令：

```powershell
$port = New-Object System.IO.Ports.SerialPort('COM10',115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$port.ReadTimeout = 100
$port.WriteTimeout = 1000
$port.NewLine = "`n"
$port.DtrEnable = $true
$port.RtsEnable = $false
$port.Open()

$port.Write("status`n")
Start-Sleep -Seconds 2
$port.Write("pins`n")
Start-Sleep -Seconds 2

$s = $port.ReadExisting()
$port.Close()
$s
```

不要随意切换 DTR/RTS。实测某些组合会进入下载模式。

## 7. 固件中 Serial 初始化建议

示例代码建议这样写：

```cpp
void setup() {
  Serial.begin(115200);
  const unsigned long start = millis();
  while (!Serial && (millis() - start) < 2000UL) {
    delay(10);
  }

  Serial.println("ESP32S3 LF/HF RFID boot");
}
```

说明：

- `while (!Serial)` 最多等 2 秒即可，不要无限等待。
- 现场脱机运行时，即使没有串口监视器，也应该继续启动 WiFi/LF/HF。

## 8. 本项目正常串口输出参考

启动成功后应该能看到：

```text
ESP32S3 LF/HF RFID boot

RFID begin
RFID config loaded
RFID pins ready
Portal AP ssid=ELECHOUSE_RFID ip=10.10.10.10 port=80 url=http://10.10.10.10/
LF init starting
LF started
HF init starting
HF init OK
```

`status` 输出示例：

```text
slot=LF hfBus=i2c hfReady=yes lfCarrier=on lfCapture=on pulses=0 dropped=0
wifi=disconnected tcp=off clientConnected=no serverClient=no
portal=10.10.10.10 ssid=ELECHOUSE_RFID
```

HF 读卡输出示例：

```json
{"band":"HF","type":"ISO14443A","id":"3B 58 C0 38","ms":701}
```

LF 读卡输出示例：

```json
{"band":"LF","type":"EM4100","id":"06 00 7C A4 C4","ms":1686}
```

## 9. 常见问题排查

| 现象 | 可能原因 | 处理 |
|---|---|---|
| 上传成功但 COM10 无输出 | 自动 RTS 复位没有进入应用 | 按 Reset，或执行 `--after watchdog-reset run` |
| 上传成功但 `Serial` 没日志 | `CDCOnBoot` 没开，`Serial` 跑到 UART0 | FQBN 使用 `CDCOnBoot=cdc` |
| 串口显示 `DOWNLOAD(USB/UART0)` | 进入 ROM 下载模式 | 重新上传，或复位回应用模式 |
| 默认 QIO 下无输出/热点不出现 | Flash mode 不兼容或启动不稳 | 改用 `FlashMode=dio` |
| `arduino-cli monitor` 打开后板子异常 | DTR/RTS 状态影响复位 | 使用 `dtr=on,rts=off` |
| Windows 显示 `USB 串行设备` 但 Manufacturer 是 Microsoft | 正常，原生 CDC 常见显示方式 | 用 VID/PID 判断，`303A:1001` 即 ESP32-S3 USB Serial/JTAG |
| 上传端口变化或消失 | 芯片进入不同 USB 模式/下载模式 | `arduino-cli board list` 重新确认端口 |

## 10. 建议固定的调试流程

1. 确认端口：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' board list
```

2. 上传主程序，使用 `CDCOnBoot=cdc`、`USBMode=hwcdc`、`FlashMode=dio`。

3. 上传后如果无输出，按板子 Reset，或执行：

```powershell
& "$env:USERPROFILE\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" `
  --chip esp32s3 -p COM10 -b 115200 `
  --before default-reset --after watchdog-reset run
```

4. 打开监视器：

```powershell
& 'D:\GreenSoftware\Arduino_cli\arduino-cli.exe' monitor -p COM10 -c baudrate=115200,dtr=on,rts=off
```

5. 发送命令验证：

```text
status
pins
hf probe
test
```

6. 如果进入 hotspot 模式，连接：

```text
SSID: ELECHOUSE_RFID
URL:  http://10.10.10.10/
```

## 11. 结论

ESP32-S3 SuperMini 的 COM10 是原生 USB Serial/JTAG CDC 口。上传和调试都可以走同一个 USB 口，但必须注意：

- 编译参数要开启 `CDCOnBoot=cdc`。
- 本板建议用 `FlashMode=dio`。
- 上传后的自动 RTS 复位不一定可靠，必要时按 Reset 或用 watchdog reset。
- 串口监视建议固定 `baudrate=115200,dtr=on,rts=off`。

按上述流程，本项目已经验证 COM10 能稳定输出启动日志、配置命令响应、HF/LF 读卡 JSON。
