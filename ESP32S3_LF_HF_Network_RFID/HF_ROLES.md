# HF role commands

The HF command set currently exposes reader scan and the card-emulation entry point.
P2P is intentionally disabled in this firmware while card emulation and NDEF payload support are being completed.

```text
hf mode scan
hf mode card
hf card uid <4|7|10-byte hex uid>
hf card ndef url <url>
hf card ndef text <text>
hf card ndef vcard <vcard_text>
hf card ndef wifi <ssid> <password>
hf card on [uid]
hf tech a|b|f|v on|off
hf status
```

Notes:

- `hf mode scan` keeps the existing RFAL discovery UID scanner.
- `hf mode card` configures NFC-A listen parameters and calls the ST25R3916 RF listen API.
- Card emulation is fixed to NFC-A Type 4 Tag / NDEF. The UID and NDEF URL/Text/vCard/WiFi payload are configurable now.
- The currently bundled ST25R3916 driver still returns `ERR_NOTSUPP` for `rfalListenStart()`, so full card emulation also requires implementing listen-mode support in that driver.
- The HF protocol switches (`hf tech a|b|f|v`) apply only to scan mode, not card emulation.
