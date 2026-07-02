# NoLogo ESP32S3 SuperMini 连接 ProxFlex LF + HF 接线方案

本文总结 NoLogo ESP32S3 SuperMini 同时连接以下模块时的硬件接线：

- ProxFlex Multi LF Board，125 kHz / 134.2 kHz 低频 RFID 前端
- ST25R3916B HF RFID/NFC 模块
- 单线 RGB LED
- 蜂鸣器

引脚分配优先使用 SuperMini 上比较好焊接的 GPIO1 到 GPIO13。

## 总体原则

- ESP32-S3 GPIO 只能接 3.3 V 逻辑。
- ProxFlex LF Board 的 CN2.1 `+5V` 必须接 5 V。
- ST25R3916B HF 板的 CN1.6 是 `+5V` 电源输入，板上再生成 3.3 V；ESP32-S3 GPIO 仍然只能接 3.3 V 逻辑。
- ESP32-S3、ProxFlex LF Board、HF 模块、RGB、蜂鸣器必须共地。
- 不要用 ProxFlex LF Board 上的 `3V3/A3V3` 给 ESP32-S3 供电。
- LF 天线、天线匹配电容、驱动回路要远离 SPI/I2C/RGB/蜂鸣器走线。
- ESP32-S3 的 GPIO0、GPIO3、GPIO45、GPIO46 是 strapping pins。GPIO3 可以用，但只建议作为可选输入，不要外接强上拉或强下拉。
- ST25R3916B 的 I2C/SPI 模式由 HF 板上的 `I2C_EN`/J1 硬件设置决定，不靠 ESP32-S3 拉高或拉低 `BSS` 切换。

## ProxFlex LF Board CN2 定义

适用于 ProxFlex V3.3 的 7P CN2：

| CN2 pin | 信号 | 类型 | 是否必须 | 说明 |
|---:|---|---|---|---|
| 1 | `+5V` | 电源输入 | 必须 | LF 模拟前端 5 V 供电 |
| 2 | `GND` | 地 | 必须 | 和 ESP32-S3、HF 模块共地 |
| 3 | `RFID_ADC` | 模拟输出 | 可选 | ADC 调试/兼容通道 |
| 4 | `RFID_PULL` | ESP32 输出 | 必须 | 负载控制；普通读卡时保持 LOW |
| 5 | `RFID_DATA` | 数字输入 | 必须 | 比较器数字输出，进 ESP32 RMT/GPIO |
| 6 | `RFID_CARRIER` | 数字输入 | 可选 | 场强/载波检测 |
| 7 | `RFID_OUT` | ESP32 输出 | 必须 | 125 kHz / 134.2 kHz 载波输入 |

## LF 基础接线

下面这部分在 HF SPI 模式和 HF I2C 模式下都一样。

| ProxFlex CN2 | 信号 | ESP32S3 SuperMini 引脚 | 说明 |
|---:|---|---:|---|
| 1 | `+5V` | `5V` / `VBUS` / 外部 5 V | 如果 USB 供电电流不够，使用外部 5 V |
| 2 | `GND` | `GND` | 必须共地 |
| 7 | `RFID_OUT` | GPIO1 | PWM / 载波输出 |
| 5 | `RFID_DATA` | GPIO2 | 比较器数字输入 |
| 4 | `RFID_PULL` | GPIO4 | ESP32 输出，默认 LOW |
| 3 | `RFID_ADC` | GPIO13 | 可选 ADC 调试输入 |

只要 `RFID_DATA` 比较器输出正常，EM4100/HID Prox 等默认读卡不依赖 `RFID_ADC`。

## ST25R3916B HF 板 CN1 定义

根据 `st25r3916b.tel` 网表，HF 板 CN1 为 7P：

| CN1 pin | 网名 | SPI 模式用途 | I2C 模式用途 |
|---:|---|---|---|
| 1 | `IRQ` | 中断输出 | 中断输出 |
| 2 | `BSS` | SPI 片选/总线选择 | 当前 I2C 接法不接 ESP32 |
| 3 | `SCLK_SCL` | SPI SCK | I2C SCL |
| 4 | `MOSI` | SPI MOSI | 不接 |
| 5 | `MISO_SDA` | SPI MISO | I2C SDA |
| 6 | `+5V` | 5 V 输入 | 5 V 输入 |
| 7 | `GND` | 地 | 地 |

CN1 没有 `RST/EN` 引脚，固件默认 `HF RESET=-1`，不要把 GPIO10 当 HF 复位脚接到这块 HF 板。

## 方案 A：HF 使用 SPI 模式

适合 ST25R3916 这类 HF 读卡芯片，或者希望 HF 通信性能更高的场景。

### SPI 模式引脚分配

| 功能 | HF 模块常见信号名 | ESP32S3 SuperMini 引脚 | 说明 |
|---|---|---:|---|
| SPI 时钟 | `SCK` / `CLK` | GPIO5 | SPI clock |
| SPI MOSI | `MOSI` / `SDI` | GPIO6 | ESP32 到 HF 模块 |
| SPI MISO | `MISO` / `SDO` | GPIO7 | HF 模块到 ESP32 |
| SPI 片选 | `BSS` / `CS` / `SS` / `NSS` | GPIO8 | 独立片选 |
| HF 中断 | `IRQ` / `INT` | GPIO9 | 建议连接 |
| 备用 GPIO | 用户扩展 | GPIO10 | 当前 ST25R3916B CN1 无 RST |
| RGB 数据 | `DIN` | GPIO11 | 串 220R 到 470R 电阻 |
| 蜂鸣器控制 | `BUZZ` | GPIO12 | 建议通过三极管/MOS 管驱动 |
| LF 载波检测 | `RFID_CARRIER` | GPIO3 或 NC | 可选；如果接 GPIO3，串 1k 到 4.7k |

### SPI 模式完整接线表

| ESP32S3 GPIO | 连接到 | 方向 | 是否必须 |
|---:|---|---|---|
| GPIO1 | ProxFlex `RFID_OUT` | 输出 | 必须 |
| GPIO2 | ProxFlex `RFID_DATA` | 输入 | 必须 |
| GPIO3 | ProxFlex `RFID_CARRIER` | 输入 | 可选 |
| GPIO4 | ProxFlex `RFID_PULL` | 输出 | 必须 |
| GPIO5 | HF `SCK` | 输出 | 必须 |
| GPIO6 | HF `MOSI` | 输出 | 必须 |
| GPIO7 | HF `MISO` | 输入 | 必须 |
| GPIO8 | HF `BSS/CS` | 输出 | 必须 |
| GPIO9 | HF `IRQ` | 输入 | 建议 |
| GPIO10 | 备用 | I/O | 可选 |
| GPIO11 | RGB `DIN` | 输出 | 可选 |
| GPIO12 | 蜂鸣器驱动输入 | 输出 | 可选 |
| GPIO13 | ProxFlex `RFID_ADC` | 模拟输入 | 可选 |

### SPI 模式注意事项

- SPI 模式几乎用完 GPIO1 到 GPIO13。
- `RFID_CARRIER` 不是读卡必需信号。若 ESP32-S3 启动异常，优先断开 GPIO3 上的 `RFID_CARRIER`。
- SPI 线尽量短。若线长或示波器看到明显振铃，可在 `SCK`、`MOSI` 串 22R 到 47R。
- SPI 模式下 `BSS/CS` 不要悬空；当前 ST25R3916B CN1 没有 `RST/EN` 引脚。
- SPI/I2C 接口模式由 HF 板上的 `I2C_EN`/J1 硬件设置决定。

## 方案 B：HF 使用 I2C 模式

适合 HF 模块明确支持 I2C，并且更看重少占 GPIO、易焊接的场景。

### I2C 模式引脚分配

| 功能 | HF 模块常见信号名 | ESP32S3 SuperMini 引脚 | 说明 |
|---|---|---:|---|
| I2C SCL | CN1.3 `SCLK_SCL` | GPIO5 | HF 板内通过 `I2C_EN` 网络上拉到 3.3 V |
| I2C SDA | CN1.5 `MISO_SDA` | GPIO7 | HF 板内通过 `I2C_EN` 网络上拉到 3.3 V |
| HF 中断 | CN1.1 `IRQ` | GPIO9 | 建议连接 |
| HF 片选/总线选择 | CN1.2 `BSS` | NC | I2C 模式不接 ESP32，不由主控驱动 |
| HF MOSI | CN1.4 `MOSI` | NC | I2C 模式不接 |
| LF 载波检测 | `RFID_CARRIER` | GPIO3 | 可选；当前固件默认使用 GPIO3 |
| 备用 GPIO | 用户扩展 | GPIO8 / GPIO10 | 可选 |
| RGB 数据 | `DIN` | GPIO11 | 串 220R 到 470R 电阻 |
| 蜂鸣器控制 | `BUZZ` | GPIO12 | 建议通过三极管/MOS 管驱动 |

### I2C 模式完整接线表

| ESP32S3 GPIO | 连接到 | 方向 | 是否必须 |
|---:|---|---|---|
| GPIO1 | ProxFlex `RFID_OUT` | 输出 | 必须 |
| GPIO2 | ProxFlex `RFID_DATA` | 输入 | 必须 |
| GPIO3 | ProxFlex `RFID_CARRIER` | 输入 | 可选 |
| GPIO4 | ProxFlex `RFID_PULL` | 输出 | 必须 |
| GPIO5 | HF CN1.3 `SCLK_SCL` | I2C 时钟 | 必须 |
| GPIO6 | NC | - | I2C 模式不接 HF |
| GPIO7 | HF CN1.5 `MISO_SDA` | I2C 数据 | 必须 |
| GPIO8 | NC / 备用 | I/O | I2C 模式不接 HF `BSS` |
| GPIO9 | HF CN1.1 `IRQ` | 输入 | 建议 |
| GPIO10 | NC / 备用 | I/O | 当前 HF CN1 无 RST |
| GPIO11 | RGB `DIN` | 输出 | 可选 |
| GPIO12 | 蜂鸣器驱动输入 | 输出 | 可选 |
| GPIO13 | ProxFlex `RFID_ADC` | 模拟输入 | 可选 |

### I2C 模式注意事项

- I2C 模式比 SPI 模式节省 GPIO，GPIO6/GPIO8/GPIO10 可以保留给其他扩展。
- `SDA`、`SCL` 只能上拉到 3.3 V。常用上拉电阻为 2.2k 到 4.7k。
- 当前 ST25R3916B 网表中 `SCLK_SCL` 和 `MISO_SDA` 通过 R8/R7 连接到 `I2C_EN` 网络；确认 HF 板 J1 已把 `I2C_EN` 设置到 I2C 工作状态。
- 当前固件和实测使用 100 kHz I2C，地址 `0x50`，已读到 ISO14443A UID。
- `BSS` 不作为 I2C 模式选择脚由 ESP32-S3 控制；I2C 模式下 CN1.2 可以不接。

## 电源连接

| 模块 | 电源脚 | 连接到 | 说明 |
|---|---|---|---|
| ESP32S3 SuperMini | USB-C | 电脑 / USB 电源 | 主控供电 |
| ProxFlex LF | CN2.1 `+5V` | 5 V | 必须 |
| ProxFlex LF | CN2.2 `GND` | 公共 GND | 必须 |
| ST25R3916B HF | CN1.6 `+5V` | 5 V | 必须；板上 U3 生成本地 3.3 V |
| ST25R3916B HF | CN1.7 `GND` | 公共 GND | 必须 |
| RGB LED | `VDD` | 5 V 或 3.3 V | 按 RGB 型号选择 |
| 蜂鸣器 | `VCC` | 3.3 V 或 5 V | 电流超过 GPIO 能力时必须加驱动 |

## 推荐选择

| HF 模式 | 推荐程度 | 原因 |
|---|---|---|
| SPI | 可用 | 速度更高，但多占 GPIO |
| I2C | 当前推荐 | 接线更少；已在 ST25R3916B HF 板上验证读到 ISO14443A UID |

当前固件默认使用 HF I2C：`SCL=GPIO5`，`SDA=GPIO7`，`IRQ=GPIO9`，`RESET=-1`。

## 固件引脚修改

如果固件按本文的 SuperMini 接线，需要把 LF 引脚改成：

```cpp
static constexpr uint8_t PIN_RFID_OUT = 1;
static constexpr uint8_t PIN_RFID_DATA = 2;
static constexpr uint8_t PIN_RFID_PULL = 4;
static constexpr uint8_t PIN_RFID_ADC = 13;
```

SPI 模式下：

```cpp
static constexpr uint8_t PIN_RFID_CARRIER = 3; // 可选
```

I2C 模式下：

```cpp
static constexpr uint8_t PIN_RFID_CARRIER = 3; // 可选
config.pins.hfBusMode = NetworkRfidHfBusMode::I2c;
config.pins.hfSck = 5;    // SCLK_SCL
config.pins.hfMiso = 7;   // MISO_SDA
config.pins.hfIrq = 9;    // IRQ
config.pins.hfReset = -1; // ST25R3916B CN1 no reset pin
```

## 上电调试顺序

1. 只连接 ESP32-S3、ProxFlex `+5V`、公共 GND。
2. 测 ProxFlex 本地 3.3 V 和 A3V3 是否正常。
3. 连接 `RFID_OUT`、`RFID_DATA`、`RFID_PULL`，先测试 LF 读卡。
4. 需要模拟量调试时再连接 `RFID_ADC`。
5. 再连接 HF 模块 CN1.6 `+5V`、CN1.7 `GND` 和 SPI/I2C 总线。
6. 先测试 HF 通信，不要急着接 RGB 和蜂鸣器。
7. LF 和 HF 都稳定后，再接 RGB 和蜂鸣器。

## 参考资料

- [Espressif ESP32-S3 硬件设计指南](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
