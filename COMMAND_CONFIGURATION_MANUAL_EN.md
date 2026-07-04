# ELECHOUSE Network RFID Reader Command Configuration Manual

Product: Network RFID Reader Mainboard V0.1H  
Firmware target: ESP32-S3 + ST25R3916B HF version  
Document date: 2026-07-04

## 1. Purpose

This document defines the public command set for the V0.1H firmware. It covers configuration, status checks, and functional tests through USB CDC, UART, TCP, and the ELECHOUSE online test broker.

Only the standard user-manual commands are included. Short aliases, legacy compatibility commands, and temporary development commands are intentionally excluded from the product documentation.

## 2. Command Channels

| Channel | Purpose | Default State | Notes |
|---|---|---|---|
| USB CDC serial | Factory setup, debug, and validation after flashing | Enabled | Default 115200 bps. Connect to a PC and send commands directly. |
| UART product interface | Host controller or external system integration | Configurable | Use `interface commands on` to allow command input through UART. |
| TCP Client/Server | Network configuration and remote debug | Configurable | Use `tcp commands on` to allow command input through TCP. |
| ELECHOUSE Broker | Online web test | Configurable | Use `elechouse on <session_code>` to connect to the website test service. |

## 3. Command Syntax

- End each command with a newline. `\n` or `\r\n` is recommended.
- Command keywords are case-insensitive. For example, `wifi status` and `WIFI STATUS` are equivalent.
- Parameter values preserve their original case. This applies to WiFi SSID, passwords, session codes, and NDEF text.
- Commands and parameters are separated by spaces. SSID, session code, and UID values should not contain spaces.
- Some commands take effect immediately, but they are not retained after power loss unless `save` is executed.
- Use `load` to reload the saved configuration from NVS.
- Use `clear` to erase the saved configuration from NVS.
- `reboot` restarts the device immediately and should be placed at the end of production scripts.

## 4. Save Policy

| Configuration Type | Immediate Effect | Needs `save` | Notes |
|---|---|---|---|
| WiFi parameters | Yes | Yes | `wifi set` reconnects immediately. Use `save` to retain after power loss. |
| TCP parameters | Yes | Yes | `tcp client/server/off` restarts the TCP socket immediately. |
| ELECHOUSE test mode | Yes | Yes | Demo units should use `save` if the session code must be retained. |
| Product interface | Yes | Yes | UART/Wiegand/ABA mode switches take effect immediately. |
| HF/LF enable policy | Yes | Yes | Auto-start behavior is retained only after `save`. |
| Output format and dedupe | Yes | Yes | Save `format`, `window`, and `dedupe` changes if they should persist. |
| Portal SSID | Partial | Yes, then reboot recommended | AP name and password are fully applied after save and reboot. |

## 5. Global Commands

| Command | Function | Example |
|---|---|---|
| `help` | Print the standard command list. | `help` |
| `status` | Print a device-level status summary. | `status` |
| `pins` | Print the current board GPIO assignment. | `pins` |
| `save` | Save the current configuration to NVS. | `save` |
| `load` | Reload configuration from NVS. | `load` |
| `clear` | Clear the saved configuration. | `clear` |
| `reboot` | Restart the device. | `reboot` |
| `test` | Emit one test card event. | `test` |

### 5.1 Output Format

| Command | Function | Parameter |
|---|---|---|
| `format json` | Output card events as JSON. | None |
| `format line` | Output card events as one-line text. | None |

JSON event example:

```text
{"band":"HF","type":"ISO14443A","id":"04 A1 B2 C3 D4","ms":123456}
```

Line event example:

```text
HF,ISO14443A,04 A1 B2 C3 D4,123456
```

### 5.2 Polling Window and Duplicate Suppression

| Command | Function | Range |
|---|---|---|
| `window <lf_ms> <hf_ms>` | Set the LF/HF time-slice windows. | Each value must be at least 20 ms. |
| `dedupe <ms>` | Set duplicate card suppression time. | 0 disables suppression. |
| `auto lf on|off` | Control LF auto-start. | Reserved for LF or dual-frequency versions. |
| `auto hf on|off` | Control HF auto-initialization. | Recommended on V0.1H. |

Recommended V0.1H setup:

```text
auto hf on
format json
dedupe 800
save
```

## 6. WiFi Commands

| Command | Function | Notes |
|---|---|---|
| `wifi status` | Show SSID, connection state, IP, RSSI, and last disconnect reason. | The password is not printed. Only password length is shown. |
| `wifi scan [ssid]` | Scan nearby access points, optionally filtered by SSID. | The scan may briefly occupy WiFi resources. |
| `wifi set <ssid> <password>` | Set router SSID and password. | Reconnects immediately. |
| `wifi reconnect` | Reconnect using the configured SSID and password. | Returns an error if SSID is empty. |
| `wifi clear` | Clear WiFi SSID and password. | Also disconnects the current WiFi connection. |

First-time setup:

```text
wifi scan
wifi set MyWifi MyPassword
wifi status
save
```

Troubleshooting notes:

- `status=CONNECTED` with an `ip=` value means the device has joined the router.
- If `rssi` is lower than about `-75` dBm, network behavior may be unstable.
- After changing router SSID or password, run `wifi set` and then `save`.

## 7. TCP Commands

| Command | Function | Notes |
|---|---|---|
| `tcp status` | Show TCP mode, target, ports, event output, and command input state. | Recommended before testing. |
| `tcp client <host> <port>` | Run as a TCP Client and connect to a server. | Suitable for cloud or LAN services. |
| `tcp server <port>` | Run as a TCP Server and listen on the given port. | Suitable when the host PC connects to the device. |
| `tcp off` | Close the TCP socket. | Does not affect WiFi. |
| `tcp events on|off` | Enable or disable card-event output to TCP. | Affects event output only. |
| `tcp commands on|off` | Enable or disable command input through TCP. | When off, TCP is event-output only. |

TCP Client example:

```text
wifi set MyWifi MyPassword
tcp client 192.168.1.100 9000
tcp events on
tcp commands on
save
```

TCP Server example:

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
save
```

## 8. ELECHOUSE Online Test Commands

| Command | Function | Notes |
|---|---|---|
| `elechouse status` | Show broker address, port, session, and connection state. | Target is fixed to `www.elechouse.com:9000`. |
| `elechouse on <session_code>` | Enable online test mode and set the session code. | Also enables TCP events and commands. |
| `elechouse off` | Disable ELECHOUSE online test mode. | Sets TCP mode to off. |
| `elechouse reconnect` | Reconnect to the broker using the current session code. | Returns an error if session code is empty. |
| `elechouse clear` | Clear the saved session code. | Recommended after production testing. |

Online test flow:

```text
wifi status
elechouse on ABCD1234
elechouse status
test
save
```

Clean up after production test:

```text
elechouse clear
save
```

## 9. Configuration Portal Commands

| Command | Function | Notes |
|---|---|---|
| `portal status` | Show configuration AP state. | Prints AP SSID, IP, and port. |
| `portal on` | Start the configuration AP. | Default IP is `10.10.10.10`. |
| `portal off` | Stop the configuration AP. | If station WiFi is configured, the device tries to return to STA mode. |
| `portal ssid <ssid> [password]` | Set the AP SSID and optional password. | Save and reboot are recommended. |

Example:

```text
portal ssid ELECHOUSE_RFID config1234
save
reboot
```

## 10. Product Interface Commands

GPIO44/GPIO43 are used as the product output interface. They can be switched between UART, Wiegand D0/D1, and ABA Clock/Data modes.

| Command | Function | Notes |
|---|---|---|
| `interface status` | Show interface mode, enable state, baud rate, and pulse parameters. | Recommended after every configuration change. |
| `interface mode uart|wiegand|aba` | Switch the product interface mode. | Restarts the product interface immediately. |
| `interface on` | Enable product interface output. | Uses the current mode. |
| `interface off` | Disable product interface output. | Does not affect USB CDC. |
| `interface events on|off` | Enable or disable card-event output on the interface. | Applies to UART, Wiegand, and ABA. |
| `interface commands on|off` | Enable or disable UART command input. | Meaningful only in UART mode. |
| `interface baud <baud>` | Set UART baud rate and switch to UART mode. | Range: 1200 to 3000000. |
| `interface pulse <us> <gap_us>` | Set Wiegand/ABA pulse width and gap. | `us`: 20 to 1000. `gap_us`: 200 to 20000. |
| `interface wiegand bits <26|34|37|56>` | Set Wiegand bit length. | Default is 34. |
| `interface aba digits <0..32>` | Set ABA output digit count. | 0 means automatic. |
| `interface aba source raw|cn` | Set ABA data source. | `raw` uses raw ID. `cn` uses card-number value. |

UART event output and command input:

```text
interface mode uart
interface baud 115200
interface events on
interface commands on
save
```

Wiegand 34-bit output:

```text
interface mode wiegand
interface wiegand bits 34
interface pulse 80 1800
interface events on
save
```

ABA Clock/Data output:

```text
interface mode aba
interface aba digits 10
interface aba source raw
interface pulse 80 1800
interface events on
save
```

## 11. HF RFID Commands

V0.1H focuses on HF card reading and basic NFC-A card emulation.

| Command | Function | Notes |
|---|---|---|
| `hf status` | Show HF bus, role, initialization, discovery, and emulation state. | First command to use during HF debug. |
| `hf init` | Initialize the HF frontend. | Usually handled by `auto hf on`. |
| `hf off` | Shut down the HF frontend. | Releases HF resources. |
| `hf probe` | Read ST25R3916B chip identity. | For hardware debug. |
| `hf speed <hz>` | Set HF I2C/SPI speed. | Current board normally uses I2C 100000. |
| `hf mode scan|card` | Switch HF scan or card-emulation mode. | `scan` reads cards. `card` emulates a tag. |
| `hf tech a|b|f|v on|off` | Enable or disable scan protocols. | A/B/F/V map to ISO14443A/B, NFC-F, and ISO15693. |

Recommended HF scan setup:

```text
hf init
hf mode scan
hf tech a on
hf tech b on
hf tech f on
hf tech v on
save
```

## 12. HF Card Emulation Commands

| Command | Function | Notes |
|---|---|---|
| `hf card status` | Show simulated card UID, tag type, NDEF type, and active state. | Does not change mode. |
| `hf card uid <hex>` | Set simulated card UID. | Accepts only 4-byte or 7-byte HEX UID. |
| `hf card type nfc-a-t4t` | Set NFC-A Type 4 Tag. | Usually better for phones. |
| `hf card type nfc-a-t2t` | Set NFC-A Type 2 Tag. | Useful for PN532 and common NFC readers. |
| `hf card ndef url <url>` | Set URL NDEF payload. | Uses the ELECHOUSE website if URL is omitted. |
| `hf card ndef text <text>` | Set Text NDEF payload. | The rest of the line is used as text. |
| `hf card ndef vcard <text>` | Set vCard NDEF payload. | For long content, use the web page configuration. |
| `hf card ndef wifi <ssid> <password>` | Set WiFi NDEF payload. | SSID is the first parameter. Password is the remaining text. |

Type 4 URL emulation:

```text
hf mode card
hf card type nfc-a-t4t
hf card uid 02 00 00 01
hf card ndef url https://www.elechouse.com/
hf card status
save
```

Type 2 URL emulation:

```text
hf mode card
hf card type nfc-a-t2t
hf card uid 04 11 22 33 44 55 66
hf card ndef url https://www.elechouse.com/
hf card status
save
```

Return to reader mode:

```text
hf mode scan
save
```

Note: `hf mode card` is the standard command for entering card-emulation mode. `hf card ...` commands only configure simulated-card parameters and show status.

## 13. LF RFID Commands

V0.1H is the HF product variant. LF commands remain in the firmware for LF or dual-frequency versions.

| Command | Function | Notes |
|---|---|---|
| `lf status` | Show LF carrier, frequency, capture state, and active slot. | On boards without LF hardware, this is mainly for state confirmation. |
| `lf init` | Start LF capture and carrier. | Switches to LF slot. |
| `lf off` | Stop LF capture and carrier. | HF can become the active slot again. |
| `lf freq <hz>` | Set LF carrier frequency. | Range: 100000 to 150000. Default is 125000. |
| `lf scan [start stop step ms]` | Scan LF frequency response. | Development and debug. |
| `lf raw <count>` | Output raw samples. | Development and debug. |
| `lf hid [ms]` | Capture HID Prox data. | LF-board related. |
| `lf indala [samples]` | Capture Indala data. | LF-board related. |

## 14. Feedback and Button Commands

### 14.1 Buzzer and LED Feedback

| Command | Function | Notes |
|---|---|---|
| `feedback status` | Show feedback state and parameters. | Includes LED, buzzer, and success hold time. |
| `feedback on` | Enable feedback. | Card-read success gives sound and light feedback. |
| `feedback off` | Disable feedback. | LED and buzzer stop output. |
| `feedback buzzer <hz> <ms>` | Set buzzer frequency and duration. | Frequency: 100 to 20000 Hz. Duration: 0 to 5000 ms. |
| `feedback success_ms <ms>` | Set success-state hold time. | Maximum 60000 ms. |
| `feedback idle <r> <g> <b>` | Set idle LED color. | Each value: 0 to 65535. |
| `feedback success <r> <g> <b>` | Set success LED color. | Each value: 0 to 65535. |
| `feedback test` | Trigger one success feedback. | For production testing. |

### 14.2 Board Button

| Command | Function | Notes |
|---|---|---|
| `button status` | Show button state and long-press timing. | GPIO is fixed by the board profile. |
| `button on` | Enable the configuration button. | Enabled by default. |
| `button off` | Disable the configuration button. | Prevents accidental operation. |
| `button timing <wifi_ms> <reset_ms>` | Set long-press thresholds for WiFi portal and factory reset. | `reset_ms` must be greater than `wifi_ms`. |

Recommended default:

```text
button timing 5000 10000
save
```

## 15. Common Configuration Flows

### 15.1 Factory Baseline

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

### 15.2 LAN TCP Test

```text
wifi set MyWifi MyPassword
tcp server 9000
tcp events on
tcp commands on
save
```

### 15.3 ELECHOUSE Online Web Test

```text
wifi status
elechouse on ABCD1234
elechouse status
test
save
```

### 15.4 Clear Online Session After Production Test

```text
elechouse clear
tcp off
save
```

## 16. Errors and Troubleshooting

| Symptom | Check |
|---|---|
| Command returns `ERR unknown command` | Make sure the command is from this standard manual and not a legacy alias. |
| WiFi has no IP | Run `wifi status` and check connection state and last disconnect reason. |
| TCP has no events | Confirm `tcp events on`, and confirm `tcp status` mode is not off. |
| TCP cannot receive commands | Confirm `tcp commands on`. |
| UART has no events | Confirm `interface mode uart`, `interface events on`, and baud rate. |
| UART cannot receive commands | Confirm `interface commands on`. |
| Wiegand/ABA has no pulses | Confirm `interface mode wiegand` or `interface mode aba`, and confirm `interface events on`. |
| HF has no card events | Run `hf status` and check ready and discovery state. |
| Card emulation does not work | Confirm `hf mode card`, then run `hf card status` and check active/state. |

## 17. Standard Command List

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

## 18. Legacy Commands Not Included in the User Manual

To keep the product documentation concise, the following legacy forms are not published as standard commands: short aliases, top-level `uart`, top-level `wiegand`, top-level `aba`, `tcp echo`, `tcp elechouse`, `elechouse code`, `hf card on/off`, `hf card payload`, and `wifi <ssid> <password>`.

User manuals, test scripts, and web instructions should use only the standard commands listed in Section 17.
