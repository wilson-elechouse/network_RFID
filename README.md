# ELECHOUSE Network RFID Reader

Firmware and local library dependencies for the ESP32-S3 LF/HF network RFID reader mainboard.

## Repository Layout

- `ESP32S3_LF_HF_Network_RFID/` - Product firmware Arduino library and example sketch.
- `libraries/NFC-RFAL/` - RFAL/NDEF support library used by the ST25R3916 driver.
- `libraries/ST25R3916_ELECHOUSE/` - ELECHOUSE ST25R3916/ST25R3916B driver with I2C/SPI support and current listen-mode work.
- `hardware/` - Board notes and EasyEDA `.tel` schematic exports.

## Current Board Target

- MCU board: ESP32-S3 SuperMini
- HF frontend: ST25R3916B over I2C
- Default HF pins: SCL GPIO5, SDA GPIO7, IRQ GPIO9
- Feedback: WS2816C data GPIO11, buzzer GPIO12
- Button: GPIO10 active-low
- Product interface pins: GPIO44/GPIO43 for UART, Wiegand D0/D1, or ABA Clock/Data

## Build

Install the ESP32 Arduino core, then compile with Arduino CLI:

```powershell
arduino-cli compile `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,PSRAM=disabled,PartitionScheme=default,CPUFreq=240" `
  --libraries ".\ESP32S3_LF_HF_Network_RFID" `
  --libraries ".\libraries\NFC-RFAL" `
  --libraries ".\libraries\ST25R3916_ELECHOUSE" `
  ".\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP"
```

Upload example:

```powershell
arduino-cli upload -p COM3 `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,FlashSize=4M,PSRAM=disabled,PartitionScheme=default,CPUFreq=240" `
  ".\ESP32S3_LF_HF_Network_RFID\examples\ESP32S3_SPI_LF_HF_TCP"
```

## Notes

The firmware includes LF/HF scan, Wi-Fi configuration portal, TCP command support, ELECHOUSE broker test mode, UART/Wiegand/ABA product output, feedback LED/buzzer, and initial NFC-A Type 4 Tag card emulation work.

P2P is intentionally disabled in the current firmware.
