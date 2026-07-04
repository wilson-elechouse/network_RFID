# ELECHOUSE Network RFID Reader Mainboard V0.1H Product Brief

本文档用于描述 ELECHOUSE Network RFID Reader Mainboard 当前 `V0.1H` 阶段的产品定位、已实现功能、硬件架构、接口能力和后续版本规划。

`V0.1H` 的产品范围定义为：主控模块 + HF 模块。LF、双频、12V、PoE、Wiegand/ABA 正式产品接口、BLE 和外接 WiFi 天线能力列入后续规划，不默认作为当前版本对外规格。

## 1. Product Overview

ELECHOUSE Network RFID Reader Mainboard 是一款模块化 RFID/NFC 读卡主板，面向网络化读卡、设备配置、读卡数据上传和后续双频扩展。

产品采用模块化规划：

| 模块 | 当前规划 | 说明 |
|---|---|---|
| 主控模块 | ESP32-S3 + Buzzer + LED 预留/反馈接口 | 负责 WiFi、配置、数据处理、命令行、TCP/UART 输出和本地反馈 |
| HF 模块 | ST25R3916B | 13.56 MHz RFID/NFC，当前 `V0.1H` 的核心射频模块 |
| LF 模块 | 125 kHz RFID Board | 后续 `V0.xL` 和双频版本规划使用 |

当前版本 `V0.1H` 已形成 HF 版本的基础闭环：可以完成 HF ID 读取、HF 卡片模拟、WiFi 参数配置、命令行配置、Buzzer 反馈、UART/TCP 数据传输和 5V 供电运行。

当前版本适合：

- HF RFID/NFC 读卡功能验证
- HF 卡片模拟功能验证
- 网络读卡器通信流程验证
- UART/TCP 数据输出集成测试
- 后续产品化主板的功能基线验证

## 2. Version Definition

产品版本按射频模块组合和功能成熟度划分。

| 版本范围 | 模块组合 | 定义 |
|---|---|---|
| `V0.1H` ~ `V0.9H` | 主控 + HF | HF 单频版本演进路线 |
| `V0.1L` ~ `V0.9L` | 主控 + LF | LF 单频版本演进路线 |
| `V1` ~ `V9` | 主控 + HF + LF | HF + LF 双频版本演进路线 |

版本后缀含义：

- `H`：High Frequency，13.56 MHz HF RFID/NFC。
- `L`：Low Frequency，125 kHz LF RFID。
- 无后缀的 `V1` ~ `V9`：双频版本，默认包含 HF 和 LF。

当前版本：

| 项目 | 当前值 |
|---|---|
| 当前版本 | `V0.1H` |
| 当前组合 | 主控 + HF |
| 当前射频方向 | HF 13.56 MHz |
| 当前硬件目标 | ESP32-S3 + ST25R3916B |
| 当前供电 | 5V |

## 3. V0.1H Current Features

`V0.1H` 当前已实现功能如下：

| 分类 | 功能 | V0.1H 状态 | 说明 |
|---|---|---|---|
| HF 读卡 | 基础 ID 读取 | 已实现 | 支持 HF UID/ID 读取输出 |
| HF 模拟 | 卡片模拟 | 已实现 | 支持 NFC 模拟卡方向，具体兼容范围以实测矩阵为准 |
| WiFi | WiFi 参数配置 | 已实现 | 支持 2.4G WiFi 参数配置 |
| 配置 | 命令行配置 | 已实现 | USB CDC、产品 UART、TCP 等入口可复用命令系统 |
| 反馈 | Buzzer 反馈 | 已实现 | 读卡成功可短鸣提示 |
| 数据传输 | UART 数据传输 | 已实现 | 产品接口当前主推 UART 文本输出 |
| 数据传输 | TCP 数据传输 | 已实现 | 支持 TCP client/server 及 ELECHOUSE 在线测试模式 |
| 供电 | 5V 供电 | 已实现 | 当前版本按 5V 输入规划 |

软件文档中已经出现但当前产品资料应谨慎标注的能力：

| 能力 | 建议对外表述 |
|---|---|
| LF 125 kHz 读卡 | 后续 LF/双频版本功能，不作为 `V0.1H` 默认能力 |
| Wiegand D0/D1 | 固件已有相关配置和输出方向，是否作为正式接口开放需结合硬件电平保护确认 |
| ABA Clock/Data | 固件已有相关配置和输出方向，是否作为正式接口开放需结合硬件电平保护确认 |
| WS2816C LED 状态 | 软件已有反馈设计；若当前硬件 LED 未定型，应写为预留/待确认 |
| BLE | 后续主控功能规划，当前 `V0.1H` 不开放 |

## 4. Hardware Architecture

`V0.1H` 硬件架构由主控和 HF 模块组成。

```text
5V Power
   |
   v
ESP32-S3 Main Controller
   |-- WiFi 2.4G
   |-- USB CDC Command/Debug
   |-- Product UART
   |-- TCP Client/Server
   |-- Buzzer Feedback
   |-- LED Feedback Reserved/Optional
   |
   +-- ST25R3916B HF Module
          |-- HF Card Scan
          |-- HF Card Emulation
```

当前主控模块：

| 项目 | 说明 |
|---|---|
| MCU | ESP32-S3 |
| 无线 | 2.4G WiFi；BLE 作为后续规划 |
| 调试/配置 | USB CDC |
| 产品通信 | UART 当前主推；Wiegand/ABA 后续评估 |
| 本地反馈 | Buzzer 当前支持；LED 反馈按硬件版本确认 |

当前 HF 模块：

| 项目 | 说明 |
|---|---|
| HF 芯片 | ST25R3916B |
| 频率 | 13.56 MHz |
| 当前总线 | I2C |
| 默认 SCL | GPIO5 |
| 默认 SDA | GPIO7 |
| 默认 IRQ | GPIO9 |

当前产品接口参考：

| 功能 | GPIO | 当前定位 |
|---|---|---|
| Product UART RX | GPIO44 | 当前产品 UART 接口 |
| Product UART TX | GPIO43 | 当前产品 UART 接口 |
| Buzzer | GPIO12 | 当前反馈接口 |
| WS2816C LED DIN | GPIO11 | 预留/按硬件版本确认 |
| Button | GPIO10 | 若当前硬件装配，可用于配置/恢复默认 |

注意：ESP32-S3 GPIO 是 3.3V 逻辑，不应直接连接 5V 逻辑信号。若后续支持 Wiegand/ABA 或外部门禁控制器，需要在硬件上确认电平转换、开漏输出和保护设计。

## 5. HF RFID Function

`V0.1H` 的核心射频功能是 HF RFID/NFC。

### HF Scan

HF Scan 用于读取 HF 卡片 UID/ID，并通过 UART、TCP 或调试串口输出读卡事件。

当前软件资料中覆盖的 HF 扫描方向包括：

| 协议/技术 | 用途 |
|---|---|
| ISO14443A | 常见 NFC/RFID 卡 UID 读取 |
| ISO14443B | HF 卡片 UID/信息读取 |
| NFC-F | NFC-F 方向扫描 |
| ISO15693 | 远距离 HF 标签 UID 读取 |

典型输出：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

### HF Card Emulation

HF Card Emulation 用于让设备模拟 NFC 标签，向手机或 NFC 读卡器提供 NDEF 内容。

当前规划/软件资料中涉及：

| 项目 | 说明 |
|---|---|
| 模拟方向 | NFC-A |
| Tag 类型 | Type 2 Tag / Type 4 Tag |
| NDEF 内容 | URL、Text、vCard、WiFi 配置 |
| UID | 支持可配置 UID，具体长度按实现和测试结果确认 |

产品资料建议写法：

> V0.1H 支持 HF 卡片模拟能力，当前以 NFC-A NDEF 标签模拟为主。不同手机、读卡器和 NDEF 类型的兼容范围以测试报告为准。

### Not Opened

P2P 当前不作为产品功能开放。

## 6. Configuration Methods

`V0.1H` 支持多种配置方式，便于开发、生产和现场测试。

| 配置入口 | 当前状态 | 用途 |
|---|---|---|
| USB CDC | 支持 | 开发调试、生产配置、查看日志 |
| 产品 UART | 支持 | 外部主机配置和读取事件 |
| TCP Socket | 支持 | 网络配置、远程命令和事件输出 |
| WiFi 网页 | 支持 | 浏览器配置 WiFi/TCP/HF/输出参数 |

默认 WiFi 配置方式：

| 项目 | 默认值 |
|---|---|
| 配置热点 | `ELECHOUSE_RFID` |
| 配置页地址 | `http://10.10.10.10/` |
| USB CDC | `115200 8N1` |

常用命令方向：

```text
help
status
pins
wifi set <ssid> <password>
tcp client <host> <port>
tcp server <port>
format json
format line
hf init
hf mode scan
hf mode card
save
reboot
```

配置原则：

- WiFi、TCP、输出格式、HF 工作角色、反馈参数等属于运行配置。
- 运行配置保存到 ESP32 NVS。
- 硬件 GPIO、HF I2C/SPI 引脚、Buzzer/LED/按键引脚属于板级固定信息，不建议作为用户配置项。
- 硬件版本变化时，应通过 board profile 或固件版本管理，而不是让用户手动填写 GPIO。

## 7. Data Output: UART and TCP

`V0.1H` 当前主推 UART 和 TCP 两类数据输出。

### UART Output

UART 用于外部主机直接接收读卡事件，也可作为命令输入入口。

建议对外规格：

| 项目 | 当前建议 |
|---|---|
| 接口 | Product UART |
| 默认波特率 | 115200 |
| 数据格式 | JSON 或 Line |
| 命令输入 | 可配置开启/关闭 |

Line 输出示例：

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

JSON 输出示例：

```json
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

### TCP Output

TCP 用于网络读卡数据传输和远程命令。

当前软件能力：

| TCP 模式 | 说明 |
|---|---|
| TCP Client | 设备主动连接服务器 |
| TCP Server | 设备监听端口，上位机主动连接 |
| ELECHOUSE Test | 固定连接 ELECHOUSE Broker，用 session code 绑定网页测试页面 |

ELECHOUSE Test 模式：

| 项目 | 固定值 |
|---|---|
| Host | `www.elechouse.com` |
| Port | `9000` |
| Protocol | Plain TCP |
| Session | 网页生成的一次性 session code |

ELECHOUSE Test 上传格式示例：

```json
{"type":"card","band":"HF","card_type":"ISO14443A","uid":"1C 2C FB 10","ms":1002132}
```

## 8. Power Supply

`V0.1H` 当前按 5V 供电定义。

| 项目 | V0.1H 状态 |
|---|---|
| 5V 输入 | 当前支持 |
| 12V 输入 | 后续评估 |
| PoE | 后续评估 |
| 逻辑电平 | ESP32-S3 GPIO 为 3.3V |

当前建议：

- 产品资料中明确写 `5V Power Input`。
- 不写 12V 直接输入，除非硬件已经加入降压、电源保护和测试。
- 不写 PoE，除非硬件已经包含 PoE PD、电源转换和以太网/供电方案。
- 若连接门禁控制器或工业设备，必须确认 GPIO 保护、电平转换和浪涌/ESD 设计。

后续供电方向：

| 方向 | 目标 | 价值 |
|---|---|---|
| 12V 输入 | 兼容门禁/工业常见电源 | 降低现场接入成本 |
| PoE | 网线同时供电和通信 | 简化布线，提高安装效率 |
| 电源保护 | 反接、过压、浪涌、ESD | 提升产品可靠性 |

## 9. Buzzer and Status Feedback

`V0.1H` 当前已实现 Buzzer 反馈。

| 状态 | 当前反馈 |
|---|---|
| 读卡成功 | Buzzer 短鸣 |
| 配置/测试 | 可通过命令触发测试反馈 |

软件资料中已有 LED 反馈设计：

| 状态 | 软件设计 |
|---|---|
| 空闲 | 红灯 |
| 读卡成功 | 绿灯保持一段时间 |

建议对外写法：

- 若 `V0.1H` 实物硬件已经装配并验证 LED：写“支持 LED 状态反馈”。
- 若 LED 只是预留或未完成硬件定型：写“LED 状态反馈预留，后续版本完善”。

反馈能力后续可以扩展为：

| 方向 | 说明 |
|---|---|
| Buzzer 模式 | 区分读卡成功、配置成功、错误状态 |
| LED 状态 | 区分空闲、读卡成功、WiFi 连接、TCP 连接、错误状态 |
| 按键反馈 | 长按进入配置、恢复默认时提供声音/灯光提示 |

## 10. Current Limitations

`V0.1H` 当前限制如下：

| 项目 | 当前限制 |
|---|---|
| LF | 不作为当前 `V0.1H` 对外功能 |
| 双频 | 不支持 HF + LF 双频产品定义 |
| 12V | 当前不支持 12V 直接供电 |
| PoE | 当前不支持 |
| BLE | 当前未开放 BLE 配置、BLE 数据输出或 BLE 管理 |
| Wiegand/ABA | 不建议直接作为当前正式接口宣称，需确认硬件保护和产品化测试 |
| 外接 WiFi 天线 | 当前未作为正式硬件能力 |
| LED | 若当前硬件未定型，应标注为预留/待完善 |
| P2P | 当前不作为产品功能开放 |

需要重点避免的误解：

- 当前软件仓库包含 LF/HF 双频固件内容，但 `V0.1H` 产品定义仍是 HF 版本。
- 当前软件仓库包含 Wiegand/ABA 配置内容，但产品资料应根据硬件接口、电平保护和测试结果决定是否正式开放。
- 当前 ESP32-S3 支持 BLE 基础条件，但 BLE 功能尚未列入 `V0.1H` 当前实现。

## 11. Hardware Evolution Roadmap

后续硬件演进主要围绕主板产品化能力展开。

| 方向 | 目标 | 价值 | 建议阶段 |
|---|---|---|---|
| 12V 供电 | 支持 12V 直接输入 | 兼容门禁和工业现场供电 | `V0.3H` ~ `V0.7H` 评估 |
| PoE | 支持网线供电 | 简化布线，提升安装便利性 | `V0.5H` ~ `V1` 评估 |
| Wiegand | 支持 Wiegand D0/D1 输出 | 兼容传统门禁控制器 | `V0.3H` 以后评估 |
| ABA Clock/Data | 支持 ABA 接口 | 兼容特定门禁/刷卡系统 | 按客户需求评估 |
| 外接 WiFi 天线 | 改善网络覆盖 | 提升现场安装稳定性 | `V0.3H` 以后评估 |
| LED 状态反馈 | 完善本地状态显示 | 便于安装、调试和维护 | `V0.2H` 以后完善 |
| 按键 | 配置/恢复默认 | 提升现场可维护性 | 视硬件装配确认 |
| ESD/浪涌保护 | 提升接口可靠性 | 面向产品化和现场部署 | 产品化版本必须考虑 |
| BLE | BLE 配置、调试、管理 | 提升手机端配置体验 | 后续主控能力规划 |

BLE 后续方向：

| BLE 能力 | 目标 |
|---|---|
| BLE 配网 | 手机通过 BLE 下发 WiFi SSID/Password |
| BLE 配置 | 配置 TCP、输出格式、读卡模式等运行参数 |
| BLE 数据输出 | 通过 BLE GATT 输出读卡事件，适合近距离调试 |
| BLE 设备管理 | 查询固件版本、设备状态、WiFi 状态、信号强度 |
| BLE OTA | 后续评估通过 BLE 升级固件 |

## 12. Version Roadmap

建议版本路线如下。

### HF Route: V0.1H ~ V0.9H

| 版本 | 目标 |
|---|---|
| `V0.1H` | HF 基础闭环：ID 读取、卡片模拟、WiFi 配置、命令行配置、Buzzer、UART/TCP、5V |
| `V0.2H` | 完善 LED/按键状态反馈，整理 V0.1H 测试矩阵 |
| `V0.3H` | 优化 HF 读卡稳定性、输出格式、配置体验，评估 Wiegand/ABA 对外开放 |
| `V0.5H` | 评估外接 WiFi 天线、12V 输入、电源保护和现场接口保护 |
| `V0.7H` | 评估 PoE、BLE 配置/管理、主板产品化接口 |
| `V0.9H` | HF 单频版本接近产品化验证 |

### LF Route: V0.1L ~ V0.9L

| 版本 | 目标 |
|---|---|
| `V0.1L` | 主控 + LF，完成 125 kHz 基础 ID 读取 |
| `V0.3L` | 复用 WiFi、UART、TCP、Buzzer、配置系统 |
| `V0.5L` | 优化 LF 解码稳定性和输出格式 |
| `V0.9L` | LF 单频版本接近产品化验证 |

### Dual Frequency Route: V1 ~ V9

| 版本 | 目标 |
|---|---|
| `V1` | 主控 + HF + LF，完成双频基础读卡 |
| `V2` | 完善 HF/LF 轮询策略和统一数据格式 |
| `V3` | 完善网络、配置、反馈和生产测试流程 |
| `V4` ~ `V5` | 加入更完整的门禁接口、电源和网络能力 |
| `V6` ~ `V9` | 产品化、可靠性、批量部署和远程维护能力 |

## 13. Mechanical and Interface Reference

本章节用于后续整理机械尺寸、连接器、接口定义和安装说明。当前 `V0.1H` 阶段建议至少记录以下信息。

### Required Interface Information

| 项目 | 当前建议记录 |
|---|---|
| 电源输入 | 5V 输入端子/接口形式 |
| HF 天线 | ST25R3916B 模块天线连接和安装方向 |
| USB | USB CDC 调试/烧录接口 |
| 产品 UART | GPIO44/GPIO43 对应连接器位置 |
| Buzzer | 蜂鸣器位置和驱动方式 |
| LED | 若有，记录 LED 类型、位置和含义 |
| Button | 若有，记录按键位置和长按行为 |
| 安装孔 | 孔位、孔径、板框尺寸 |
| 天线安全距离 | HF 天线与金属、线束、电源、电感等距离要求 |

### Current Electrical Notes

| 项目 | 注意事项 |
|---|---|
| GPIO 电平 | ESP32-S3 GPIO 为 3.3V，不是 5V 容忍 |
| HF 模块供电 | ST25R3916B 模块按硬件设计接入 5V/本地稳压 |
| 共地 | 主控、HF 模块、外部接口必须共地 |
| 外部接口 | 连接 5V 门禁控制器时需要电平转换或保护 |
| 天线 | HF 天线附近避免大面积金属和强干扰走线 |

### Future Mechanical Additions

后续产品资料应补充：

- 主板尺寸图
- 安装孔尺寸
- 接口方向图
- 端子定义图
- 天线安装建议
- 外壳/开孔建议
- 贴纸或丝印定义

## 14. Revision History

| 文档版本 | 产品版本 | 日期 | 说明 |
|---|---|---|---|
| `0.1` | `V0.1H` | 2026-07-03 | 初始产品说明草案，基于当前软件 Markdown 资料和 V0.1H 产品定义整理 |

## Appendix: Recommended Short Description

可用于网页、README 或产品简介的短描述：

> ELECHOUSE Network RFID Reader Mainboard V0.1H is an ESP32-S3 and ST25R3916B based HF RFID/NFC reader mainboard. It supports HF ID reading, NFC card emulation, WiFi configuration, command-line configuration, UART/TCP data output, ELECHOUSE online TCP test mode, buzzer feedback, and 5V power input. LF, dual-frequency operation, 12V input, PoE, Wiegand/ABA interfaces, BLE features, and external WiFi antenna support are planned for future versions.

中文短描述：

> ELECHOUSE Network RFID Reader Mainboard V0.1H 是基于 ESP32-S3 和 ST25R3916B 的 HF RFID/NFC 网络读卡主板，支持 HF ID 读取、NFC 卡片模拟、WiFi 配置、命令行配置、UART/TCP 数据输出、ELECHOUSE 在线 TCP 测试、Buzzer 反馈和 5V 供电。LF、双频、12V、PoE、Wiegand/ABA、BLE 和外接 WiFi 天线能力将作为后续版本规划。
