#include "NetworkRfidReader.h"

#include <driver/gpio.h>
#include <new>
#include <stdio.h>
#include <string.h>
#include <st25r3916_com.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#if __has_include(<esp32-hal-rmt.h>)
#include <esp32-hal-rmt.h>
#endif

#ifndef OUTPUT_OPEN_DRAIN
#define OUTPUT_OPEN_DRAIN OUTPUT
#endif

#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(SOC_RMT_SUPPORTED) && SOC_RMT_SUPPORTED
#define NETWORK_RFID_HAS_RMT 1
#else
#define NETWORK_RFID_HAS_RMT 0
#endif

namespace {
constexpr uint32_t kStationConnectTimeoutMs = 15000UL;
constexpr uint32_t kPortalAutoRebootMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kWs2816RmtHz = 10000000UL;
constexpr uint8_t kWs2816T0H = 3;
constexpr uint8_t kWs2816T0L = 10;
constexpr uint8_t kWs2816T1H = 7;
constexpr uint8_t kWs2816T1L = 6;
constexpr uint8_t kWs2816BitsPerPixel = 48;
constexpr uint32_t kButtonDebounceMs = 30;
constexpr uint16_t kButtonBeepGapMs = 120;
constexpr uint16_t kMinProductPulseUs = 20;
constexpr uint16_t kMaxProductPulseUs = 1000;
constexpr uint16_t kMinProductPulseGapUs = 200;
constexpr uint16_t kMaxProductPulseGapUs = 20000;
constexpr const char* kElechouseBrokerHost = "www.elechouse.com";
constexpr uint16_t kElechouseBrokerPort = 9000;
constexpr uint32_t kElechouseHeartbeatMs = 20000UL;
constexpr bool kHfP2pEnabled = false;
constexpr uint32_t kHfCardIsoDepTimeoutMs = 2000UL;
constexpr uint16_t kHfCardRawTxTimeoutMs = 25;
constexpr uint16_t kHfCardFdtListenUs = 100;
constexpr size_t kMaxHfCardPayloadLen = 512;
constexpr size_t kMaxHfCardWifiSsidLen = 32;
constexpr size_t kMaxHfCardWifiPasswordLen = 64;
constexpr uint8_t kT4tFileNone = 0;
constexpr uint8_t kT4tFileCc = 1;
constexpr uint8_t kT4tFileNdef = 2;
constexpr uint16_t kT4tCcFileId = 0xE103;
constexpr uint16_t kT4tNdefFileId = 0xE104;
constexpr size_t kT4tMaxReadChunk = 240;
constexpr uint8_t kIsoDepPcbIBlock = 0x02;
constexpr uint8_t kIsoDepPcbRBlock = 0xA2;
constexpr uint8_t kIsoDepPcbSBlock = 0xC2;
constexpr uint8_t kIsoDepPcbChaining = 0x10;
constexpr uint8_t kIsoDepPcbBlockNumber = 0x01;
constexpr uint8_t kIsoDepPcbTypeMask = 0xC0;
constexpr uint8_t kIsoDepPcbRBlockMask = 0xE0;
constexpr uint8_t kIsoDepPcbSBlockMask = 0xF0;
constexpr uint8_t kIsoDepPcbDeselect = 0xC2;
constexpr uint8_t kIsoDepPcbWtx = 0xF2;
constexpr uint8_t kT4tSwSuccess[] = {0x90, 0x00};
constexpr uint8_t kT4tSwWrongParams[] = {0x6A, 0x86};
constexpr uint8_t kT4tSwFileNotFound[] = {0x6A, 0x82};
constexpr uint8_t kT4tSwConditionsNotSatisfied[] = {0x69, 0x85};
constexpr uint8_t kT4tSwInsNotSupported[] = {0x6D, 0x00};
constexpr uint8_t kT4tNdefAid[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
constexpr uint16_t kIso14443aCrcInit = 0x6363;
constexpr uint32_t kHfCardRawIrqs = ST25R3916_IRQ_MASK_FWL |
                                    ST25R3916_IRQ_MASK_TXE |
                                    ST25R3916_IRQ_MASK_RXS |
                                    ST25R3916_IRQ_MASK_RXE |
                                    ST25R3916_IRQ_MASK_PAR |
                                    ST25R3916_IRQ_MASK_CRC |
                                    ST25R3916_IRQ_MASK_ERR1 |
                                    ST25R3916_IRQ_MASK_ERR2 |
                                    ST25R3916_IRQ_MASK_NRE |
                                    ST25R3916_IRQ_MASK_EON |
                                    ST25R3916_IRQ_MASK_EOF |
                                    ST25R3916_IRQ_MASK_RX_REST;
constexpr uint32_t kHfCardRawErrorIrqs = ST25R3916_IRQ_MASK_PAR |
                                         ST25R3916_IRQ_MASK_CRC |
                                         ST25R3916_IRQ_MASK_ERR1 |
                                         ST25R3916_IRQ_MASK_ERR2;
constexpr uint32_t kHfCardDirectStateIrqs = ST25R3916_IRQ_MASK_EON |
                                            ST25R3916_IRQ_MASK_EOF |
                                            ST25R3916_IRQ_MASK_NFCT |
                                            ST25R3916_IRQ_MASK_RXE_PTA |
                                            ST25R3916_IRQ_MASK_WU_A |
                                            ST25R3916_IRQ_MASK_WU_A_X;
constexpr uint32_t kHfCardDirectIrqs = kHfCardRawIrqs |
                                       kHfCardDirectStateIrqs |
                                       ST25R3916_IRQ_MASK_OSC;

uint16_t iso14443aCrc(const uint8_t* data, size_t length) {
  uint16_t crc = kIso14443aCrcInit;
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i] ^ static_cast<uint8_t>(crc & 0xffU);
    byte ^= static_cast<uint8_t>(byte << 4);
    crc = static_cast<uint16_t>((crc >> 8) ^
                                (static_cast<uint16_t>(byte) << 8) ^
                                (static_cast<uint16_t>(byte) << 3) ^
                                (byte >> 4));
  }
  return crc;
}

const char* hfPtaStateName(uint8_t state) {
  switch (state & ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_state_mask) {
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_power_off:
      return "POWER_OFF";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_idle:
      return "IDLE";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l1:
      return "READY_L1";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l2:
      return "READY_L2";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_active:
      return "ACTIVE";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_halt:
      return "HALT";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l1_x:
      return "READY_L1_X";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l2_x:
      return "READY_L2_X";
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_active_x:
      return "ACTIVE_X";
    default:
      return "RFU";
  }
}

rfalLmState hfPtaToLmState(uint8_t state) {
  switch (state & ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_state_mask) {
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_power_off:
      return RFAL_LM_STATE_POWER_OFF;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_idle:
      return RFAL_LM_STATE_IDLE;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l1:
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l2:
      return RFAL_LM_STATE_READY_A;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l1_x:
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_ready_l2_x:
      return RFAL_LM_STATE_READY_Ax;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_active:
      return RFAL_LM_STATE_ACTIVE_A;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_active_x:
      return RFAL_LM_STATE_ACTIVE_Ax;
    case ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_st_halt:
      return RFAL_LM_STATE_SLEEP_A;
    default:
      return RFAL_LM_STATE_NOT_INIT;
  }
}
constexpr uint8_t kT4tCcFile[] = {
  0x00, 0x0F,
  0x20,
  0x00, 0xF6,
  0x00, 0xF6,
  0x04, 0x06,
  0xE1, 0x04,
  0x02, 0x00,
  0x00,
  0xFF,
};

bool isSupportedWiegandBits(uint8_t bits) {
  return bits == 26 || bits == 34 || bits == 37 || bits == 56;
}

void limitString(String& value, size_t maxLength) {
  if (value.length() > maxLength) {
    value.remove(maxLength);
  }
}

bool appendByte(uint8_t* out, size_t maxLength, size_t& pos, uint8_t value) {
  if (pos >= maxLength) {
    return false;
  }
  out[pos++] = value;
  return true;
}

bool appendBytes(uint8_t* out, size_t maxLength, size_t& pos, const uint8_t* data, size_t length) {
  if (length > (maxLength - pos)) {
    return false;
  }
  memcpy(&out[pos], data, length);
  pos += length;
  return true;
}

bool appendStringBytes(uint8_t* out, size_t maxLength, size_t& pos, const String& value) {
  const size_t length = static_cast<size_t>(value.length());
  return appendBytes(out, maxLength, pos, reinterpret_cast<const uint8_t*>(value.c_str()), length);
}

bool appendNdefHeader(uint8_t* out, size_t maxLength, size_t& pos, uint8_t tnf, uint8_t typeLength, size_t payloadLength) {
  const bool shortRecord = payloadLength <= 255U;
  if (!appendByte(out, maxLength, pos, static_cast<uint8_t>(0x80U | 0x40U | (shortRecord ? 0x10U : 0x00U) | (tnf & 0x07U)))) {
    return false;
  }
  if (!appendByte(out, maxLength, pos, typeLength)) {
    return false;
  }
  if (shortRecord) {
    return appendByte(out, maxLength, pos, static_cast<uint8_t>(payloadLength));
  }
  return appendByte(out, maxLength, pos, static_cast<uint8_t>((payloadLength >> 24) & 0xFFU)) &&
         appendByte(out, maxLength, pos, static_cast<uint8_t>((payloadLength >> 16) & 0xFFU)) &&
         appendByte(out, maxLength, pos, static_cast<uint8_t>((payloadLength >> 8) & 0xFFU)) &&
         appendByte(out, maxLength, pos, static_cast<uint8_t>(payloadLength & 0xFFU));
}

size_t appendStatus(uint8_t* out, size_t maxLength, size_t pos, const uint8_t* sw) {
  if ((maxLength - pos) < 2U) {
    return 0;
  }
  out[pos++] = sw[0];
  out[pos++] = sw[1];
  return pos;
}

#if NETWORK_RFID_HAS_RMT
void appendWs2816Word(rmt_data_t* symbols, size_t& index, uint16_t value) {
  for (int bit = 15; bit >= 0; --bit) {
    const bool one = (value & (1U << bit)) != 0;
    symbols[index].level0 = 1;
    symbols[index].duration0 = one ? kWs2816T1H : kWs2816T0H;
    symbols[index].level1 = 0;
    symbols[index].duration1 = one ? kWs2816T1L : kWs2816T0L;
    ++index;
  }
}
#endif
}

NetworkRfidReader* NetworkRfidReader::activeInstance_ = nullptr;

NetworkRfidReader::NetworkRfidReader() = default;

NetworkRfidReader::~NetworkRfidReader() {
  end();
}

bool NetworkRfidReader::begin(const NetworkRfidConfig& config, Stream& console, bool loadSavedConfig) {
  end();

  config_ = config;
  console_ = &console;
  primaryConsole_ = &console;
  pinMode(0, INPUT_PULLUP);
#if defined(GPIO_NUM_0)
  gpio_pullup_en(GPIO_NUM_0);
  gpio_pulldown_dis(GPIO_NUM_0);
#endif
  if (console_ != nullptr) {
    console_->println();
    console_->println(F("RFID begin"));
  }
  prefsOpen_ = prefs_.begin("lfhf-rfid", false);
  if (loadSavedConfig) {
    loadConfig();
  }
  if (console_ != nullptr) {
    console_->println(loadSavedConfig ? F("RFID config loaded") : F("RFID saved config skipped"));
  }

  activeInstance_ = this;
  if (!wifiDisconnectEventRegistered_) {
    wifiDisconnectEventId_ = WiFi.onEvent(onWiFiEventStatic, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    wifiDisconnectEventRegistered_ = true;
  }

  analogReadResolution(12);
  if (config_.pins.lfAdc >= 0) {
    pinMode(config_.pins.lfAdc, INPUT);
  }
  if (config_.pins.lfCarrierDetect >= 0) {
    pinMode(config_.pins.lfCarrierDetect, INPUT);
  }
  if (config_.pins.lfData >= 0) {
    pinMode(config_.pins.lfData, INPUT);
  }
  if (config_.pins.lfPull >= 0) {
    pinMode(config_.pins.lfPull, OUTPUT);
    digitalWrite(config_.pins.lfPull, LOW);
  }
  if (config_.pins.lfOut >= 0) {
    pinMode(config_.pins.lfOut, OUTPUT);
    digitalWrite(config_.pins.lfOut, LOW);
  }

  setupButton();
  setupProductInterface();
  setupFeedback();
  resetPulseQueue();
  em4100_.reset();
  hidProx_.reset();
  if (console_ != nullptr) {
    console_->println(F("RFID pins ready"));
  }

  bool station_connected = false;
  if (config_.wifiSsid.length() > 0) {
    if (console_ != nullptr) {
      console_->print(F("WiFi connecting ssid="));
      console_->println(config_.wifiSsid);
    }
    startWifi();
    const uint32_t wifi_start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifi_start_ms) < kStationConnectTimeoutMs) {
      serviceButton();
      serviceFeedback();
      delay(50);
    }
    station_connected = WiFi.status() == WL_CONNECTED;
    if (console_ != nullptr) {
      console_->print(F("WiFi "));
      if (station_connected) {
        console_->println(WiFi.localIP().toString());
      } else {
        console_->print(F("connect failed status="));
        console_->print(wifiStatusName(WiFi.status()));
        console_->print(F("("));
        console_->print(static_cast<int>(WiFi.status()));
        console_->print(F(")"));
        if (lastWifiDisconnectReason_ != 0) {
          console_->print(F(" reason="));
          console_->print(lastWifiDisconnectReason_);
          console_->print(F(":"));
          console_->print(WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(lastWifiDisconnectReason_)));
        }
        console_->println();
      }
    }
    if (!station_connected) {
      wifiConnectInProgress_ = false;
      WiFi.disconnect(false, false);
      delay(100);
    }
  }
  if (!station_connected) {
    config_.configPortalEnabled = true;
    startConfigPortal();
  }

  printHelp();
  if (config_.autoStartLf) {
    if (console_ != nullptr) {
      console_->println(F("LF init starting"));
    }
    setActiveSlot(NetworkRfidSlot::LF);
    if (console_ != nullptr) {
      console_->println(F("LF started"));
    }
  } else if (console_ != nullptr) {
    console_->println(F("LF init skipped; use: lf init"));
  }

  if (config_.autoInitHf) {
    if (console_ != nullptr) {
      console_->println(F("HF init starting"));
    }
    hfReady_ = setupHf();
    if (console_ != nullptr) {
      console_->println(hfReady_ ? F("HF init OK") : F("HF init failed"));
    }
  } else if (console_ != nullptr) {
    console_->println(F("HF init skipped; use: hf init"));
  }

  if (!isLfEnabled() && isHfEnabled()) {
    setActiveSlot(NetworkRfidSlot::HF);
  }

  printStatus();
  return hfReady_;
}

void NetworkRfidReader::end() {
  setLfCapture(false);
  setLfCarrier(false);
  stopFeedbackBuzzer();
  setFeedbackLed(0, 0, 0);
  releaseProductInterface();
  stopHfDiscovery();
  stopConfigPortal();

  if (tcpClient_.connected()) {
    tcpClient_.stop();
  }
  if (serverClient_.connected()) {
    serverClient_.stop();
  }
  if (server_ != nullptr) {
    server_->stop();
    delete server_;
    server_ = nullptr;
  }
  activeServerPort_ = 0;

  releaseHf();
  hfReady_ = false;
  hfDiscoveryActive_ = false;

  if (prefsOpen_) {
    prefs_.end();
    prefsOpen_ = false;
  }
  if (wifiDisconnectEventRegistered_) {
    WiFi.removeEvent(wifiDisconnectEventId_);
    wifiDisconnectEventRegistered_ = false;
    wifiDisconnectEventId_ = 0;
  }

  if (activeInstance_ == this) {
    activeInstance_ = nullptr;
  }
}

void NetworkRfidReader::loop() {
  handleSerial();
  serviceButton();
  serviceConfigPortal();
  serviceNetwork();
  serviceFeedback();

  const uint32_t now = millis();
  const bool lf_enabled = isLfEnabled();
  const bool hf_enabled = isHfEnabled();

  if (!lf_enabled && !hf_enabled) {
    return;
  }

  if (!lf_enabled) {
    if (activeSlot_ != NetworkRfidSlot::HF) {
      setActiveSlot(NetworkRfidSlot::HF);
    }
    if (hf_enabled) {
      serviceHf();
    }
    return;
  }

  if (!hf_enabled) {
    if (activeSlot_ != NetworkRfidSlot::LF) {
      setActiveSlot(NetworkRfidSlot::LF);
    }
    processLfPulses();
    return;
  }

  if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
    if (activeSlot_ != NetworkRfidSlot::HF) {
      setActiveSlot(NetworkRfidSlot::HF);
    }
    if (hfReady_) {
      serviceHf();
    }
    return;
  }

  if (activeSlot_ == NetworkRfidSlot::LF) {
    processLfPulses();
    const uint32_t lf_window_ms = config_.lfWindowMs == 0 ? 1 : config_.lfWindowMs;
    if ((now - slotStartMs_) >= lf_window_ms) {
      setActiveSlot(NetworkRfidSlot::HF);
    }
    return;
  }

  if (hfReady_) {
    serviceHf();
  }

  const uint32_t hf_window_ms = config_.hfWindowMs == 0 ? 1 : config_.hfWindowMs;
  if ((now - slotStartMs_) >= hf_window_ms) {
    setActiveSlot(NetworkRfidSlot::LF);
  }
}

const NetworkRfidConfig& NetworkRfidReader::config() const {
  return config_;
}

bool NetworkRfidReader::saveConfig() {
  if (!prefsOpen_) {
    prefsOpen_ = prefs_.begin("lfhf-rfid", false);
  }
  if (!prefsOpen_) {
    return false;
  }

  prefs_.putBool("valid", true);
  prefs_.putString("ssid", config_.wifiSsid);
  prefs_.putString("pass", config_.wifiPassword);
  prefs_.putUChar("tcpMode", static_cast<uint8_t>(config_.tcpMode));
  prefs_.putString("host", config_.tcpHost);
  prefs_.putUShort("port", config_.tcpPort);
  prefs_.putUShort("listen", config_.tcpListenPort);
  prefs_.putUChar("fmt", static_cast<uint8_t>(config_.outputFormat));
  prefs_.putBool("tcpEcho", config_.tcpEchoEvents);
  prefs_.putBool("tcpCmd", config_.tcpCommands);
  prefs_.putString("ehCode", config_.elechouseSessionCode);
  prefs_.putUInt("lfHz", config_.lfCarrierHz);
  prefs_.putUInt("lfWin", config_.lfWindowMs);
  prefs_.putUInt("hfWin", config_.hfWindowMs);
  prefs_.putUInt("dedupe", config_.duplicateSuppressMs);
  prefs_.putUInt("reconn", config_.tcpReconnectMs);
  prefs_.putUInt("hfSpiHz", config_.hfSpiHz);
  prefs_.putUInt("hfI2cHz", config_.hfI2cHz);
  prefs_.putUShort("hfDisc", config_.hfDiscoveryDurationMs);
  prefs_.putUInt("hfTechs", config_.hfTechs);
  prefs_.putUChar("hfRole", static_cast<uint8_t>(config_.hfRole));
  prefs_.putString("hfCardUid", config_.hfCardUid);
  prefs_.putUChar("hfCardTyp", static_cast<uint8_t>(config_.hfCardType));
  prefs_.putUChar("hfNdefTyp", static_cast<uint8_t>(config_.hfCardPayloadType));
  prefs_.putString("hfNdef", config_.hfCardPayload);
  prefs_.putString("hfNdefSsid", config_.hfCardWifiSsid);
  prefs_.putString("hfNdefPass", config_.hfCardWifiPassword);
  prefs_.putString("hfP2pMsg", config_.hfP2pMessage);
  prefs_.putBool("portal", config_.configPortalEnabled);
  prefs_.putString("apSsid", config_.configPortalSsid);
  prefs_.putString("apPass", config_.configPortalPassword);
  prefs_.putUShort("apPort", config_.configPortalPort);
  prefs_.putBool("echo", config_.echoEventsToSerial);
  prefs_.putBool("autoLf", config_.autoStartLf);
  prefs_.putBool("autoHf", config_.autoInitHf);
  prefs_.putBool("fbEn", config_.feedbackEnabled);
  prefs_.putUInt("fbHz", config_.feedbackBuzzerHz);
  prefs_.putUShort("fbBuzzMs", config_.feedbackBuzzerMs);
  prefs_.putUShort("fbSuccMs", config_.feedbackSuccessMs);
  prefs_.putUShort("fbIR", config_.feedbackIdleRed);
  prefs_.putUShort("fbIG", config_.feedbackIdleGreen);
  prefs_.putUShort("fbIB", config_.feedbackIdleBlue);
  prefs_.putUShort("fbSR", config_.feedbackSuccessRed);
  prefs_.putUShort("fbSG", config_.feedbackSuccessGreen);
  prefs_.putUShort("fbSB", config_.feedbackSuccessBlue);
  prefs_.putBool("btnEn", config_.configButtonEnabled);
  prefs_.putUShort("btnCfgMs", config_.buttonWifiConfigMs);
  prefs_.putUShort("btnRstMs", config_.buttonFactoryResetMs);
  prefs_.putBool("uartEn", config_.hardwareUartEnabled);
  prefs_.putUInt("uartBaud", config_.hardwareUartBaud);
  prefs_.putBool("uartEcho", config_.hardwareUartEchoEvents);
  prefs_.putBool("uartCmd", config_.hardwareUartCommands);
  prefs_.putUChar("ifaceMode", static_cast<uint8_t>(config_.productInterfaceMode));
  prefs_.putUChar("wgBits", config_.wiegandBits);
  prefs_.putUShort("pulseUs", config_.productPulseUs);
  prefs_.putUShort("gapUs", config_.productPulseGapUs);
  prefs_.putUChar("abaDigits", config_.abaDigits);
  prefs_.putBool("abaCn", config_.abaUseCardNumber);
  return true;
}

bool NetworkRfidReader::loadConfig() {
  if (!prefsOpen_) {
    prefsOpen_ = prefs_.begin("lfhf-rfid", false);
  }
  if (!prefsOpen_ || !prefs_.isKey("valid")) {
    return false;
  }

  config_.wifiSsid = prefs_.getString("ssid", config_.wifiSsid);
  config_.wifiPassword = prefs_.getString("pass", config_.wifiPassword);
  config_.tcpMode = static_cast<NetworkRfidTcpMode>(prefs_.getUChar("tcpMode", static_cast<uint8_t>(config_.tcpMode)));
  config_.tcpHost = prefs_.getString("host", config_.tcpHost);
  config_.tcpPort = prefs_.getUShort("port", config_.tcpPort);
  config_.tcpListenPort = prefs_.getUShort("listen", config_.tcpListenPort);
  config_.outputFormat = static_cast<NetworkRfidOutputFormat>(prefs_.getUChar("fmt", static_cast<uint8_t>(config_.outputFormat)));
  config_.tcpEchoEvents = prefs_.getBool("tcpEcho", config_.tcpEchoEvents);
  config_.tcpCommands = prefs_.getBool("tcpCmd", config_.tcpCommands);
  config_.elechouseSessionCode = prefs_.getString("ehCode", config_.elechouseSessionCode);
  config_.elechouseSessionCode.trim();
  if (!isValidElechouseSessionCode(config_.elechouseSessionCode)) {
    config_.elechouseSessionCode = "";
  }
  if (config_.tcpMode != NetworkRfidTcpMode::Off &&
      config_.tcpMode != NetworkRfidTcpMode::Client &&
      config_.tcpMode != NetworkRfidTcpMode::Server &&
      config_.tcpMode != NetworkRfidTcpMode::ElechouseTest) {
    config_.tcpMode = NetworkRfidTcpMode::Off;
  }
  config_.lfCarrierHz = prefs_.getUInt("lfHz", config_.lfCarrierHz);
  config_.lfWindowMs = prefs_.getUInt("lfWin", config_.lfWindowMs);
  config_.hfWindowMs = prefs_.getUInt("hfWin", config_.hfWindowMs);
  config_.duplicateSuppressMs = prefs_.getUInt("dedupe", config_.duplicateSuppressMs);
  config_.tcpReconnectMs = prefs_.getUInt("reconn", config_.tcpReconnectMs);
  config_.hfSpiHz = prefs_.getUInt("hfSpiHz", config_.hfSpiHz);
  config_.hfI2cHz = prefs_.getUInt("hfI2cHz", config_.hfI2cHz);
  config_.hfDiscoveryDurationMs = prefs_.getUShort("hfDisc", config_.hfDiscoveryDurationMs);
  config_.hfTechs = static_cast<uint16_t>(prefs_.getUInt("hfTechs", config_.hfTechs));
  config_.hfRole = static_cast<NetworkRfidHfRole>(
      prefs_.getUChar("hfRole", static_cast<uint8_t>(config_.hfRole)));
  if (config_.hfRole != NetworkRfidHfRole::Scan &&
      config_.hfRole != NetworkRfidHfRole::CardEmulation &&
      config_.hfRole != NetworkRfidHfRole::P2p) {
    config_.hfRole = NetworkRfidHfRole::Scan;
  }
  if (!kHfP2pEnabled && config_.hfRole == NetworkRfidHfRole::P2p) {
    config_.hfRole = NetworkRfidHfRole::Scan;
  }
  if (!kHfP2pEnabled) {
    config_.hfTechs &= ~RFAL_NFC_POLL_TECH_AP2P;
  }
  config_.hfCardUid = prefs_.getString("hfCardUid", config_.hfCardUid);
  config_.hfCardType = static_cast<NetworkRfidHfCardType>(
      prefs_.getUChar("hfCardTyp", static_cast<uint8_t>(config_.hfCardType)));
  if (config_.hfCardType != NetworkRfidHfCardType::NfcAType4 &&
      config_.hfCardType != NetworkRfidHfCardType::NfcAType2) {
    config_.hfCardType = NetworkRfidHfCardType::NfcAType4;
  }
  config_.hfCardPayloadType = static_cast<NetworkRfidHfCardPayloadType>(
      prefs_.getUChar("hfNdefTyp", static_cast<uint8_t>(config_.hfCardPayloadType)));
  if (config_.hfCardPayloadType != NetworkRfidHfCardPayloadType::Url &&
      config_.hfCardPayloadType != NetworkRfidHfCardPayloadType::Text &&
      config_.hfCardPayloadType != NetworkRfidHfCardPayloadType::Vcard &&
      config_.hfCardPayloadType != NetworkRfidHfCardPayloadType::Wifi) {
    config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Url;
  }
  config_.hfCardPayload = prefs_.getString("hfNdef", config_.hfCardPayload);
  config_.hfCardWifiSsid = prefs_.getString("hfNdefSsid", config_.hfCardWifiSsid);
  config_.hfCardWifiPassword = prefs_.getString("hfNdefPass", config_.hfCardWifiPassword);
  limitString(config_.hfCardPayload, kMaxHfCardPayloadLen);
  limitString(config_.hfCardWifiSsid, kMaxHfCardWifiSsidLen);
  limitString(config_.hfCardWifiPassword, kMaxHfCardWifiPasswordLen);
  config_.hfP2pMessage = prefs_.getString("hfP2pMsg", config_.hfP2pMessage);
  config_.configPortalEnabled = prefs_.getBool("portal", config_.configPortalEnabled);
  config_.configPortalSsid = prefs_.getString("apSsid", config_.configPortalSsid);
  config_.configPortalPassword = prefs_.getString("apPass", config_.configPortalPassword);
  if (config_.configPortalSsid.startsWith("RFID-")) {
    config_.configPortalSsid = "";
  }
  if (config_.configPortalPassword == "rfid123456") {
    config_.configPortalPassword = "";
  }
  config_.configPortalPort = prefs_.getUShort("apPort", config_.configPortalPort);
  if (config_.configPortalPort == 0) {
    config_.configPortalPort = 80;
  }
  config_.echoEventsToSerial = prefs_.getBool("echo", config_.echoEventsToSerial);
  config_.autoStartLf = prefs_.getBool("autoLf", config_.autoStartLf);
  config_.autoInitHf = prefs_.getBool("autoHf", config_.autoInitHf);
  config_.feedbackEnabled = prefs_.getBool("fbEn", config_.feedbackEnabled);
  config_.feedbackBuzzerHz = prefs_.getUInt("fbHz", config_.feedbackBuzzerHz);
  config_.feedbackBuzzerMs = prefs_.getUShort("fbBuzzMs", config_.feedbackBuzzerMs);
  config_.feedbackSuccessMs = prefs_.getUShort("fbSuccMs", config_.feedbackSuccessMs);
  config_.feedbackIdleRed = prefs_.getUShort("fbIR", config_.feedbackIdleRed);
  config_.feedbackIdleGreen = prefs_.getUShort("fbIG", config_.feedbackIdleGreen);
  config_.feedbackIdleBlue = prefs_.getUShort("fbIB", config_.feedbackIdleBlue);
  config_.feedbackSuccessRed = prefs_.getUShort("fbSR", config_.feedbackSuccessRed);
  config_.feedbackSuccessGreen = prefs_.getUShort("fbSG", config_.feedbackSuccessGreen);
  config_.feedbackSuccessBlue = prefs_.getUShort("fbSB", config_.feedbackSuccessBlue);
  config_.configButtonEnabled = prefs_.getBool("btnEn", config_.configButtonEnabled);
  config_.buttonWifiConfigMs = prefs_.getUShort("btnCfgMs", config_.buttonWifiConfigMs);
  config_.buttonFactoryResetMs = prefs_.getUShort("btnRstMs", config_.buttonFactoryResetMs);
  config_.hardwareUartEnabled = prefs_.getBool("uartEn", config_.hardwareUartEnabled);
  config_.hardwareUartBaud = prefs_.getUInt("uartBaud", config_.hardwareUartBaud);
  config_.hardwareUartEchoEvents = prefs_.getBool("uartEcho", config_.hardwareUartEchoEvents);
  config_.hardwareUartCommands = prefs_.getBool("uartCmd", config_.hardwareUartCommands);
  config_.productInterfaceMode = static_cast<NetworkRfidProductInterfaceMode>(
      prefs_.getUChar("ifaceMode", static_cast<uint8_t>(config_.productInterfaceMode)));
  if (config_.productInterfaceMode != NetworkRfidProductInterfaceMode::Uart &&
      config_.productInterfaceMode != NetworkRfidProductInterfaceMode::Wiegand &&
      config_.productInterfaceMode != NetworkRfidProductInterfaceMode::Aba) {
    config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Uart;
  }
  config_.wiegandBits = prefs_.getUChar("wgBits", config_.wiegandBits);
  if (!isSupportedWiegandBits(config_.wiegandBits)) {
    config_.wiegandBits = 34;
  }
  config_.productPulseUs = prefs_.getUShort("pulseUs", config_.productPulseUs);
  if (config_.productPulseUs < kMinProductPulseUs) {
    config_.productPulseUs = kMinProductPulseUs;
  } else if (config_.productPulseUs > kMaxProductPulseUs) {
    config_.productPulseUs = kMaxProductPulseUs;
  }
  config_.productPulseGapUs = prefs_.getUShort("gapUs", config_.productPulseGapUs);
  if (config_.productPulseGapUs < kMinProductPulseGapUs) {
    config_.productPulseGapUs = kMinProductPulseGapUs;
  } else if (config_.productPulseGapUs > kMaxProductPulseGapUs) {
    config_.productPulseGapUs = kMaxProductPulseGapUs;
  }
  config_.abaDigits = prefs_.getUChar("abaDigits", config_.abaDigits);
  if (config_.abaDigits > 32) {
    config_.abaDigits = 32;
  }
  config_.abaUseCardNumber = prefs_.getBool("abaCn", config_.abaUseCardNumber);
  return true;
}

void NetworkRfidReader::clearSavedConfig() {
  if (!prefsOpen_) {
    prefsOpen_ = prefs_.begin("lfhf-rfid", false);
  }
  if (prefsOpen_) {
    prefs_.clear();
  }
}

void NetworkRfidReader::setCardCallback(NetworkRfidCardCallback callback, void* context) {
  callback_ = callback;
  callbackContext_ = context;
}

void IRAM_ATTR NetworkRfidReader::onLfDataEdgeStatic() {
  if (activeInstance_ != nullptr) {
    activeInstance_->onLfDataEdge();
  }
}

void NetworkRfidReader::onHfStateChangeStatic(rfalNfcState state) {
  if (activeInstance_ != nullptr) {
    activeInstance_->onHfStateChange(state);
  }
}

void NetworkRfidReader::onWiFiEventStatic(arduino_event_id_t event, arduino_event_info_t info) {
  if (activeInstance_ == nullptr || event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    return;
  }
  activeInstance_->lastWifiDisconnectReason_ = info.wifi_sta_disconnected.reason;
  activeInstance_->lastWifiDisconnectMs_ = millis();
  activeInstance_->wifiConnectInProgress_ = false;
}

void IRAM_ATTR NetworkRfidReader::onLfDataEdge() {
  const uint32_t now = micros();
  const uint32_t duration = now - lastEdgeUs_;
  const uint8_t new_level = gpio_get_level(static_cast<gpio_num_t>(config_.pins.lfData)) ? 1 : 0;

  const uint16_t next_head = (pulseHead_ + 1) & (PulseQueueSize - 1);
  if (next_head != pulseTail_) {
    pulseQueue_[pulseHead_].durationUs = duration > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(duration);
    pulseQueue_[pulseHead_].level = lastDataLevel_;
    pulseHead_ = next_head;
    capturedPulses_++;
  } else {
    droppedPulses_++;
  }

  lastEdgeUs_ = now;
  lastDataLevel_ = new_level;
}

bool NetworkRfidReader::popPulse(PulseSample& pulse) {
  noInterrupts();
  if (pulseTail_ == pulseHead_) {
    interrupts();
    return false;
  }
  pulse.durationUs = pulseQueue_[pulseTail_].durationUs;
  pulse.level = pulseQueue_[pulseTail_].level;
  pulseTail_ = (pulseTail_ + 1) & (PulseQueueSize - 1);
  interrupts();
  return true;
}

void NetworkRfidReader::resetPulseQueue() {
  noInterrupts();
  pulseHead_ = 0;
  pulseTail_ = 0;
  droppedPulses_ = 0;
  capturedPulses_ = 0;
  lastDataLevel_ = (config_.pins.lfData >= 0) ?
    (gpio_get_level(static_cast<gpio_num_t>(config_.pins.lfData)) ? 1 : 0) : 0;
  lastEdgeUs_ = micros();
  interrupts();
}

void NetworkRfidReader::setLfCarrier(bool enabled) {
  if (enabled == lfCarrierEnabled_) {
    return;
  }

  if (config_.pins.lfOut < 0) {
    lfCarrierEnabled_ = false;
    return;
  }

  if (enabled) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (!ledcAttach(config_.pins.lfOut, config_.lfCarrierHz, 8)) {
      if (console_ != nullptr) {
        console_->println(F("ERR ledcAttach failed"));
      }
      return;
    }
    ledcWrite(config_.pins.lfOut, 128);
#else
    ledcSetup(config_.pins.lfLedcChannel, config_.lfCarrierHz, 8);
    ledcAttachPin(config_.pins.lfOut, config_.pins.lfLedcChannel);
    ledcWrite(config_.pins.lfLedcChannel, 128);
#endif
    lfCarrierEnabled_ = true;
    return;
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(config_.pins.lfOut);
#else
  ledcDetachPin(config_.pins.lfOut);
#endif
  pinMode(config_.pins.lfOut, OUTPUT);
  digitalWrite(config_.pins.lfOut, LOW);
  lfCarrierEnabled_ = false;
}

void NetworkRfidReader::setLfCapture(bool enabled) {
  if (enabled == lfCaptureEnabled_) {
    return;
  }
  if (config_.pins.lfData < 0) {
    lfCaptureEnabled_ = false;
    return;
  }

  if (enabled) {
    resetPulseQueue();
    em4100_.reset();
    hidProx_.reset();
    indala_.reset();
    attachInterrupt(digitalPinToInterrupt(config_.pins.lfData), onLfDataEdgeStatic, CHANGE);
    lfCaptureEnabled_ = true;
  } else {
    detachInterrupt(digitalPinToInterrupt(config_.pins.lfData));
    lfCaptureEnabled_ = false;
  }
}

void NetworkRfidReader::setActiveSlot(NetworkRfidSlot slot) {
  if ((slot == activeSlot_) && (slotStartMs_ != 0)) {
    return;
  }

  if (slot == NetworkRfidSlot::LF) {
    stopHfDiscovery();
    if (config_.pins.lfPull >= 0) {
      digitalWrite(config_.pins.lfPull, LOW);
    }
    setLfCarrier(true);
    setLfCapture(true);
  } else {
    setLfCapture(false);
    setLfCarrier(false);
    if (config_.pins.lfPull >= 0) {
      digitalWrite(config_.pins.lfPull, LOW);
    }
    restartHfRole();
  }

  activeSlot_ = slot;
  slotStartMs_ = millis();
}

bool NetworkRfidReader::isLfEnabled() const {
  return config_.autoStartLf;
}

bool NetworkRfidReader::isHfEnabled() const {
  return config_.autoInitHf && hfReady_ && nfc_ != nullptr;
}

void NetworkRfidReader::releaseHf() {
  stopHfDiscovery();
  stopHfCardEmulation();
  hfReady_ = false;
  delete nfc_;
  nfc_ = nullptr;
  delete hfReader_;
  hfReader_ = nullptr;
  delete spi_;
  spi_ = nullptr;
  i2c_ = nullptr;
}

void NetworkRfidReader::processLfPulses() {
  PulseSample pulse = {};
  Em4100Id em_id = {};
  Em4100DecodeInfo em_info = {};
  HidH10301Id h10301 = {};
  HidGenericId generic = {};
  HidDecodeInfo hid_info = {};
  uint16_t processed = 0;

  while (processed < 2048 && popPulse(pulse)) {
    processed++;
    if (lfRawDumpRemaining_ > 0 && console_ != nullptr) {
      console_->print(F("LFRAW level="));
      console_->print(pulse.level ? 1 : 0);
      console_->print(F(" us="));
      console_->println(pulse.durationUs);
      lfRawDumpRemaining_--;
    }

    if (hidProx_.pushPulse(pulse.level != 0, pulse.durationUs, h10301, generic, hid_info)) {
      if (hid_info.h10301) {
        String id = F("FC=");
        id += String(h10301.facility);
        id += F(" CN=");
        id += String(h10301.card);
        id += F(" RAW=");
        id += hexBytes(h10301.raw, sizeof(h10301.raw), true);
        emitCard("LF", "HID-H10301", id);
        lastH10301_ = h10301;
        haveLastH10301_ = true;
        lastH10301EmitMs_ = millis();
      } else if (hid_info.generic) {
        if (generic.bit_size == 26 && haveLastH10301_ && (millis() - lastH10301EmitMs_) < 250UL) {
          continue;
        }
        String id = String(generic.bit_size);
        id += F("-bit ");
        id += hexBytes(generic.data, sizeof(generic.data), true);
        emitCard("LF", "HIDProx", id);
      }
    }

    if (em4100_.pushPulse(pulse.level != 0, pulse.durationUs, em_id, em_info)) {
      emitCard("LF", "EM4100", hexBytes(em_id.bytes, sizeof(em_id.bytes), true));
    }
  }
}

void NetworkRfidReader::processLfHidPulses(uint16_t max_pulses) {
  PulseSample pulse = {};
  HidH10301Id h10301 = {};
  HidGenericId generic = {};
  HidDecodeInfo hid_info = {};
  uint16_t processed = 0;

  while (processed < max_pulses && popPulse(pulse)) {
    processed++;
    if (hidProx_.pushPulse(pulse.level != 0, pulse.durationUs, h10301, generic, hid_info)) {
      if (hid_info.h10301) {
        String id = F("FC=");
        id += String(h10301.facility);
        id += F(" CN=");
        id += String(h10301.card);
        id += F(" RAW=");
        id += hexBytes(h10301.raw, sizeof(h10301.raw), true);
        emitCard("LF", "HID-H10301", id);
        lastH10301_ = h10301;
        haveLastH10301_ = true;
        lastH10301EmitMs_ = millis();
      } else if (hid_info.generic) {
        if (generic.bit_size == 26 && haveLastH10301_ && (millis() - lastH10301EmitMs_) < 250UL) {
          continue;
        }
        String id = String(generic.bit_size);
        id += F("-bit ");
        id += hexBytes(generic.data, sizeof(generic.data), true);
        emitCard("LF", "HIDProx", id);
      }
    }
  }
}

bool NetworkRfidReader::setupHf() {
  if (hfReady_) {
    return true;
  }

  releaseHf();

  if (config_.pins.hfMode >= 0) {
    pinMode(config_.pins.hfMode, OUTPUT);
    digitalWrite(config_.pins.hfMode, config_.pins.hfModeLevel);
  }

  if (config_.pins.hfReset >= 0) {
    const uint8_t inactive = config_.pins.hfResetActiveLow ? HIGH : LOW;
    const uint8_t active = config_.pins.hfResetActiveLow ? LOW : HIGH;
    pinMode(config_.pins.hfReset, OUTPUT);
    digitalWrite(config_.pins.hfReset, inactive);
    delay(5);
    digitalWrite(config_.pins.hfReset, active);
    delay(5);
    digitalWrite(config_.pins.hfReset, inactive);
    delay(20);
  }

  if (config_.pins.hfIrq >= 0) {
    pinMode(config_.pins.hfIrq, INPUT);
  }

  if (config_.pins.hfBusMode == NetworkRfidHfBusMode::I2c) {
    if (config_.pins.hfSck < 0 || config_.pins.hfMiso < 0 || config_.pins.hfIrq < 0) {
      if (console_ != nullptr) {
        console_->println(F("HF I2C init failed: SCL/SDA/IRQ pin disabled"));
      }
      return false;
    }
    i2c_ = &Wire;
    i2c_->begin(config_.pins.hfMiso, config_.pins.hfSck, config_.hfI2cHz);
    delay(20);
    hfReader_ = new RfalRfST25R3916Class(i2c_, config_.pins.hfIrq);
  } else {
    if (config_.pins.hfCs < 0) {
      if (console_ != nullptr) {
        console_->println(F("HF SPI init failed: CS pin disabled"));
      }
      return false;
    }
    pinMode(config_.pins.hfCs, OUTPUT);
    digitalWrite(config_.pins.hfCs, HIGH);
    spi_ = new SPIClass(config_.pins.hfSpiBus);
    hfReader_ = new RfalRfST25R3916Class(spi_, config_.pins.hfCs, config_.pins.hfIrq, config_.hfSpiHz);
    spi_->begin(config_.pins.hfSck, config_.pins.hfMiso, config_.pins.hfMosi, config_.pins.hfCs);
  }

  nfc_ = new RfalNfcClass(hfReader_);

  const ReturnCode err = nfc_->rfalNfcInitialize();
  if (err != ERR_NONE) {
    if (console_ != nullptr) {
      console_->print(F("HF init failed: "));
      console_->println(static_cast<int>(err));
    }
    releaseHf();
    return false;
  }

  return true;
}

bool NetworkRfidReader::probeHfIdentity() {
  if (console_ != nullptr) {
    console_->print(F("HF probe bus="));
    console_->print(hfBusModeName(config_.pins.hfBusMode));
    console_->print(F(" SCK/SCL="));
    console_->print(config_.pins.hfSck);
    console_->print(F(" MOSI="));
    console_->print(config_.pins.hfMosi);
    console_->print(F(" MISO/SDA="));
    console_->print(config_.pins.hfMiso);
    console_->print(F(" CS="));
    console_->print(config_.pins.hfCs);
    console_->print(F(" IRQ="));
    console_->print(config_.pins.hfIrq);
    console_->print(F(" SPI="));
    console_->print(config_.hfSpiHz);
    console_->print(F(" I2C="));
    console_->println(config_.hfI2cHz);
  }

  if (config_.pins.hfBusMode == NetworkRfidHfBusMode::Spi && config_.pins.hfCs < 0) {
    if (console_ != nullptr) {
      console_->println(F("HF probe failed: CS pin disabled"));
    }
    return false;
  }

  if (config_.pins.hfMode >= 0) {
    pinMode(config_.pins.hfMode, OUTPUT);
    digitalWrite(config_.pins.hfMode, config_.pins.hfModeLevel);
  }

  if (config_.pins.hfReset >= 0) {
    const uint8_t inactive = config_.pins.hfResetActiveLow ? HIGH : LOW;
    const uint8_t active = config_.pins.hfResetActiveLow ? LOW : HIGH;
    pinMode(config_.pins.hfReset, OUTPUT);
    digitalWrite(config_.pins.hfReset, inactive);
    delay(5);
    digitalWrite(config_.pins.hfReset, active);
    delay(5);
    digitalWrite(config_.pins.hfReset, inactive);
    delay(20);
  }

  if (config_.pins.hfIrq >= 0) {
    pinMode(config_.pins.hfIrq, INPUT);
  }

  if (config_.pins.hfBusMode == NetworkRfidHfBusMode::I2c) {
    if (config_.pins.hfSck < 0 || config_.pins.hfMiso < 0 || config_.pins.hfIrq < 0) {
      if (console_ != nullptr) {
        console_->println(F("HF probe failed: SCL/SDA/IRQ pin disabled"));
      }
      return false;
    }
    Wire.begin(config_.pins.hfMiso, config_.pins.hfSck, config_.hfI2cHz);
    delay(20);
    Wire.beginTransmission(0x50);
    const uint8_t ack = static_cast<uint8_t>(Wire.endTransmission(true));
    if (console_ != nullptr) {
      console_->print(F("HF I2C empty-write ACK 0x50="));
      console_->println(static_cast<int>(ack));
    }
    RfalRfST25R3916Class probe_reader(&Wire, config_.pins.hfIrq);

    uint8_t id = 0;
    const ReturnCode err = probe_reader.st25r3916ReadRegister(ST25R3916_REG_IC_IDENTITY, &id);

    const uint8_t type = id & ST25R3916_REG_IC_IDENTITY_ic_type_mask;
    const uint8_t rev = id & ST25R3916_REG_IC_IDENTITY_ic_rev_mask;
    const bool match = (type == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916 ||
                        type == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916b);

    if (console_ != nullptr) {
      console_->print(F("HF probe err="));
      console_->print(static_cast<int>(err));
      console_->print(F(" identity=0x"));
      if (id < 0x10) {
        console_->print('0');
      }
      console_->print(id, HEX);
      console_->print(F(" type=0x"));
      if (type < 0x10) {
        console_->print('0');
      }
      console_->print(type, HEX);
      console_->print(F(" rev="));
      console_->print(rev);
      console_->print(F(" match="));
      console_->println(match ? F("yes") : F("no"));
    }

    return (err == ERR_NONE) && match;
  }

  if (config_.pins.hfCs >= 0) {
    pinMode(config_.pins.hfCs, OUTPUT);
    digitalWrite(config_.pins.hfCs, HIGH);
  }
  SPIClass probe_spi(config_.pins.hfSpiBus);
  RfalRfST25R3916Class probe_reader(&probe_spi,
                                    config_.pins.hfCs,
                                    config_.pins.hfIrq,
                                    config_.hfSpiHz);
  probe_spi.begin(config_.pins.hfSck, config_.pins.hfMiso, config_.pins.hfMosi, config_.pins.hfCs);

  uint8_t id = 0;
  const ReturnCode err = probe_reader.st25r3916ReadRegister(ST25R3916_REG_IC_IDENTITY, &id);
  probe_spi.end();

  const uint8_t type = id & ST25R3916_REG_IC_IDENTITY_ic_type_mask;
  const uint8_t rev = id & ST25R3916_REG_IC_IDENTITY_ic_rev_mask;
  const bool match = (type == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916 ||
                      type == ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916b);

  if (console_ != nullptr) {
    console_->print(F("HF probe err="));
    console_->print(static_cast<int>(err));
    console_->print(F(" identity=0x"));
    if (id < 0x10) {
      console_->print('0');
    }
    console_->print(id, HEX);
    console_->print(F(" type=0x"));
    if (type < 0x10) {
      console_->print('0');
    }
    console_->print(type, HEX);
    console_->print(F(" rev="));
    console_->print(rev);
    console_->print(F(" match="));
    console_->println(match ? F("yes") : F("no"));
  }

  return (err == ERR_NONE) && match;
}

uint16_t NetworkRfidReader::activeHfTechs() const {
  if (kHfP2pEnabled && config_.hfRole == NetworkRfidHfRole::P2p) {
    return RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_F | RFAL_NFC_POLL_TECH_AP2P;
  }
  uint16_t techs = config_.hfTechs == 0 ? RFAL_NFC_POLL_TECH_A : config_.hfTechs;
  if (!kHfP2pEnabled) {
    techs &= ~RFAL_NFC_POLL_TECH_AP2P;
  }
  return techs == 0 ? RFAL_NFC_POLL_TECH_A : techs;
}

void NetworkRfidReader::serviceHf() {
  if (!isHfEnabled()) {
    return;
  }

  if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
    serviceHfCardEmulation();
    return;
  }

  stopHfCardEmulation();
  if (nfc_ == nullptr) {
    return;
  }
  if (!hfDiscoveryActive_ && !hfDataExchangeActive_) {
    startHfDiscovery();
  }
  nfc_->rfalNfcWorker();
  serviceHfDataExchange();
}

void NetworkRfidReader::restartHfRole() {
  if (!isHfEnabled()) {
    return;
  }

  if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
    stopHfDiscovery();
    startHfCardEmulation();
  } else {
    stopHfCardEmulation();
    startHfDiscovery();
  }
}

void NetworkRfidReader::startHfDiscovery() {
  if (!hfReady_ || nfc_ == nullptr || hfDiscoveryActive_) {
    return;
  }
  if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastHfDiscoverAttemptMs_) < 250U) {
    return;
  }
  lastHfDiscoverAttemptMs_ = now;

  memset(&hfDiscover_, 0, sizeof(hfDiscover_));
  hfDiscover_.compMode = RFAL_COMPLIANCE_MODE_NFC;
  hfDiscover_.devLimit = RFAL_ESP32_DEFAULT_DEVICE_LIMIT;
  hfDiscover_.techs2Find = activeHfTechs();
  hfDiscover_.totalDuration = config_.hfDiscoveryDurationMs;
  hfDiscover_.nfcfBR = RFAL_BR_212;
  hfDiscover_.ap2pBR = RFAL_BR_424;
  const uint8_t nfcid3[RFAL_NFCDEP_NFCID3_LEN] = {
      0x02, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0x01};
  memcpy(hfDiscover_.nfcid3, nfcid3, sizeof(nfcid3));
  hfDiscover_.notifyCb = onHfStateChangeStatic;

  const ReturnCode err = nfc_->rfalNfcDiscover(&hfDiscover_);
  if (err == ERR_NONE) {
    hfDiscoveryActive_ = true;
  } else if (console_ != nullptr) {
    console_->print(F("HF discovery failed: "));
    console_->println(static_cast<int>(err));
  }
}

void NetworkRfidReader::stopHfDiscovery() {
  if (nfc_ != nullptr && nfc_->rfalNfcGetState() > RFAL_NFC_STATE_IDLE) {
    nfc_->rfalNfcDeactivate(false);
  }
  if (hfReader_ != nullptr) {
    hfReader_->rfalFieldOff();
  }
  hfDiscoveryActive_ = false;
  hfDataExchangeActive_ = false;
  hfExchangeRxData_ = nullptr;
  hfExchangeRxLen_ = nullptr;
}

void NetworkRfidReader::onHfStateChange(rfalNfcState state) {
  if (state != RFAL_NFC_STATE_ACTIVATED || nfc_ == nullptr) {
    return;
  }

  rfalNfcDevice* device = nullptr;
  if (nfc_->rfalNfcGetActiveDevice(&device) == ERR_NONE && device != nullptr) {
    emitCard("HF", hfTypeName(device->type), hexBytes(device->nfcid, device->nfcidLen, true));
    if (kHfP2pEnabled && config_.hfRole == NetworkRfidHfRole::P2p && isHfP2pDevice(device) && beginHfDataExchange(device)) {
      return;
    }
  }

  nfc_->rfalNfcDeactivate(true);
  hfDiscoveryActive_ = true;
}

bool NetworkRfidReader::startHfCardEmulation() {
  if (!hfReady_ || hfReader_ == nullptr || hfCardEmulationActive_) {
    return hfCardEmulationActive_;
  }

  const uint32_t now = millis();
  if ((now - lastHfCardEmuAttemptMs_) < 1000U) {
    return false;
  }
  lastHfCardEmuAttemptMs_ = now;

  uint8_t uid[RFAL_NFCID1_TRIPLE_LEN] = {};
  size_t uid_len = 0;
  if (!parseHexBytes(config_.hfCardUid, uid, sizeof(uid), uid_len) ||
      (uid_len != 4 && uid_len != 7)) {
    if (console_ != nullptr) {
      console_->println(F("ERR hf card uid must be 4 or 7 hex bytes"));
    }
    return false;
  }

  resetHfCardProtocol();
  memset(&hfCardEmuA_, 0, sizeof(hfCardEmuA_));
  const bool type2 = config_.hfCardType == NetworkRfidHfCardType::NfcAType2;
  if (type2 && config_.pins.hfBusMode == NetworkRfidHfBusMode::I2c &&
      i2c_ != nullptr && config_.hfI2cHz < 400000UL) {
    config_.hfI2cHz = 400000UL;
    i2c_->setClock(config_.hfI2cHz);
  }
  const uint8_t atqa0 = (type2 && uid_len == 7U) ? 0x44U : 0x04U;
  const uint8_t finalSak = type2 ? 0x00U : 0x20U;
  hfCardEmuA_.nfcidLen = uid_len == 4 ? RFAL_LM_NFCID_LEN_04 :
                         RFAL_LM_NFCID_LEN_07;
  memcpy(hfCardEmuA_.nfcid, uid, uid_len);
  hfCardEmuA_.SENS_RES[0] = atqa0;
  hfCardEmuA_.SENS_RES[1] = 0x00;
  hfCardEmuA_.SEL_RES = finalSak;
  hfListenRxBits_ = 0;
  memset(hfListenRxBuf_, 0, sizeof(hfListenRxBuf_));
  if (type2 && buildHfT2tMemory(hfCardT2tMemory_, sizeof(hfCardT2tMemory_)) == 0U) {
    if (console_ != nullptr) {
      console_->println(F("ERR hf Type 2 memory build failed"));
    }
    return false;
  }
  hfLastLmState_ = RFAL_LM_STATE_NOT_INIT;
  hfCardLastPtState_ = 0xFF;
  hfCardDirectRxArmed_ = false;

  if (nfc_ != nullptr && nfc_->rfalNfcGetState() > RFAL_NFC_STATE_IDLE) {
    nfc_->rfalNfcDeactivate(false);
  }
  hfDiscoveryActive_ = false;
  hfDataExchangeActive_ = false;
  hfReader_->rfalFieldOff();

  if (!configureHfCardDirectListener(uid, uid_len)) {
    return false;
  }

  hfCardEmulationActive_ = true;
  if (console_ != nullptr) {
    console_->print(F("HF card direct emulation active uid="));
    console_->print(hexBytes(hfCardEmuA_.nfcid, uid_len, true));
    console_->print(F(" type="));
    console_->println(hfCardTypeName(config_.hfCardType));
  }
  return true;
}

void NetworkRfidReader::stopHfCardEmulation() {
  if (hfReader_ != nullptr && hfCardEmulationActive_) {
    hfReader_->st25r3916DisableInterrupts(ST25R3916_IRQ_MASK_ALL);
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);
    hfReader_->st25r3916WriteRegister(ST25R3916_REG_MODE, ST25R3916_REG_MODE_om0);
    hfReader_->st25r3916ClrRegisterBits(ST25R3916_REG_OP_CONTROL,
                                        ST25R3916_REG_OP_CONTROL_rx_en |
                                          ST25R3916_REG_OP_CONTROL_tx_en |
                                          ST25R3916_REG_OP_CONTROL_en_fd_mask);
  }
  hfCardEmulationActive_ = false;
  resetHfCardProtocol();
}

void NetworkRfidReader::serviceHfCardEmulation() {
  if (!hfCardEmulationActive_) {
    startHfCardEmulation();
    return;
  }

  if (hfReader_ == nullptr) {
    hfCardEmulationActive_ = false;
    return;
  }

  if (hfCardIsoDepActive_) {
    serviceHfCardIsoDep();
    return;
  }

  serviceHfCardDirectState();

  if (hfLastLmState_ != RFAL_LM_STATE_ACTIVE_A && hfLastLmState_ != RFAL_LM_STATE_ACTIVE_Ax) {
    return;
  }

  if (config_.hfCardType == NetworkRfidHfCardType::NfcAType2) {
    serviceHfCardType2();
    return;
  }

  size_t rx_bytes = 0;
  const ReturnCode ret = pollHfCardRawFrame(rx_bytes);
  if (ret == ERR_BUSY) {
    return;
  }

  if (ret != ERR_NONE) {
    if (ret != ERR_TIMEOUT && console_ != nullptr) {
      console_->print(F("HF card direct RATS RX discarded: "));
      console_->println(static_cast<int>(ret));
    }
    enableHfCardDirectRx();
    return;
  }

  if (!handleHfCardListenFrame(hfListenRxBuf_, rx_bytes)) {
    if (console_ != nullptr) {
      console_->print(F("HF card direct RX before RATS="));
      console_->println(hexBytes(hfListenRxBuf_, rx_bytes, true));
    }
    enableHfCardDirectRx();
  }
}

bool NetworkRfidReader::configureHfCardDirectListener(const uint8_t* uid, size_t uidLen) {
  if (hfReader_ == nullptr || uid == nullptr || (uidLen != 4U && uidLen != 7U)) {
    return false;
  }

  uint8_t ptMem[ST25R3916_PTM_A_LEN] = {};
  memcpy(ptMem, uid, uidLen);
  const bool type2 = config_.hfCardType == NetworkRfidHfCardType::NfcAType2;
  const uint8_t atqa0 = (type2 && uidLen == 7U) ? 0x44U : 0x04U;
  const uint8_t finalSak = type2 ? 0x00U : 0x20U;
  ptMem[10] = atqa0;
  ptMem[11] = 0x00;
  ptMem[12] = (uidLen == 4U) ? finalSak : static_cast<uint8_t>(finalSak | 0x04U);
  ptMem[13] = finalSak;
  ptMem[14] = finalSak;

  ReturnCode err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);
  if (err == ERR_NONE) {
    hfReader_->st25r3916DisableInterrupts(ST25R3916_IRQ_MASK_ALL);
    hfReader_->st25r3916ClearInterrupts();
    err = hfReader_->st25r3916ChangeRegisterBits(ST25R3916_REG_AUX,
                                                 ST25R3916_REG_AUX_nfc_id_mask,
                                                 uidLen == 4U ? ST25R3916_REG_AUX_nfc_id_4bytes :
                                                               ST25R3916_REG_AUX_nfc_id_7bytes);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WritePTMem(ptMem, sizeof(ptMem));
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_RX_CONF2,
                                            ST25R3916_REG_RX_CONF2_agc6_3 |
                                              ST25R3916_REG_RX_CONF2_agc_m |
                                              ST25R3916_REG_RX_CONF2_agc_en |
                                              ST25R3916_REG_RX_CONF2_sqm_dyn);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_RX_CONF3, 0x00);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_RX_CONF4, 0x00);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_CORR_CONF1,
                                            ST25R3916_REG_CORR_CONF1_corr_s0 |
                                              ST25R3916_REG_CORR_CONF1_corr_s4 |
                                              ST25R3916_REG_CORR_CONF1_corr_s6);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_CORR_CONF2, 0x00);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_MASK_RX_TIMER, 0x02);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_PASSIVE_TARGET,
                                            ST25R3916_REG_PASSIVE_TARGET_fdel_2 |
                                              ST25R3916_REG_PASSIVE_TARGET_fdel_0 |
                                              ST25R3916_REG_PASSIVE_TARGET_d_ac_ap2p |
                                              ST25R3916_REG_PASSIVE_TARGET_d_212_424_1r);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916ClrRegisterBits(ST25R3916_REG_ISO14443A_NFC,
                                              ST25R3916_REG_ISO14443A_NFC_no_tx_par |
                                                ST25R3916_REG_ISO14443A_NFC_no_rx_par |
                                                ST25R3916_REG_ISO14443A_NFC_nfc_f0);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_MODE,
                                            ST25R3916_REG_MODE_targ_targ |
                                              ST25R3916_REG_MODE_om_targ_nfca);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916WriteRegister(ST25R3916_REG_OP_CONTROL,
                                            ST25R3916_REG_OP_CONTROL_en |
                                              ST25R3916_REG_OP_CONTROL_rx_en |
                                              ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);
  }
  if (err == ERR_NONE) {
    err = hfReader_->rfalSetAnalogConfig(RFAL_ANALOG_CONFIG_TECH_CHIP |
                                         RFAL_ANALOG_CONFIG_CHIP_LISTEN_ON);
  }
  if (err == ERR_NONE) {
    hfReader_->st25r3916ClearAndEnableInterrupts(kHfCardDirectIrqs);
    err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SENSE);
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
  }

  if (err != ERR_NONE) {
    if (console_ != nullptr) {
      console_->print(F("HF card direct listener failed: "));
      console_->println(static_cast<int>(err));
    }
    return false;
  }

  uint8_t status = 0;
  if (hfReader_->st25r3916ReadRegister(ST25R3916_REG_PASSIVE_TARGET_STATUS, &status) == ERR_NONE) {
    logHfCardDirectState(status);
  }
  return true;
}

void NetworkRfidReader::serviceHfCardDirectState() {
  if (hfReader_ == nullptr) {
    return;
  }

  uint8_t status = 0;
  if (hfReader_->st25r3916ReadRegister(ST25R3916_REG_PASSIVE_TARGET_STATUS, &status) == ERR_NONE) {
    logHfCardDirectState(status);
  }

  const uint32_t irqs = hfReader_->st25r3916GetInterrupt(kHfCardDirectStateIrqs);
  if (irqs == 0U) {
    return;
  }

  const bool fieldOn = hfReader_->rfalIsExtFieldOn();
  const bool timeCritical = (irqs & (ST25R3916_IRQ_MASK_EON |
                                      ST25R3916_IRQ_MASK_NFCT |
                                      ST25R3916_IRQ_MASK_RXE_PTA |
                                      ST25R3916_IRQ_MASK_WU_A |
                                      ST25R3916_IRQ_MASK_WU_A_X)) != 0U;
  if (!timeCritical && console_ != nullptr) {
    console_->print(F("HF card direct irq=0x"));
    console_->print(irqs, HEX);
    console_->print(F(" state="));
    console_->print(hfPtaStateName(status));
    console_->print(F(" field="));
    console_->println(fieldOn ? F("on") : F("off"));
  }

  if ((irqs & ST25R3916_IRQ_MASK_EOF) != 0U) {
    if (!fieldOn) {
      hfCardIsoDepActive_ = false;
      hfCardIsoDepStartMs_ = 0;
      hfCardLastTxLen_ = 0;
      hfCardDirectRxArmed_ = false;
      hfReader_->st25r3916ChangeRegisterBits(ST25R3916_REG_OP_CONTROL,
                                             ST25R3916_REG_OP_CONTROL_en |
                                               ST25R3916_REG_OP_CONTROL_rx_en |
                                               ST25R3916_REG_OP_CONTROL_en_fd_mask,
                                             ST25R3916_REG_OP_CONTROL_en |
                                               ST25R3916_REG_OP_CONTROL_rx_en |
                                               ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
      hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_STOP);
      hfReader_->st25r3916ClrRegisterBits(ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
      hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SENSE);
      hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
      hfLastLmState_ = RFAL_LM_STATE_POWER_OFF;
      return;
    }
  }

  if ((irqs & ST25R3916_IRQ_MASK_EON) != 0U) {
    hfReader_->st25r3916ChangeRegisterBits(ST25R3916_REG_OP_CONTROL,
                                           ST25R3916_REG_OP_CONTROL_en |
                                             ST25R3916_REG_OP_CONTROL_rx_en |
                                             ST25R3916_REG_OP_CONTROL_en_fd_mask,
                                           ST25R3916_REG_OP_CONTROL_en |
                                             ST25R3916_REG_OP_CONTROL_rx_en |
                                             ST25R3916_REG_OP_CONTROL_en_fd_auto_efd);
    hfReader_->st25r3916ClrRegisterBits(ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_GOTO_SENSE);
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
    hfCardDirectRxArmed_ = false;
    hfLastLmState_ = RFAL_LM_STATE_IDLE;
  }

  if ((irqs & ST25R3916_IRQ_MASK_RXE_PTA) != 0U) {
    if (hfReader_->st25r3916ReadRegister(ST25R3916_REG_PASSIVE_TARGET_STATUS, &status) == ERR_NONE) {
      logHfCardDirectState(status);
    }
  }

  if ((irqs & (ST25R3916_IRQ_MASK_WU_A | ST25R3916_IRQ_MASK_WU_A_X)) != 0U) {
    hfReader_->st25r3916SetBitrate(RFAL_BR_106, RFAL_BR_106);
    hfReader_->st25r3916SetRegisterBits(ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    hfReader_->st25r3916GetInterrupt(kHfCardRawErrorIrqs | ST25R3916_IRQ_MASK_RXE);
    enableHfCardDirectRx();
    hfLastLmState_ = ((irqs & ST25R3916_IRQ_MASK_WU_A_X) != 0U) ? RFAL_LM_STATE_ACTIVE_Ax : RFAL_LM_STATE_ACTIVE_A;
  }

  if ((hfLastLmState_ == RFAL_LM_STATE_ACTIVE_A || hfLastLmState_ == RFAL_LM_STATE_ACTIVE_Ax) &&
      !hfCardDirectRxArmed_) {
    hfReader_->st25r3916SetBitrate(RFAL_BR_106, RFAL_BR_106);
    hfReader_->st25r3916SetRegisterBits(ST25R3916_REG_PASSIVE_TARGET, ST25R3916_REG_PASSIVE_TARGET_d_106_ac_a);
    hfReader_->st25r3916GetInterrupt(kHfCardRawErrorIrqs | ST25R3916_IRQ_MASK_RXE);
    enableHfCardDirectRx();
  }
}

void NetworkRfidReader::enableHfCardDirectRx() {
  if (hfReader_ == nullptr) {
    return;
  }
  hfCardDirectRxArmed_ = true;
  hfReader_->st25r3916EnableInterrupts(kHfCardDirectIrqs);
  hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
}

void NetworkRfidReader::logHfCardDirectState(uint8_t status) {
  const uint8_t state = status & ST25R3916_REG_PASSIVE_TARGET_STATUS_pta_state_mask;
  if (state == hfCardLastPtState_) {
    return;
  }
  hfCardLastPtState_ = state;
  const rfalLmState mapped = hfPtaToLmState(state);
  if (mapped != RFAL_LM_STATE_NOT_INIT) {
    hfLastLmState_ = mapped;
  }
  if (mapped == RFAL_LM_STATE_READY_A ||
      mapped == RFAL_LM_STATE_READY_Ax ||
      mapped == RFAL_LM_STATE_ACTIVE_A ||
      mapped == RFAL_LM_STATE_ACTIVE_Ax) {
    return;
  }
  if (console_ != nullptr) {
    console_->print(F("HF card direct state="));
    console_->print(hfPtaStateName(state));
    console_->print(F(" raw=0x"));
    console_->println(status, HEX);
  }
}

void NetworkRfidReader::resetHfCardProtocol() {
  hfLastLmState_ = RFAL_LM_STATE_NOT_INIT;
  hfCardLastPtState_ = 0xFF;
  hfListenRxBits_ = 0;
  hfCardDirectRxArmed_ = false;
  hfCardIsoDepActive_ = false;
  hfCardIsoDepStartMs_ = 0;
  hfCardExpectedBlock_ = 0;
  hfCardSelectedFile_ = kT4tFileNone;
  hfCardTxLen_ = 0;
  hfCardLastTxLen_ = 0;
}

bool NetworkRfidReader::handleHfCardListenFrame(const uint8_t* data, size_t length) {
  if (data == nullptr || length < 2U || data[0] != 0xE0U) {
    return false;
  }

  const uint8_t ats[] = {
    0x05,  // TL
    0x78,  // FSCI=8, TA/TB/TC present
    0x00,  // 106 kbps only
    0x80,  // FWI=8, SFGI=0
    0x00,  // no DID/NAD
  };

  hfCardIsoDepActive_ = true;
  hfCardIsoDepStartMs_ = millis();
  hfCardExpectedBlock_ = 0;
  hfCardSelectedFile_ = kT4tFileNone;
  hfLastLmState_ = RFAL_LM_STATE_CARDEMU_4A;

  const bool sent = sendHfCardIsoDepFrame(ats, sizeof(ats), true);
  if (sent && console_ != nullptr) {
    console_->print(F("HF card RATS -> ATS field="));
    console_->println(hfReader_->rfalIsExtFieldOn() ? F("on") : F("off"));
    console_->print(F("HF card RATS="));
    console_->println(hexBytes(data, length, true));
    uint8_t status = 0;
    if (hfReader_->st25r3916ReadRegister(ST25R3916_REG_PASSIVE_TARGET_STATUS, &status) == ERR_NONE) {
      console_->print(F("HF card direct state="));
      console_->print(hfPtaStateName(status));
      console_->print(F(" raw=0x"));
      console_->println(status, HEX);
    }
  }
  return sent;
}

void NetworkRfidReader::serviceHfCardIsoDep() {
  size_t rx_bytes = 0;
  const ReturnCode ret = pollHfCardRawFrame(rx_bytes);
  if (ret == ERR_BUSY) {
    if (hfCardIsoDepStartMs_ != 0U && (millis() - hfCardIsoDepStartMs_) > kHfCardIsoDepTimeoutMs) {
      if (console_ != nullptr) {
        console_->println(F("HF card ISO-DEP raw RX timeout"));
      }
      restartHfCardEmulation();
    }
    return;
  }

  if (ret != ERR_NONE) {
    if (ret == ERR_INCOMPLETE_BYTE || ret == ERR_CRC || ret == ERR_PAR || ret == ERR_FRAMING) {
      if (console_ != nullptr) {
        console_->print(F("HF card ISO-DEP raw RX discarded: "));
        console_->println(static_cast<int>(ret));
      }
      hfReader_->st25r3916EnableInterrupts(kHfCardRawIrqs);
      hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
      return;
    }
    if (console_ != nullptr && ret != ERR_TIMEOUT) {
      console_->print(F("HF card ISO-DEP raw RX stopped: "));
      console_->println(static_cast<int>(ret));
    }
    restartHfCardEmulation();
    return;
  }

  hfCardIsoDepStartMs_ = millis();
  const bool handled = handleHfCardIsoDepFrame(hfListenRxBuf_, rx_bytes);
  if (handled && console_ != nullptr) {
    console_->print(F("HF card ISO-DEP RX="));
    console_->println(hexBytes(hfListenRxBuf_, rx_bytes, true));
  }
  if (!handled) {
    restartHfCardEmulation();
  }
}

void NetworkRfidReader::serviceHfCardType2() {
  size_t rx_bytes = 0;
  const ReturnCode ret = pollHfCardRawFrame(rx_bytes);
  if (ret == ERR_BUSY) {
    return;
  }

  if (ret != ERR_NONE) {
    if (ret == ERR_INCOMPLETE_BYTE || ret == ERR_CRC || ret == ERR_PAR || ret == ERR_FRAMING) {
      if (console_ != nullptr) {
        console_->print(F("HF card Type 2 RX discarded: "));
        console_->println(static_cast<int>(ret));
      }
      enableHfCardDirectRx();
      return;
    }
    if (console_ != nullptr && ret != ERR_TIMEOUT) {
      console_->print(F("HF card Type 2 RX stopped: "));
      console_->println(static_cast<int>(ret));
    }
    enableHfCardDirectRx();
    return;
  }

  if (!handleHfCardType2Frame(hfListenRxBuf_, rx_bytes)) {
    if (console_ != nullptr) {
      console_->print(F("HF card Type 2 unsupported RX="));
      console_->println(hexBytes(hfListenRxBuf_, rx_bytes, true));
    }
    restartHfCardEmulation();
  }
}

bool NetworkRfidReader::handleHfCardIsoDepFrame(const uint8_t* frame, size_t length) {
  if (frame == nullptr || length == 0U) {
    return false;
  }

  const uint8_t pcb = frame[0];

  if ((pcb & kIsoDepPcbTypeMask) == 0xC0U) {
    if ((pcb & 0xF0U) == (kIsoDepPcbWtx & 0xF0U)) {
      const uint8_t wtxm = (length > 1U) ? frame[1] : 1U;
      uint8_t response[] = {kIsoDepPcbWtx, wtxm};
      return sendHfCardIsoDepFrame(response, sizeof(response), true);
    }

    uint8_t response[] = {kIsoDepPcbDeselect};
    sendHfCardIsoDepFrame(response, sizeof(response), false);
    hfCardEmulationActive_ = false;
    resetHfCardProtocol();
    lastHfCardEmuAttemptMs_ = 0;
    startHfCardEmulation();
    return true;
  }

  if ((pcb & kIsoDepPcbRBlockMask) == 0x80U) {
    if (hfCardLastTxLen_ > 0U) {
      return sendHfCardIsoDepFrame(hfCardLastTxBuf_, hfCardLastTxLen_, true);
    }
    uint8_t rack[] = {static_cast<uint8_t>(kIsoDepPcbRBlock | (hfCardExpectedBlock_ & kIsoDepPcbBlockNumber))};
    return sendHfCardIsoDepFrame(rack, sizeof(rack), true);
  }

  if ((pcb & kIsoDepPcbTypeMask) != 0x00U) {
    return false;
  }

  const uint8_t rxBlock = (pcb & kIsoDepPcbBlockNumber);
  if (rxBlock != hfCardExpectedBlock_ && hfCardLastTxLen_ > 0U) {
    return sendHfCardIsoDepFrame(hfCardLastTxBuf_, hfCardLastTxLen_, true);
  }

  size_t infPos = 1;
  if ((pcb & 0x08U) != 0U) {
    infPos++;
  }
  if ((pcb & 0x04U) != 0U) {
    infPos++;
  }
  if (infPos > length) {
    return false;
  }

  if ((pcb & kIsoDepPcbChaining) != 0U) {
    hfCardExpectedBlock_ ^= 1U;
    uint8_t rack[] = {static_cast<uint8_t>(kIsoDepPcbRBlock | (hfCardExpectedBlock_ & kIsoDepPcbBlockNumber))};
    return sendHfCardIsoDepFrame(rack, sizeof(rack), true);
  }

  const size_t apduLen = length - infPos;
  const size_t respLen = buildHfT4tApduResponse(&frame[infPos], apduLen, &hfCardTxBuf_[1], sizeof(hfCardTxBuf_) - 1U);
  if (respLen == 0U) {
    return false;
  }

  hfCardTxBuf_[0] = static_cast<uint8_t>(kIsoDepPcbIBlock | rxBlock);
  hfCardExpectedBlock_ = rxBlock ^ 1U;
  return sendHfCardIsoDepFrame(hfCardTxBuf_, respLen + 1U, true);
}

bool NetworkRfidReader::handleHfCardType2Frame(const uint8_t* frame, size_t length) {
  if (frame == nullptr || length == 0U) {
    return false;
  }

  const uint8_t cmd = frame[0];
  if (cmd == 0x30U && length >= 2U) {
    const size_t offset = static_cast<size_t>(frame[1]) * 4U;
    uint8_t response[16] = {};
    for (size_t i = 0; i < sizeof(response); ++i) {
      const size_t pos = offset + i;
      response[i] = (pos < sizeof(hfCardT2tMemory_)) ? hfCardT2tMemory_[pos] : 0x00U;
    }
    return sendHfCardType2FrameFast(response, sizeof(response), true);
  }

  if (cmd == 0x3AU && length >= 3U) {
    const uint8_t startPage = frame[1];
    const uint8_t endPage = frame[2];
    if (endPage < startPage) {
      return false;
    }

    size_t responseLen = (static_cast<size_t>(endPage) - startPage + 1U) * 4U;
    const size_t maxResponseLen = (sizeof(hfCardTxBuf_) - 2U) & ~0x03U;
    if (responseLen > maxResponseLen) {
      responseLen = maxResponseLen;
    }
    if (responseLen == 0U) {
      return false;
    }

    const size_t offset = static_cast<size_t>(startPage) * 4U;
    for (size_t i = 0; i < responseLen; ++i) {
      const size_t pos = offset + i;
      hfCardTxBuf_[i] = (pos < sizeof(hfCardT2tMemory_)) ? hfCardT2tMemory_[pos] : 0x00U;
    }
    return sendHfCardType2FrameFast(hfCardTxBuf_, responseLen, true);
  }

  if (cmd == 0x60U) {
    const uint8_t version[] = {
      0x00, 0x04, 0x04, 0x02,
      0x01, 0x00, 0x0F, 0x03,
    };
    return sendHfCardType2FrameFast(version, sizeof(version), true);
  }

  if (cmd == 0x1AU) {
    restartHfCardEmulation();
    return true;
  }

  if (cmd == 0x50U && length >= 2U) {
    restartHfCardEmulation();
    return true;
  }

  return false;
}

bool NetworkRfidReader::sendHfCardIsoDepFrame(const uint8_t* frame, size_t length, bool expectRx) {
  if (hfReader_ == nullptr || frame == nullptr || length == 0U || length > (sizeof(hfCardTxBuf_) - 2U)) {
    return false;
  }

  memcpy(hfCardLastTxBuf_, frame, length);
  hfCardLastTxLen_ = static_cast<uint16_t>(length);
  hfListenRxBits_ = 0;

  memmove(hfCardTxBuf_, frame, length);
  const uint16_t crc = iso14443aCrc(hfCardTxBuf_, length);
  hfCardTxBuf_[length] = static_cast<uint8_t>(crc & 0xffU);
  hfCardTxBuf_[length + 1U] = static_cast<uint8_t>((crc >> 8) & 0xffU);
  const size_t txLength = length + 2U;

  ReturnCode err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916ChangeRegisterBits(ST25R3916_REG_ISO14443A_NFC,
                                                 ST25R3916_REG_ISO14443A_NFC_no_tx_par |
                                                   ST25R3916_REG_ISO14443A_NFC_no_rx_par |
                                                   ST25R3916_REG_ISO14443A_NFC_nfc_f0,
                                                 ST25R3916_REG_ISO14443A_NFC_no_tx_par_off |
                                                   ST25R3916_REG_ISO14443A_NFC_no_rx_par_off |
                                                   ST25R3916_REG_ISO14443A_NFC_nfc_f0_off);
  }
  if (err == ERR_NONE) {
    hfReader_->st25r3916SetNumTxBits(static_cast<uint16_t>(txLength * 8U));
    hfReader_->st25r3916ClearAndEnableInterrupts(kHfCardRawIrqs);
    err = hfReader_->st25r3916WriteFifo(hfCardTxBuf_, static_cast<uint16_t>(txLength));
  }
  if (err == ERR_NONE) {
    delayMicroseconds(kHfCardFdtListenUs);
    err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
  }
  if (err != ERR_NONE) {
    if (console_ != nullptr) {
      console_->print(F("HF card raw TX start failed: "));
      console_->println(static_cast<int>(err));
    }
    return false;
  }

  const uint32_t tx_irqs = hfReader_->st25r3916WaitForInterruptsTimed(ST25R3916_IRQ_MASK_TXE |
                                                                        kHfCardRawErrorIrqs |
                                                                        ST25R3916_IRQ_MASK_EOF,
                                                                      kHfCardRawTxTimeoutMs);
  if ((tx_irqs & kHfCardRawErrorIrqs) != 0U) {
    if (console_ != nullptr) {
      console_->print(F("HF card raw TX error irq=0x"));
      console_->println(tx_irqs, HEX);
    }
    return false;
  }
  if ((tx_irqs & ST25R3916_IRQ_MASK_TXE) == 0U) {
    if (console_ != nullptr) {
      console_->print(F("HF card raw TX timeout irq=0x"));
      console_->println(tx_irqs, HEX);
    }
    return false;
  }

  if (expectRx) {
    hfReader_->st25r3916EnableInterrupts(kHfCardRawIrqs);
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_UNMASK_RECEIVE_DATA);
  }
  hfCardIsoDepStartMs_ = expectRx ? millis() : 0U;
  return true;
}

bool NetworkRfidReader::sendHfCardType2FrameFast(const uint8_t* frame, size_t length, bool expectRx) {
  if (hfReader_ == nullptr || frame == nullptr || length == 0U || length > (sizeof(hfCardTxBuf_) - 2U)) {
    return false;
  }

  memmove(hfCardTxBuf_, frame, length);
  const uint16_t crc = iso14443aCrc(hfCardTxBuf_, length);
  hfCardTxBuf_[length] = static_cast<uint8_t>(crc & 0xffU);
  hfCardTxBuf_[length + 1U] = static_cast<uint8_t>((crc >> 8) & 0xffU);
  const size_t txLength = length + 2U;
  hfListenRxBits_ = 0;

  ReturnCode err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
  if (err == ERR_NONE) {
    hfReader_->st25r3916SetNumTxBits(static_cast<uint16_t>(txLength * 8U));
    err = hfReader_->st25r3916WriteFifo(hfCardTxBuf_, static_cast<uint16_t>(txLength));
  }
  if (err == ERR_NONE) {
    err = hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);
  }
  if (err != ERR_NONE) {
    return false;
  }

  const uint32_t tx_irqs = hfReader_->st25r3916WaitForInterruptsTimed(ST25R3916_IRQ_MASK_TXE |
                                                                        kHfCardRawErrorIrqs |
                                                                        ST25R3916_IRQ_MASK_EOF,
                                                                      kHfCardRawTxTimeoutMs);
  if ((tx_irqs & (kHfCardRawErrorIrqs | ST25R3916_IRQ_MASK_TXE)) != ST25R3916_IRQ_MASK_TXE) {
    return false;
  }

  if (expectRx) {
    enableHfCardDirectRx();
  }
  return true;
}

ReturnCode NetworkRfidReader::pollHfCardRawFrame(size_t& rxBytes) {
  rxBytes = 0;
  if (hfReader_ == nullptr) {
    return ERR_WRONG_STATE;
  }

  const uint32_t irqs = hfReader_->st25r3916GetInterrupt(kHfCardRawIrqs);
  if (irqs == 0U) {
    return ERR_BUSY;
  }

  if ((irqs & kHfCardRawErrorIrqs) != 0U) {
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
    if ((irqs & ST25R3916_IRQ_MASK_CRC) != 0U) {
      return ERR_CRC;
    }
    if ((irqs & ST25R3916_IRQ_MASK_PAR) != 0U) {
      return ERR_PAR;
    }
    return ERR_FRAMING;
  }

  if ((irqs & ST25R3916_IRQ_MASK_RXE) == 0U) {
    return ERR_BUSY;
  }

  const uint16_t fifoBytes = hfReader_->st25r3916GetNumFIFOBytes();
  const uint8_t lastBits = hfReader_->st25r3916GetNumFIFOLastBits();
  if (lastBits != 0U) {
    uint8_t partial[4] = {};
    const uint16_t partialBytes = static_cast<uint16_t>((fifoBytes < sizeof(partial)) ? fifoBytes : sizeof(partial));
    if (partialBytes > 0U) {
      hfReader_->st25r3916ReadFifo(partial, partialBytes);
    }
    if (console_ != nullptr) {
      console_->print(F("HF card raw RX incomplete fifo="));
      console_->print(fifoBytes);
      console_->print(F(" lastBits="));
      console_->print(lastBits);
      if (partialBytes > 0U) {
        console_->print(F(" data="));
        console_->print(hexBytes(partial, partialBytes, true));
      }
      console_->println();
    }
    hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
    return ERR_INCOMPLETE_BYTE;
  }

  if (fifoBytes == 0U) {
    return ERR_BUSY;
  }

  const uint16_t readBytes = static_cast<uint16_t>(min(static_cast<size_t>(fifoBytes), sizeof(hfListenRxBuf_)));
  ReturnCode err = hfReader_->st25r3916ReadFifo(hfListenRxBuf_, readBytes);
  hfReader_->st25r3916ExecuteCommand(ST25R3916_CMD_CLEAR_FIFO);
  if (err != ERR_NONE) {
    return err;
  }
  if (fifoBytes > sizeof(hfListenRxBuf_)) {
    return ERR_NOMEM;
  }

  rxBytes = readBytes;
  if (rxBytes >= 2U) {
    rxBytes -= 2U;
  }
  hfListenRxBits_ = static_cast<uint16_t>(rxBytes * 8U);
  return (rxBytes > 0U) ? ERR_NONE : ERR_TIMEOUT;
}

void NetworkRfidReader::restartHfCardEmulation() {
  hfCardEmulationActive_ = false;
  resetHfCardProtocol();
  lastHfCardEmuAttemptMs_ = 0;
  startHfCardEmulation();
}

size_t NetworkRfidReader::buildHfNdefMessage(uint8_t* out, size_t maxLength) const {
  if (out == nullptr || maxLength == 0U) {
    return 0;
  }

  size_t pos = 0;
  if (config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Url) {
    const String payload = config_.hfCardPayload.length() > 0 ? config_.hfCardPayload : String(F("https://www.elechouse.com/"));
    const size_t payloadLen = 1U + static_cast<size_t>(payload.length());
    if (!appendNdefHeader(out, maxLength, pos, 0x01, 1, payloadLen) ||
        !appendByte(out, maxLength, pos, 'U') ||
        !appendByte(out, maxLength, pos, 0x00) ||
        !appendStringBytes(out, maxLength, pos, payload)) {
      return 0;
    }
    return pos;
  }

  if (config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Vcard) {
    const char mime[] = "text/vcard";
    const size_t payloadLen = static_cast<size_t>(config_.hfCardPayload.length());
    if (!appendNdefHeader(out, maxLength, pos, 0x02, static_cast<uint8_t>(strlen(mime)), payloadLen) ||
        !appendBytes(out, maxLength, pos, reinterpret_cast<const uint8_t*>(mime), strlen(mime)) ||
        !appendStringBytes(out, maxLength, pos, config_.hfCardPayload)) {
      return 0;
    }
    return pos;
  }

  String payload;
  if (config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Wifi) {
    payload = F("WIFI:T:WPA;S:");
    payload += config_.hfCardWifiSsid;
    payload += F(";P:");
    payload += config_.hfCardWifiPassword;
    payload += F(";;");
  } else {
    payload = config_.hfCardPayload;
  }

  const size_t payloadLen = 3U + static_cast<size_t>(payload.length());
  if (!appendNdefHeader(out, maxLength, pos, 0x01, 1, payloadLen) ||
      !appendByte(out, maxLength, pos, 'T') ||
      !appendByte(out, maxLength, pos, 0x02) ||
      !appendByte(out, maxLength, pos, 'e') ||
      !appendByte(out, maxLength, pos, 'n') ||
      !appendStringBytes(out, maxLength, pos, payload)) {
    return 0;
  }
  return pos;
}

size_t NetworkRfidReader::buildHfNdefFile(uint8_t* out, size_t maxLength) const {
  if (out == nullptr || maxLength < 2U) {
    return 0;
  }

  const size_t ndefLen = buildHfNdefMessage(&out[2], maxLength - 2U);
  if (ndefLen == 0U || ndefLen > 0xFFFEU) {
    return 0;
  }

  out[0] = static_cast<uint8_t>((ndefLen >> 8) & 0xFFU);
  out[1] = static_cast<uint8_t>(ndefLen & 0xFFU);
  return ndefLen + 2U;
}

size_t NetworkRfidReader::buildHfT2tMemory(uint8_t* out, size_t maxLength) const {
  if (out == nullptr || maxLength < 20U || (maxLength % 4U) != 0U) {
    return 0;
  }

  uint8_t uid[RFAL_NFCID1_TRIPLE_LEN] = {};
  size_t uidLen = 0;
  if (!parseHexBytes(config_.hfCardUid, uid, sizeof(uid), uidLen) ||
      (uidLen != 4U && uidLen != 7U)) {
    return 0;
  }

  memset(out, 0, maxLength);
  if (uidLen == 7U) {
    out[0] = uid[0];
    out[1] = uid[1];
    out[2] = uid[2];
    out[3] = static_cast<uint8_t>(0x88U ^ uid[0] ^ uid[1] ^ uid[2]);
    out[4] = uid[3];
    out[5] = uid[4];
    out[6] = uid[5];
    out[7] = uid[6];
    out[8] = static_cast<uint8_t>(uid[3] ^ uid[4] ^ uid[5] ^ uid[6]);
  } else {
    out[0] = uid[0];
    out[1] = uid[1];
    out[2] = uid[2];
    out[3] = static_cast<uint8_t>(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]);
    out[4] = uid[3];
    out[8] = uid[3];
  }
  out[9] = 0x48;
  out[10] = 0x00;
  out[11] = 0x00;

  const size_t dataAreaLen = maxLength - 16U;
  const size_t ccSize = dataAreaLen / 8U;
  out[12] = 0xE1;
  out[13] = 0x10;
  out[14] = static_cast<uint8_t>(ccSize > 0xFFU ? 0xFFU : ccSize);
  out[15] = 0x00;

  uint8_t ndef[HfT2tMemoryMaxLen] = {};
  const size_t ndefLen = buildHfNdefMessage(ndef, sizeof(ndef));
  if (ndefLen == 0U || ndefLen > 0xFFFEU) {
    return 0;
  }

  size_t pos = 16U;
  if (ndefLen <= 0xFEU) {
    if ((pos + 2U + ndefLen + 1U) > maxLength) {
      return 0;
    }
    out[pos++] = 0x03;
    out[pos++] = static_cast<uint8_t>(ndefLen);
  } else {
    if ((pos + 4U + ndefLen + 1U) > maxLength) {
      return 0;
    }
    out[pos++] = 0x03;
    out[pos++] = 0xFF;
    out[pos++] = static_cast<uint8_t>((ndefLen >> 8) & 0xFFU);
    out[pos++] = static_cast<uint8_t>(ndefLen & 0xFFU);
  }

  memcpy(&out[pos], ndef, ndefLen);
  pos += ndefLen;
  out[pos] = 0xFE;
  return maxLength;
}

size_t NetworkRfidReader::buildHfT4tApduResponse(const uint8_t* apdu, size_t apduLen, uint8_t* out, size_t maxLength) {
  if (apdu == nullptr || out == nullptr || maxLength < 2U || apduLen < 2U) {
    return appendStatus(out, maxLength, 0, kT4tSwWrongParams);
  }

  const uint8_t ins = apdu[1];
  if (ins == 0xA4U) {
    if (apduLen < 5U) {
      return appendStatus(out, maxLength, 0, kT4tSwWrongParams);
    }

    const uint8_t p1 = apdu[2];
    const uint8_t lc = apdu[4];
    if (apduLen < (5U + lc)) {
      return appendStatus(out, maxLength, 0, kT4tSwWrongParams);
    }

    const uint8_t* data = &apdu[5];
    if (p1 == 0x04U) {
      if (lc == sizeof(kT4tNdefAid) && memcmp(data, kT4tNdefAid, sizeof(kT4tNdefAid)) == 0) {
        hfCardSelectedFile_ = kT4tFileNone;
        return appendStatus(out, maxLength, 0, kT4tSwSuccess);
      }
      return appendStatus(out, maxLength, 0, kT4tSwFileNotFound);
    }

    if (lc == 2U) {
      const uint16_t fileId = (static_cast<uint16_t>(data[0]) << 8) | data[1];
      if (fileId == kT4tCcFileId) {
        hfCardSelectedFile_ = kT4tFileCc;
        return appendStatus(out, maxLength, 0, kT4tSwSuccess);
      }
      if (fileId == kT4tNdefFileId) {
        hfCardSelectedFile_ = kT4tFileNdef;
        return appendStatus(out, maxLength, 0, kT4tSwSuccess);
      }
    }
    return appendStatus(out, maxLength, 0, kT4tSwFileNotFound);
  }

  if (ins == 0xB0U) {
    if (apduLen < 5U) {
      return appendStatus(out, maxLength, 0, kT4tSwWrongParams);
    }

    const uint16_t offset = (static_cast<uint16_t>(apdu[2] & 0x7FU) << 8) | apdu[3];
    uint16_t le = apdu[4];
    if (le == 0U) {
      le = 256U;
    }

    const uint8_t* source = nullptr;
    size_t sourceLen = 0;
    if (hfCardSelectedFile_ == kT4tFileCc) {
      source = kT4tCcFile;
      sourceLen = sizeof(kT4tCcFile);
    } else if (hfCardSelectedFile_ == kT4tFileNdef) {
      sourceLen = buildHfNdefFile(hfCardNdefFile_, sizeof(hfCardNdefFile_));
      source = hfCardNdefFile_;
    } else {
      return appendStatus(out, maxLength, 0, kT4tSwConditionsNotSatisfied);
    }

    if (source == nullptr || sourceLen == 0U || offset > sourceLen) {
      return appendStatus(out, maxLength, 0, kT4tSwWrongParams);
    }

    size_t available = sourceLen - offset;
    size_t chunk = le < available ? le : available;
    const size_t room = maxLength - 2U;
    if (chunk > room) {
      chunk = room;
    }
    if (chunk > kT4tMaxReadChunk) {
      chunk = kT4tMaxReadChunk;
    }

    memcpy(out, &source[offset], chunk);
    return appendStatus(out, maxLength, chunk, kT4tSwSuccess);
  }

  return appendStatus(out, maxLength, 0, kT4tSwInsNotSupported);
}

bool NetworkRfidReader::beginHfDataExchange(rfalNfcDevice* device) {
  if (nfc_ == nullptr || device == nullptr || !isHfP2pDevice(device)) {
    return false;
  }

  String message = config_.hfP2pMessage;
  if (message.length() == 0) {
    message = F("ELECHOUSE RFID P2P");
  }

  const size_t tx_len = static_cast<size_t>(message.length()) > sizeof(hfP2pTxBuf_)
                            ? sizeof(hfP2pTxBuf_)
                            : static_cast<size_t>(message.length());
  memcpy(hfP2pTxBuf_, message.c_str(), tx_len);

  uint8_t* rx_data = nullptr;
  uint16_t* rx_len = nullptr;
  const ReturnCode err = nfc_->rfalNfcDataExchangeStart(hfP2pTxBuf_,
                                                        static_cast<uint16_t>(tx_len),
                                                        &rx_data,
                                                        &rx_len,
                                                        RFAL_FWT_NONE);
  if (err != ERR_NONE) {
    if (console_ != nullptr) {
      console_->print(F("HF P2P exchange start failed: "));
      console_->println(static_cast<int>(err));
    }
    return false;
  }

  hfDataExchangeActive_ = true;
  hfDiscoveryActive_ = false;
  hfExchangeRxData_ = rx_data;
  hfExchangeRxLen_ = rx_len;
  hfDataExchangeStartMs_ = millis();
  if (console_ != nullptr) {
    console_->print(F("HF P2P tx="));
    console_->println(message);
  }
  return true;
}

void NetworkRfidReader::serviceHfDataExchange() {
  if (!hfDataExchangeActive_ || nfc_ == nullptr) {
    return;
  }

  const ReturnCode err = nfc_->rfalNfcDataExchangeGetStatus();
  if (err == ERR_BUSY) {
    if ((millis() - hfDataExchangeStartMs_) > 5000UL) {
      if (console_ != nullptr) {
        console_->println(F("HF P2P exchange timeout"));
      }
      nfc_->rfalNfcDeactivate(true);
      hfDataExchangeActive_ = false;
      hfDiscoveryActive_ = true;
    }
    return;
  }

  if (err == ERR_NONE && hfExchangeRxData_ != nullptr && hfExchangeRxLen_ != nullptr) {
    const uint16_t rx_len = *hfExchangeRxLen_;
    String id = hexBytes(hfExchangeRxData_, rx_len, true);
    const String ascii = asciiPreview(hfExchangeRxData_, rx_len);
    if (ascii.length() > 0) {
      id += F(" ASCII=\"");
      id += ascii;
      id += '"';
    }
    emitCard("HF", "P2P-RX", id);
  } else if (console_ != nullptr) {
    console_->print(F("HF P2P exchange err="));
    console_->println(static_cast<int>(err));
  }

  nfc_->rfalNfcDeactivate(true);
  hfDataExchangeActive_ = false;
  hfExchangeRxData_ = nullptr;
  hfExchangeRxLen_ = nullptr;
  hfDiscoveryActive_ = true;
}

void NetworkRfidReader::handleSerial() {
  if (primaryConsole_ != nullptr) {
    handleSerialStream(*primaryConsole_, serialLine_);
  }
  if (hardwareUartReady_ && config_.hardwareUartCommands) {
    handleSerialStream(hardwareUart_, hardwareUartLine_);
  }
}

void NetworkRfidReader::handleSerialStream(Stream& stream, String& line) {
  while (stream.available() > 0) {
    const int value = stream.read();
    if (value < 0) {
      return;
    }
    const char c = static_cast<char>(value);
    if (c == '\r' || c == '\n') {
      line.trim();
      if (line.length() > 0) {
        Stream* previous_console = console_;
        console_ = &stream;
        handleCommand(line);
        console_ = previous_console;
      }
      line = "";
    } else if (c == '\b' || c == 0x7F) {
      if (line.length() > 0) {
        line.remove(line.length() - 1);
      }
    } else if (c >= 32 && c <= 126) {
      if (line.length() < 220) {
        line += c;
      } else {
        line = "";
      }
    }
  }
}

void NetworkRfidReader::handleCommand(String line) {
  String rest = line;
  const String cmd = lowerCopy(nextToken(rest));

  if (cmd == "help") {
    printHelp();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "pins") {
    printPins();
  } else if (cmd == "wifi") {
    handleWifiCommand(rest);
  } else if (cmd == "tcp") {
    handleTcpCommand(rest);
  } else if (cmd == "elechouse") {
    handleElechouseCommand(rest);
  } else if (cmd == "interface") {
    handleInterfaceCommand(rest);
  } else if (cmd == "portal") {
    handlePortalCommand(rest);
  } else if (cmd == "feedback") {
    handleFeedbackCommand(rest);
  } else if (cmd == "button") {
    handleButtonCommand(rest);
  } else if (cmd == "format") {
    const String fmt = lowerCopy(nextToken(rest));
    if (fmt == "json") {
      config_.outputFormat = NetworkRfidOutputFormat::Json;
      console_->println(F("OK format=json"));
    } else if (fmt == "line") {
      config_.outputFormat = NetworkRfidOutputFormat::CsvLine;
      console_->println(F("OK format=line"));
    } else {
      console_->println(F("ERR format json|line"));
    }
  } else if (cmd == "window") {
    const uint32_t lf = nextToken(rest).toInt();
    const uint32_t hf = nextToken(rest).toInt();
    if (lf < 20 || hf < 20) {
      console_->println(F("ERR window <lf_ms> <hf_ms>, min 20"));
    } else {
      config_.lfWindowMs = lf;
      config_.hfWindowMs = hf;
      console_->println(F("OK window updated"));
    }
  } else if (cmd == "dedupe") {
    config_.duplicateSuppressMs = nextToken(rest).toInt();
    console_->println(F("OK dedupe updated"));
  } else if (cmd == "auto") {
    const String target = lowerCopy(nextToken(rest));
    const String value = lowerCopy(nextToken(rest));
    if ((target != "lf" && target != "hf") || (value != "on" && value != "off")) {
      console_->println(F("ERR auto lf|hf on|off"));
      return;
    }
    const bool enabled = value == "on";
    if (target == "lf") {
      config_.autoStartLf = enabled;
      if (enabled) {
        setActiveSlot(NetworkRfidSlot::LF);
      } else {
        setLfCapture(false);
        setLfCarrier(false);
        if (isHfEnabled()) {
          setActiveSlot(NetworkRfidSlot::HF);
        }
      }
    } else {
      config_.autoInitHf = enabled;
      if (enabled) {
        if (!hfReady_) {
          hfReady_ = setupHf();
        }
        if (hfReady_ && !isLfEnabled()) {
          setActiveSlot(NetworkRfidSlot::HF);
        }
      } else {
        releaseHf();
        if (isLfEnabled()) {
          setActiveSlot(NetworkRfidSlot::LF);
        }
      }
    }
    console_->println(F("OK auto updated"));
  } else if (cmd == "lf") {
    handleLfCommand(rest);
  } else if (cmd == "hf") {
    handleHfCommand(rest);
  } else if (cmd == "save") {
    console_->println(saveConfig() ? F("OK saved") : F("ERR save failed"));
  } else if (cmd == "load") {
    console_->println(loadConfig() ? F("OK loaded, reboot recommended") : F("ERR no saved config"));
  } else if (cmd == "clear") {
    clearSavedConfig();
    console_->println(F("OK saved config cleared"));
  } else if (cmd == "reboot") {
    console_->println(F("Rebooting"));
    delay(100);
    ESP.restart();
  } else if (cmd == "test") {
    emitCard("TEST", "SERIAL", String(F("12345678")));
  } else {
    console_->print(F("ERR unknown command: "));
    console_->println(cmd);
  }
}

void NetworkRfidReader::handleLfCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR lf init|off|status|freq <hz>|scan [start stop step ms]|raw <count>|hid [ms]|indala [samples]"));
    return;
  }
  if (sub == "status") {
    console_->print(F("LF carrier="));
    console_->print(lfCarrierEnabled_ ? F("on") : F("off"));
    console_->print(F(" freq="));
    console_->print(config_.lfCarrierHz);
    console_->print(F(" capture="));
    console_->print(lfCaptureEnabled_ ? F("on") : F("off"));
    console_->print(F(" hidBits="));
    console_->print(hidProx_.fskBitCount());
    console_->print(F(" slot="));
    console_->println(slotName(activeSlot_));
    return;
  }

  if (sub == "init") {
    config_.autoStartLf = true;
    console_->println(F("LF init starting"));
    setLfCapture(false);
    setLfCarrier(false);
    slotStartMs_ = 0;
    setActiveSlot(NetworkRfidSlot::LF);
    console_->println(F("LF started"));
    return;
  }

  if (sub == "off") {
    config_.autoStartLf = false;
    setLfCapture(false);
    setLfCarrier(false);
    if (isHfEnabled()) {
      setActiveSlot(NetworkRfidSlot::HF);
    }
    console_->println(F("LF stopped"));
    return;
  }

  if (sub == "freq") {
    const uint32_t hz = nextToken(rest).toInt();
    if (hz < 100000UL || hz > 150000UL) {
      console_->println(F("ERR lf freq <100000..150000>"));
      return;
    }
    const bool was_on = lfCarrierEnabled_;
    const bool was_capture_on = lfCaptureEnabled_;
    if (was_capture_on) {
      setLfCapture(false);
    }
    if (was_on) {
      setLfCarrier(false);
    }
    config_.lfCarrierHz = hz;
    if (was_on) {
      setLfCarrier(true);
    }
    if (was_capture_on) {
      setLfCapture(true);
    }
    console_->print(F("OK lf freq="));
    console_->println(config_.lfCarrierHz);
    return;
  }

  if (sub == "raw") {
    uint32_t count = nextToken(rest).toInt();
    if (count == 0) {
      count = 128;
    } else if (count > 512) {
      count = 512;
    }
    lfRawDumpRemaining_ = static_cast<uint16_t>(count);
    console_->print(F("OK lf raw count="));
    console_->println(lfRawDumpRemaining_);
    return;
  }

  if (sub == "scan") {
    handleLfScanCommand(rest);
    return;
  }

  if (sub == "indala") {
    handleLfIndalaCommand(rest);
    return;
  }

  if (sub == "hid") {
    handleLfHidCommand(rest);
    return;
  }

  console_->println(F("ERR lf init|off|status|freq <hz>|scan [start stop step ms]|raw <count>|hid [ms]|indala [samples]"));
}

void NetworkRfidReader::handleLfHidCommand(String rest) {
  uint32_t duration_ms = nextToken(rest).toInt();
  if (duration_ms == 0) {
    duration_ms = 2000;
  } else if (duration_ms < 500) {
    duration_ms = 500;
  } else if (duration_ms > 8000) {
    duration_ms = 8000;
  }

  setLfCapture(false);
  hidProx_.reset();
  em4100_.reset();
  resetPulseQueue();
  stopHfDiscovery();
  if (config_.pins.lfPull >= 0) {
    digitalWrite(config_.pins.lfPull, LOW);
  }
  setLfCarrier(true);
  setLfCapture(true);
  activeSlot_ = NetworkRfidSlot::LF;
  slotStartMs_ = millis();

  console_->print(F("LF HID debug ms="));
  console_->println(duration_ms);

  const uint32_t start_ms = millis();
  uint32_t last_yield_ms = start_ms;
  while ((millis() - start_ms) < duration_ms) {
    processLfHidPulses(1024);
    const uint32_t now_ms = millis();
    if (now_ms != last_yield_ms) {
      delay(0);
      last_yield_ms = now_ms;
    }
  }
  processLfHidPulses(2048);

  uint32_t captured = 0;
  uint32_t dropped = 0;
  noInterrupts();
  captured = capturedPulses_;
  dropped = droppedPulses_;
  interrupts();

  char preview[105] = {};
  hidProx_.copyEncodedPreview(preview, sizeof(preview));
  const HidPulseFilterStats filter_stats = hidProx_.pulseFilterStats();
  const FskDemodStats fsk_stats = hidProx_.fskStats();
  console_->print(F("LF HID debug pulses="));
  console_->print(captured);
  console_->print(F(" dropped="));
  console_->print(dropped);
  console_->print(F(" filtered="));
  console_->print(filter_stats.filtered_pulses);
  console_->print(F(" merged="));
  console_->print(filter_stats.merged_same_level);
  console_->print(F(" glitches="));
  console_->print(filter_stats.dropped_glitches);
  console_->print(F(" longResets="));
  console_->print(filter_stats.long_resets);
  console_->print(F(" periods="));
  console_->print(fsk_stats.accepted_periods);
  console_->print(F(" lowP="));
  console_->print(fsk_stats.low_periods);
  console_->print(F(" highP="));
  console_->print(fsk_stats.high_periods);
  console_->print(F(" shortP="));
  console_->print(fsk_stats.rejected_short);
  console_->print(F(" longP="));
  console_->print(fsk_stats.rejected_long);
  console_->print(F(" recoveredP="));
  console_->print(fsk_stats.recovered_long);
  console_->print(F(" trans="));
  console_->print(fsk_stats.transitions);
  console_->print(F(" fskBits="));
  console_->print(hidProx_.fskBitCount());
  console_->print(F(" preview="));
  console_->println(preview);
}

void NetworkRfidReader::handleLfScanCommand(String rest) {
  if (config_.pins.lfAdc < 0) {
    console_->println(F("ERR lf adc pin disabled"));
    return;
  }

  uint32_t start_hz = nextToken(rest).toInt();
  uint32_t stop_hz = nextToken(rest).toInt();
  uint32_t step_hz = nextToken(rest).toInt();
  uint32_t sample_ms = nextToken(rest).toInt();
  if (start_hz == 0) {
    start_hz = 110000UL;
  }
  if (stop_hz == 0) {
    stop_hz = 135000UL;
  }
  if (step_hz == 0) {
    step_hz = 500UL;
  }
  if (sample_ms == 0) {
    sample_ms = 60UL;
  }

  if (start_hz < 80000UL || stop_hz > 160000UL || start_hz >= stop_hz ||
      step_hz < 100UL || step_hz > 10000UL || sample_ms < 10UL || sample_ms > 500UL) {
    console_->println(F("ERR lf scan [start stop step ms], hz 80000..160000"));
    return;
  }

  const uint32_t saved_hz = config_.lfCarrierHz;
  const bool saved_capture = lfCaptureEnabled_;
  const bool saved_carrier = lfCarrierEnabled_;
  if (saved_capture) {
    setLfCapture(false);
  }
  stopHfDiscovery();
  if (config_.pins.lfPull >= 0) {
    digitalWrite(config_.pins.lfPull, LOW);
  }

  console_->print(F("LF scan start="));
  console_->print(start_hz);
  console_->print(F(" stop="));
  console_->print(stop_hz);
  console_->print(F(" step="));
  console_->print(step_hz);
  console_->print(F(" ms="));
  console_->println(sample_ms);

  uint32_t best_hz = 0;
  uint16_t best_pp = 0;
  uint16_t best_min = 0;
  uint16_t best_max = 0;
  uint32_t best_avg = 0;

  for (uint32_t hz = start_hz; hz <= stop_hz; hz += step_hz) {
    if (lfCarrierEnabled_) {
      setLfCarrier(false);
    }
    config_.lfCarrierHz = hz;
    setLfCarrier(true);
    delay(25);

    uint16_t min_adc = 4095;
    uint16_t max_adc = 0;
    uint32_t sum = 0;
    uint32_t samples = 0;
    const uint32_t start_ms = millis();
    while ((millis() - start_ms) < sample_ms) {
      const uint16_t value = static_cast<uint16_t>(analogRead(config_.pins.lfAdc));
      if (value < min_adc) {
        min_adc = value;
      }
      if (value > max_adc) {
        max_adc = value;
      }
      sum += value;
      samples++;
      delayMicroseconds(13 + ((samples & 0x07U) * 7U));
    }

    const uint16_t pp = max_adc - min_adc;
    const uint32_t avg = samples == 0 ? 0 : sum / samples;
    console_->print(F("LFSCAN hz="));
    console_->print(hz);
    console_->print(F(" min="));
    console_->print(min_adc);
    console_->print(F(" max="));
    console_->print(max_adc);
    console_->print(F(" pp="));
    console_->print(pp);
    console_->print(F(" avg="));
    console_->println(avg);

    if (pp > best_pp) {
      best_hz = hz;
      best_pp = pp;
      best_min = min_adc;
      best_max = max_adc;
      best_avg = avg;
    }
  }

  if (lfCarrierEnabled_) {
    setLfCarrier(false);
  }
  config_.lfCarrierHz = saved_hz;
  if (saved_carrier || saved_capture) {
    setLfCarrier(true);
  }
  if (saved_capture) {
    setLfCapture(true);
  }

  console_->print(F("LFSCAN best hz="));
  console_->print(best_hz);
  console_->print(F(" min="));
  console_->print(best_min);
  console_->print(F(" max="));
  console_->print(best_max);
  console_->print(F(" pp="));
  console_->print(best_pp);
  console_->print(F(" avg="));
  console_->println(best_avg);
}

void NetworkRfidReader::handleLfIndalaCommand(String rest) {
  uint32_t requested = nextToken(rest).toInt();
  if (requested == 0) {
    requested = 20000;
  } else if (requested < 12000) {
    requested = 12000;
  } else if (requested > 60000) {
    requested = 60000;
  }

  IndalaId id = {};
  IndalaDecodeInfo info = {};
  size_t samples_read = 0;

  console_->print(F("LF Indala software capture samples="));
  console_->println(requested);

  if (!captureIndalaSamples(static_cast<uint16_t>(requested), id, info, samples_read)) {
    console_->print(F("ERR indala not decoded samples="));
    console_->print(samples_read);
    console_->print(F(" bits="));
    console_->print(info.bit_count);
    console_->print(F(" shifts="));
    console_->print(info.phase_shifts);
    console_->print(F(" halfUs="));
    console_->print(info.half_period_us);
    console_->print(F(" hist="));
    for (size_t i = 1; i < 16; i++) {
      if (info.pulse_hist[i] == 0) {
        continue;
      }
      console_->print(i);
      console_->print(':');
      console_->print(info.pulse_hist[i]);
      console_->print(' ');
    }
    if (info.bit_preview_len > 0) {
      console_->print(F("preview="));
      console_->print(info.bit_preview);
    }
    console_->println();
    return;
  }

  String card_id = F("FC=");
  card_id += String(id.facility);
  card_id += F(" CN=");
  card_id += String(id.card);
  card_id += F(" RAW=");
  card_id += hexBytes(id.raw, sizeof(id.raw), true);

  console_->print(F("OK indala samples="));
  console_->print(samples_read);
  console_->print(F(" bits="));
  console_->print(info.bit_count);
  console_->print(F(" start="));
  console_->print(info.frame_start);
  console_->print(F(" shifts="));
  console_->print(info.phase_shifts);
  console_->print(F(" halfUs="));
  console_->print(info.half_period_us);
  console_->print(F(" inverted="));
  console_->println(info.inverted ? F("yes") : F("no"));

  emitCard("LF", "Indala", card_id);
}

bool NetworkRfidReader::captureIndalaSamples(
  uint16_t requestedSamples,
  IndalaId& id,
  IndalaDecodeInfo& info,
  size_t& samplesRead) {
  samplesRead = 0;
  memset(&info, 0, sizeof(info));

  if (config_.pins.lfData < 0) {
    return false;
  }

  const bool restore_capture = lfCaptureEnabled_;
  if (restore_capture) {
    setLfCapture(false);
  }
  if (!lfCarrierEnabled_) {
    setLfCarrier(true);
  }

  delay(20);

  const int pin = config_.pins.lfData;
  pinMode(pin, INPUT);

  static constexpr uint8_t kSamplePeriodUs = 2;
  bool decoded = false;
  IndalaDecoder decoder;
  decoder.reset();

  bool current_level = gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
  uint16_t run_samples = 0;
  uint32_t next_sample_us = micros();

  for (uint16_t i = 0; i < requestedSamples; i++) {
    while (static_cast<int32_t>(micros() - next_sample_us) < 0) {
      // tight timing loop for LF diagnostic sampling
    }
    next_sample_us += kSamplePeriodUs;

    const bool level = gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
    if (level == current_level && run_samples < 0x7FFF) {
      run_samples++;
    } else {
      if (run_samples > 0) {
        decoded = decoder.pushPulse(
          current_level,
          static_cast<uint32_t>(run_samples) * kSamplePeriodUs,
          id,
        info);
        if (decoded) {
          samplesRead = i;
          break;
        }
      }
      current_level = level;
      run_samples = 1;
    }
  }

  if (!decoded && run_samples > 0) {
    decoded = decoder.pushPulse(
      current_level,
      static_cast<uint32_t>(run_samples) * kSamplePeriodUs,
      id,
      info);
  }
  if (!decoded) {
    decoded = decoder.finish(id, info);
    samplesRead = requestedSamples;
  }
  if (decoded && samplesRead == 0) {
    samplesRead = requestedSamples;
  }

  // Keep GPIO edge capture disabled after this diagnostic. Indala PSK presents
  // carrier-rate edges on LF DATA, which can overwhelm the ISR path while the
  // command is printing diagnostics.
  (void) restore_capture;

  return decoded;
}

void NetworkRfidReader::handleHfCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR hf status|probe|speed <hz>|init|off|mode scan|card|tech a|b|f|v on|off|card ..."));
    return;
  }
  if (sub == "status") {
    console_->print(F("HF bus="));
    console_->print(hfBusModeName(config_.pins.hfBusMode));
    console_->print(F(" role="));
    console_->print(hfRoleName(config_.hfRole));
    console_->print(F(" ready="));
    console_->print(hfReady_ ? F("yes") : F("no"));
    console_->print(F(" discovery="));
    console_->print(hfDiscoveryActive_ ? F("yes") : F("no"));
    console_->print(F(" cardEmu="));
    console_->print(hfCardEmulationActive_ ? F("yes") : F("no"));
    console_->print(F(" p2p=disabled"));
    console_->print(F(" cardType="));
    console_->print(hfCardTypeName(config_.hfCardType));
    console_->print(F(" ndef="));
    console_->print(hfCardPayloadTypeName(config_.hfCardPayloadType));
    console_->print(F(" spiHz="));
    console_->print(config_.hfSpiHz);
    console_->print(F(" i2cHz="));
    console_->print(config_.hfI2cHz);
    console_->print(F(" techs=0x"));
    console_->println(activeHfTechs(), HEX);
    return;
  }

  if (sub == "probe") {
    probeHfIdentity();
    return;
  }

  if (sub == "mode") {
    const String mode = lowerCopy(nextToken(rest));
    if (mode == "scan") {
      config_.hfRole = NetworkRfidHfRole::Scan;
    } else if (mode == "card") {
      config_.hfRole = NetworkRfidHfRole::CardEmulation;
      config_.autoInitHf = true;
      if (!hfReady_) {
        hfReady_ = setupHf();
      }
    } else {
      console_->println(F("ERR hf mode scan|card"));
      return;
    }
    restartHfRole();
    console_->print(F("OK hf mode="));
    console_->println(hfRoleName(config_.hfRole));
    return;
  }

  if (sub == "tech") {
    const String tech = lowerCopy(nextToken(rest));
    const String value = lowerCopy(nextToken(rest));
    if (tech.length() == 0 || value.length() == 0) {
      console_->println(F("ERR hf tech a|b|f|v on|off"));
      return;
    }
    uint16_t mask = 0;
    if (tech == "a") {
      mask = RFAL_NFC_POLL_TECH_A;
    } else if (tech == "b") {
      mask = RFAL_NFC_POLL_TECH_B;
    } else if (tech == "f") {
      mask = RFAL_NFC_POLL_TECH_F;
    } else if (tech == "v") {
      mask = RFAL_NFC_POLL_TECH_V;
    } else {
      console_->println(F("ERR hf tech a|b|f|v on|off"));
      return;
    }
    if (value == "on") {
      config_.hfTechs |= mask;
    } else if (value == "off") {
      config_.hfTechs &= ~mask;
    } else {
      console_->println(F("ERR hf tech a|b|f|v on|off"));
      return;
    }
    stopHfDiscovery();
    console_->print(F("OK hf techs=0x"));
    console_->println(config_.hfTechs, HEX);
    return;
  }

  if (sub == "card") {
    String action = lowerCopy(nextToken(rest));
    if (action.length() == 0) {
      console_->println(F("ERR hf card status|uid <hex>|type nfc-a-t4t|nfc-a-t2t|ndef url|text|vcard|wifi <payload>"));
      return;
    }
    if (action == "status") {
      console_->print(F("hf card role="));
      console_->print(hfRoleName(config_.hfRole));
      console_->print(F(" ready="));
      console_->print(hfReady_ ? F("yes") : F("no"));
      console_->print(F(" uid="));
      console_->print(config_.hfCardUid);
      console_->print(F(" active="));
      console_->print(hfCardEmulationActive_ ? F("yes") : F("no"));
      console_->print(F(" state="));
      console_->print(hfLmStateName(hfLastLmState_));
      console_->print(F(" type="));
      console_->print(hfCardTypeName(config_.hfCardType));
      console_->print(F(" ndef="));
      console_->print(hfCardPayloadTypeName(config_.hfCardPayloadType));
      if (config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Wifi) {
        console_->print(F(" ssid=\""));
        console_->print(config_.hfCardWifiSsid);
      } else {
        console_->print(F(" payload=\""));
        console_->print(config_.hfCardPayload);
      }
      console_->println(F("\""));
      return;
    }
    if (action == "uid") {
      String uid_text = rest;
      uid_text.trim();
      String lower_uid = lowerCopy(uid_text);
      int cut_pos = -1;
      const char* markers[] = {" active=", " state=", " type=", " ndef=", " payload=", " ssid="};
      for (size_t i = 0; i < (sizeof(markers) / sizeof(markers[0])); ++i) {
        const int marker_pos = lower_uid.indexOf(markers[i]);
        if (marker_pos >= 0 && (cut_pos < 0 || marker_pos < cut_pos)) {
          cut_pos = marker_pos;
        }
      }
      if (cut_pos >= 0) {
        uid_text = uid_text.substring(0, cut_pos);
        uid_text.trim();
      }
      size_t uid_len = 0;
      uint8_t uid[RFAL_NFCID1_TRIPLE_LEN] = {};
      if (!parseHexBytes(uid_text, uid, sizeof(uid), uid_len) ||
          (uid_len != 4 && uid_len != 7)) {
        console_->println(F("ERR hf card uid <4|7-byte-uid-hex>"));
        return;
      }
      config_.hfCardUid = hexBytes(uid, uid_len, true);
      if (hfCardEmulationActive_) {
        stopHfCardEmulation();
        lastHfCardEmuAttemptMs_ = 0;
        restartHfRole();
      }
      console_->print(F("OK hf card uid="));
      console_->print(config_.hfCardUid);
      console_->print(F(" active="));
      console_->println(hfCardEmulationActive_ ? F("yes") : F("no"));
      return;
    }
    if (action == "type") {
      String type = lowerCopy(nextToken(rest));
      type.trim();
      if (type == "nfc-a-t4t") {
        config_.hfCardType = NetworkRfidHfCardType::NfcAType4;
      } else if (type == "nfc-a-t2t") {
        config_.hfCardType = NetworkRfidHfCardType::NfcAType2;
      } else {
        console_->println(F("ERR hf card type nfc-a-t4t|nfc-a-t2t"));
        return;
      }
      if (hfCardEmulationActive_) {
        stopHfCardEmulation();
        lastHfCardEmuAttemptMs_ = 0;
      }
      if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
        restartHfRole();
      }
      console_->print(F("OK hf card type="));
      console_->println(hfCardTypeName(config_.hfCardType));
      return;
    }
    if (action == "ndef") {
      const String type = lowerCopy(nextToken(rest));
      rest.trim();
      if (type == "url") {
        config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Url;
        config_.hfCardPayload = rest.length() > 0 ? rest : String(F("https://www.elechouse.com/"));
        limitString(config_.hfCardPayload, kMaxHfCardPayloadLen);
      } else if (type == "text") {
        config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Text;
        config_.hfCardPayload = rest;
        limitString(config_.hfCardPayload, kMaxHfCardPayloadLen);
      } else if (type == "vcard") {
        config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Vcard;
        config_.hfCardPayload = rest;
        limitString(config_.hfCardPayload, kMaxHfCardPayloadLen);
      } else if (type == "wifi") {
        config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Wifi;
        config_.hfCardWifiSsid = nextToken(rest);
        config_.hfCardWifiPassword = rest;
        limitString(config_.hfCardWifiSsid, kMaxHfCardWifiSsidLen);
        limitString(config_.hfCardWifiPassword, kMaxHfCardWifiPasswordLen);
      } else {
        console_->println(F("ERR hf card ndef url <url>|text <text>|vcard <text>|wifi <ssid> <password>"));
        return;
      }
      console_->print(F("OK hf card ndef="));
      console_->println(hfCardPayloadTypeName(config_.hfCardPayloadType));
      return;
    }
    console_->println(F("ERR hf card status|uid <hex>|type nfc-a-t4t|nfc-a-t2t|ndef url|text|vcard|wifi <payload>"));
    return;
  }

  if (sub == "speed") {
    const uint32_t hz = nextToken(rest).toInt();
    if (config_.pins.hfBusMode == NetworkRfidHfBusMode::I2c) {
      if (hz < 10000UL || hz > 1000000UL) {
        console_->println(F("ERR hf speed <10000..1000000> for I2C"));
        return;
      }
      config_.hfI2cHz = hz;
    } else {
      if (hz < 100000UL || hz > 10000000UL) {
        console_->println(F("ERR hf speed <100000..10000000> for SPI"));
        return;
      }
      config_.hfSpiHz = hz;
    }
    console_->println(F("OK hf speed updated"));
    return;
  }

  if (sub == "init") {
    config_.autoInitHf = true;
    console_->println(F("HF init starting"));
    hfReady_ = setupHf();
    if (hfReady_ && !isLfEnabled()) {
      setActiveSlot(NetworkRfidSlot::HF);
    } else if (hfReady_ && activeSlot_ == NetworkRfidSlot::HF) {
      restartHfRole();
    }
    console_->println(hfReady_ ? F("HF init OK") : F("HF init failed"));
    return;
  }

  if (sub == "off") {
    config_.autoInitHf = false;
    releaseHf();
    if (isLfEnabled()) {
      setActiveSlot(NetworkRfidSlot::LF);
    }
    console_->println(F("HF stopped"));
    return;
  }

  console_->println(F("ERR hf status|probe|speed <hz>|init|off|mode scan|card|tech a|b|f|v on|off|card ..."));
}

void NetworkRfidReader::handleWifiCommand(String rest) {
  const String first = nextToken(rest);
  const String action = lowerCopy(first);

  if (first.length() == 0) {
    console_->println(F("ERR wifi status|scan [ssid]|set <ssid> <password>|reconnect|clear"));
    return;
  }
  if (action == "status") {
    const wl_status_t status = WiFi.status();
    console_->print(F("wifi ssid="));
    console_->print(config_.wifiSsid);
    console_->print(F(" passLen="));
    console_->print(config_.wifiPassword.length());
    console_->print(F(" hostname="));
    console_->print(NETWORK_RFID_WIFI_HOSTNAME);
    console_->print(F(" status="));
    console_->print(wifiStatusName(status));
    console_->print(F("("));
    console_->print(static_cast<int>(status));
    console_->print(F(")"));
    if (status == WL_CONNECTED) {
      console_->print(F(" ip="));
      console_->print(WiFi.localIP());
      console_->print(F(" rssi="));
      console_->print(WiFi.RSSI());
      console_->print(F(" bssid="));
      console_->print(WiFi.BSSIDstr());
    }
    if (lastWifiDisconnectReason_ != 0) {
      console_->print(F(" lastReason="));
      console_->print(lastWifiDisconnectReason_);
      console_->print(F(":"));
      console_->print(WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(lastWifiDisconnectReason_)));
      console_->print(F(" ageMs="));
      console_->print(millis() - lastWifiDisconnectMs_);
    }
    console_->println();
    return;
  }

  if (action == "clear") {
    config_.wifiSsid = "";
    config_.wifiPassword = "";
    wifiConnectInProgress_ = false;
    restartTcpSocket();
    WiFi.disconnect(false, true);
    console_->println(F("OK wifi cleared"));
    return;
  }

  if (action == "reconnect") {
    if (config_.wifiSsid.length() == 0) {
      console_->println(F("ERR wifi ssid is empty"));
      return;
    }
    restartStation();
    console_->println(F("OK wifi reconnecting"));
    return;
  }

  if (action == "scan") {
    const String target = rest;
    console_->println(F("WiFi scan starting"));
    const int count = WiFi.scanNetworks(false, true);
    console_->print(F("WiFi scan count="));
    console_->println(count);
    for (int i = 0; i < count; i++) {
      const String ssid = WiFi.SSID(i);
      if (target.length() > 0 && ssid != target) {
        continue;
      }
      console_->print(F("  ssid="));
      console_->print(ssid);
      console_->print(F(" rssi="));
      console_->print(WiFi.RSSI(i));
      console_->print(F(" ch="));
      console_->print(WiFi.channel(i));
      console_->print(F(" enc="));
      console_->println(static_cast<int>(WiFi.encryptionType(i)));
    }
    WiFi.scanDelete();
    return;
  }

  if (action != "set") {
    console_->println(F("ERR wifi status|scan [ssid]|set <ssid> <password>|reconnect|clear"));
    return;
  }

  const String ssid = nextToken(rest);
  if (ssid.length() == 0) {
    console_->println(F("ERR wifi set <ssid> <password>"));
    return;
  }

  config_.wifiSsid = ssid;
  rest.trim();
  config_.wifiPassword = rest;
  restartStation();
  console_->println(F("OK wifi updated"));
}

void NetworkRfidReader::handleTcpCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));

  if (sub.length() == 0) {
    console_->println(F("ERR tcp status|client <host> <port>|server <port>|off|events on|off|commands on|off"));
    return;
  }
  if (sub == "status") {
    console_->print(F("tcp mode="));
    console_->print(tcpModeName(config_.tcpMode));
    console_->print(F(" host="));
    console_->print(config_.tcpMode == NetworkRfidTcpMode::ElechouseTest ? String(kElechouseBrokerHost) : config_.tcpHost);
    console_->print(F(" port="));
    console_->print(config_.tcpMode == NetworkRfidTcpMode::ElechouseTest ? kElechouseBrokerPort : config_.tcpPort);
    console_->print(F(" listen="));
    console_->print(config_.tcpListenPort);
    console_->print(F(" events="));
    console_->print(config_.tcpEchoEvents ? F("on") : F("off"));
    console_->print(F(" commands="));
    console_->print(config_.tcpCommands ? F("on") : F("off"));
    if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
      console_->print(F(" session="));
      console_->print(config_.elechouseSessionCode);
      console_->print(F(" connected="));
      console_->print(tcpClient_.connected() ? F("yes") : F("no"));
      console_->print(F(" brokerOk="));
      console_->print(elechouseBrokerOk_ ? F("yes") : F("no"));
    }
    console_->println();
    return;
  }

  if (sub == "off") {
    config_.tcpMode = NetworkRfidTcpMode::Off;
    console_->println(F("OK tcp off"));
    if (console_ == &tcpClient_ || console_ == &serverClient_) {
      console_->flush();
      delay(20);
    }
    restartTcpSocket();
    return;
  }

  if (sub == "client") {
    const String host = nextToken(rest);
    const uint16_t port = static_cast<uint16_t>(nextToken(rest).toInt());
    if (host.length() == 0 || port == 0) {
      console_->println(F("ERR tcp client <host> <port>"));
      return;
    }
    config_.tcpMode = NetworkRfidTcpMode::Client;
    config_.tcpHost = host;
    config_.tcpPort = port;
    console_->println(F("OK tcp client"));
    if (console_ == &tcpClient_ || console_ == &serverClient_) {
      console_->flush();
      delay(20);
    }
    restartTcpSocket();
    return;
  }

  if (sub == "server") {
    const uint16_t port = static_cast<uint16_t>(nextToken(rest).toInt());
    if (port == 0) {
      console_->println(F("ERR tcp server <port>"));
      return;
    }
    config_.tcpMode = NetworkRfidTcpMode::Server;
    config_.tcpListenPort = port;
    console_->println(F("OK tcp server"));
    if (console_ == &tcpClient_ || console_ == &serverClient_) {
      console_->flush();
      delay(20);
    }
    restartTcpSocket();
    return;
  }

  if (sub == "events") {
    const String value = lowerCopy(nextToken(rest));
    if (value != "on" && value != "off") {
      console_->println(F("ERR tcp events on|off"));
      return;
    }
    config_.tcpEchoEvents = value == "on";
    console_->println(F("OK tcp events updated"));
    return;
  }

  if (sub == "commands") {
    const String value = lowerCopy(nextToken(rest));
    if (value != "on" && value != "off") {
      console_->println(F("ERR tcp commands on|off"));
      return;
    }
    config_.tcpCommands = value == "on";
    console_->println(F("OK tcp commands updated"));
    return;
  }

  console_->println(F("ERR tcp status|client <host> <port>|server <port>|off|events on|off|commands on|off"));
}

void NetworkRfidReader::handleElechouseCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR elechouse status|on <session_code>|off|reconnect|clear"));
    return;
  }
  if (sub == "status") {
    console_->print(F("elechouse host="));
    console_->print(kElechouseBrokerHost);
    console_->print(F(" port="));
    console_->print(kElechouseBrokerPort);
    console_->print(F(" session="));
    console_->print(config_.elechouseSessionCode);
    console_->print(F(" mode="));
    console_->print(tcpModeName(config_.tcpMode));
    console_->print(F(" connected="));
    console_->print(tcpClient_.connected() ? F("yes") : F("no"));
    console_->print(F(" brokerOk="));
    console_->println(elechouseBrokerOk_ ? F("yes") : F("no"));
    return;
  }

  if (sub == "clear") {
    config_.elechouseSessionCode = "";
    elechouseBrokerOk_ = false;
    console_->println(F("OK elechouse cleared"));
    return;
  }

  if (sub == "on") {
    String code = nextToken(rest);
    code.trim();
    if (!isValidElechouseSessionCode(code)) {
      console_->println(F("ERR elechouse on <session_code>"));
      return;
    }
    config_.elechouseSessionCode = code;
    config_.tcpMode = NetworkRfidTcpMode::ElechouseTest;
    config_.tcpEchoEvents = true;
    config_.tcpCommands = true;
    console_->println(F("OK elechouse test on"));
    if (console_ == &tcpClient_) {
      console_->flush();
      delay(20);
    }
    restartTcpSocket();
    return;
  }

  if (sub == "off") {
    if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
      config_.tcpMode = NetworkRfidTcpMode::Off;
      console_->println(F("OK elechouse test off"));
      if (console_ == &tcpClient_) {
        console_->flush();
        delay(20);
      }
      restartTcpSocket();
    } else {
      console_->println(F("OK elechouse already off"));
    }
    return;
  }

  if (sub == "reconnect") {
    if (config_.elechouseSessionCode.length() == 0) {
      console_->println(F("ERR elechouse on <session_code>"));
      return;
    }
    if (config_.tcpMode != NetworkRfidTcpMode::ElechouseTest) {
      config_.tcpMode = NetworkRfidTcpMode::ElechouseTest;
    }
    restartTcpSocket();
    ensureTcpClient();
    console_->println(F("OK elechouse reconnecting"));
    return;
  }

  console_->println(F("ERR elechouse status|on <session_code>|off|reconnect|clear"));
}

void NetworkRfidReader::handlePortalCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR portal on|off|status|ssid <ssid> [password]"));
    return;
  }
  if (sub == "status") {
    console_->print(F("portal enabled="));
    console_->print(config_.configPortalEnabled ? F("yes") : F("no"));
    console_->print(F(" running="));
    console_->print(portalRunning_ ? F("yes") : F("no"));
    console_->print(F(" ssid="));
    console_->print(config_.configPortalSsid.length() > 0 ? config_.configPortalSsid : defaultPortalSsid());
    console_->print(F(" ip="));
    console_->print(portalRunning_ ? WiFi.softAPIP().toString() : String(F("-")));
    console_->print(F(" port="));
    console_->print(config_.configPortalPort);
    console_->print(F(" timeoutMs="));
    console_->println(portalRunning_ ? String(kPortalAutoRebootMs - (millis() - portalStartMs_)) : String(F("-")));
    return;
  }

  if (sub == "on") {
    config_.configPortalEnabled = true;
    startConfigPortal();
    console_->println(F("OK portal on"));
    return;
  }

  if (sub == "off") {
    config_.configPortalEnabled = false;
    stopConfigPortal();
    if (config_.wifiSsid.length() > 0) {
      restartStation();
    }
    console_->println(F("OK portal off"));
    return;
  }

  if (sub == "ssid") {
    const String ssid = nextToken(rest);
    rest.trim();
    if (ssid.length() == 0) {
      console_->println(F("ERR portal ssid <ssid> [password]"));
      return;
    }
    config_.configPortalSsid = ssid;
    if (rest.length() > 0) {
      config_.configPortalPassword = rest;
    } else {
      config_.configPortalPassword = "";
    }
    console_->println(F("OK portal updated, save and reboot to apply AP name/password"));
    return;
  }

  console_->println(F("ERR portal on|off|status|ssid"));
}

void NetworkRfidReader::handleInterfaceCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR interface status|mode uart|wiegand|aba|on|off|events on|off|commands on|off|baud <baud>|pulse <us> <gap_us>|wiegand bits <bits>|aba digits <n>|aba source raw|cn"));
    return;
  }
  if (sub == "status") {
    console_->print(F("interface mode="));
    console_->print(productInterfaceModeName(config_.productInterfaceMode));
    console_->print(F(" enabled="));
    console_->print(config_.hardwareUartEnabled ? F("yes") : F("no"));
    console_->print(F(" uartReady="));
    console_->print(hardwareUartReady_ ? F("yes") : F("no"));
    console_->print(F(" pulseReady="));
    console_->print(productInterfacePulseReady_ ? F("yes") : F("no"));
    console_->print(F(" d0Clock="));
    console_->print(config_.pins.hardwareUartRx);
    console_->print(F(" d1Data="));
    console_->print(config_.pins.hardwareUartTx);
    console_->print(F(" events="));
    console_->print(config_.hardwareUartEchoEvents ? F("on") : F("off"));
    console_->print(F(" commands="));
    console_->print(config_.hardwareUartCommands ? F("on") : F("off"));
    console_->print(F(" baud="));
    console_->print(config_.hardwareUartBaud);
    console_->print(F(" wiegandBits="));
    console_->print(config_.wiegandBits);
    console_->print(F(" pulseUs="));
    console_->print(config_.productPulseUs);
    console_->print(F(" gapUs="));
    console_->print(config_.productPulseGapUs);
    console_->print(F(" abaDigits="));
    if (config_.abaDigits == 0) {
      console_->print(F("auto"));
    } else {
      console_->print(config_.abaDigits);
    }
    console_->print(F(" abaSource="));
    console_->println(config_.abaUseCardNumber ? F("cn") : F("raw"));
    return;
  }

  if (sub == "mode") {
    const String value = lowerCopy(nextToken(rest));
    NetworkRfidProductInterfaceMode mode = config_.productInterfaceMode;
    if (value == "uart") {
      mode = NetworkRfidProductInterfaceMode::Uart;
    } else if (value == "wiegand") {
      mode = NetworkRfidProductInterfaceMode::Wiegand;
    } else if (value == "aba") {
      mode = NetworkRfidProductInterfaceMode::Aba;
    } else {
      console_->println(F("ERR interface mode uart|wiegand|aba"));
      return;
    }

    config_.productInterfaceMode = mode;
    config_.hardwareUartEnabled = true;
    const bool command_from_uart = console_ == &hardwareUart_;
    if (command_from_uart) {
      console_->print(F("OK interface mode="));
      console_->println(productInterfaceModeName(config_.productInterfaceMode));
      console_->flush();
      delay(20);
    }
    restartProductInterface();
    if (!command_from_uart) {
      console_->print(F("OK interface mode="));
      console_->println(productInterfaceModeName(config_.productInterfaceMode));
    }
    return;
  }

  if (sub == "on") {
    config_.hardwareUartEnabled = true;
    const bool command_from_uart = console_ == &hardwareUart_;
    if (command_from_uart) {
      console_->println(F("OK interface on"));
      console_->flush();
      delay(20);
    }
    restartProductInterface();
    if (!command_from_uart) {
      console_->println(F("OK interface on"));
    }
    return;
  }

  if (sub == "off") {
    config_.hardwareUartEnabled = false;
    console_->println(F("OK interface off"));
    if (console_ == &hardwareUart_) {
      console_->flush();
      delay(20);
    }
    restartProductInterface();
    return;
  }

  if (sub == "events") {
    const String value = lowerCopy(nextToken(rest));
    if (value != "on" && value != "off") {
      console_->println(F("ERR interface events on|off"));
      return;
    }
    config_.hardwareUartEchoEvents = value == "on";
    console_->println(F("OK interface events updated"));
    return;
  }

  if (sub == "commands") {
    const String value = lowerCopy(nextToken(rest));
    if (value != "on" && value != "off") {
      console_->println(F("ERR interface commands on|off"));
      return;
    }
    config_.hardwareUartCommands = value == "on";
    console_->println(F("OK interface commands updated"));
    return;
  }

  if (sub == "baud") {
    const uint32_t baud = nextToken(rest).toInt();
    if (baud < 1200UL || baud > 3000000UL) {
      console_->println(F("ERR interface baud 1200..3000000"));
      return;
    }
    config_.hardwareUartBaud = baud;
    config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Uart;
    const bool command_from_uart = console_ == &hardwareUart_;
    if (command_from_uart) {
      console_->print(F("OK interface baud="));
      console_->println(config_.hardwareUartBaud);
      console_->flush();
      delay(20);
    }
    restartProductInterface();
    if (!command_from_uart) {
      console_->print(F("OK interface baud="));
      console_->println(config_.hardwareUartBaud);
    }
    return;
  }

  if (sub == "pulse") {
    const uint32_t pulse = nextToken(rest).toInt();
    const uint32_t gap = nextToken(rest).toInt();
    if (pulse < kMinProductPulseUs || pulse > kMaxProductPulseUs ||
        gap < kMinProductPulseGapUs || gap > kMaxProductPulseGapUs) {
      console_->print(F("ERR interface pulse <"));
      console_->print(kMinProductPulseUs);
      console_->print(F(".."));
      console_->print(kMaxProductPulseUs);
      console_->print(F("_us> <"));
      console_->print(kMinProductPulseGapUs);
      console_->print(F(".."));
      console_->print(kMaxProductPulseGapUs);
      console_->println(F("_us>"));
      return;
    }
    config_.productPulseUs = static_cast<uint16_t>(pulse);
    config_.productPulseGapUs = static_cast<uint16_t>(gap);
    console_->println(F("OK interface pulse updated"));
    return;
  }

  if (sub == "wiegand") {
    const String action = lowerCopy(nextToken(rest));
    if (action == "bits") {
      const uint8_t bits = static_cast<uint8_t>(nextToken(rest).toInt());
      if (!isSupportedWiegandBits(bits)) {
        console_->println(F("ERR interface wiegand bits 26|34|37|56"));
        return;
      }
      config_.wiegandBits = bits;
      console_->print(F("OK wiegand bits="));
      console_->println(config_.wiegandBits);
      return;
    }
    console_->println(F("ERR interface wiegand bits <26|34|37|56>"));
    return;
  }

  if (sub == "aba") {
    const String action = lowerCopy(nextToken(rest));
    if (action == "digits") {
      const uint32_t digits = nextToken(rest).toInt();
      if (digits > 32) {
        console_->println(F("ERR interface aba digits 0..32"));
        return;
      }
      config_.abaDigits = static_cast<uint8_t>(digits);
      console_->print(F("OK aba digits="));
      if (config_.abaDigits == 0) {
        console_->println(F("auto"));
      } else {
        console_->println(config_.abaDigits);
      }
      return;
    }
    if (action == "source") {
      const String source = lowerCopy(nextToken(rest));
      if (source == "raw") {
        config_.abaUseCardNumber = false;
      } else if (source == "cn") {
        config_.abaUseCardNumber = true;
      } else {
        console_->println(F("ERR interface aba source raw|cn"));
        return;
      }
      console_->print(F("OK aba source="));
      console_->println(config_.abaUseCardNumber ? F("cn") : F("raw"));
      return;
    }
    console_->println(F("ERR interface aba digits <0..32>|source raw|cn"));
    return;
  }

  console_->println(F("ERR interface status|mode uart|wiegand|aba|on|off|events on|off|commands on|off|baud <baud>|pulse <us> <gap_us>|wiegand bits <bits>|aba digits <n>|aba source raw|cn"));
}

void NetworkRfidReader::handleFeedbackCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR feedback status|on|off|buzzer <hz> <ms>|success_ms <ms>|idle <r> <g> <b>|success <r> <g> <b>|test"));
    return;
  }
  if (sub == "status") {
    console_->print(F("feedback enabled="));
    console_->print(config_.feedbackEnabled ? F("yes") : F("no"));
    console_->print(F(" led="));
    console_->print(config_.pins.feedbackLed);
    console_->print(F(" buzzer="));
    console_->print(config_.pins.feedbackBuzzer);
    console_->print(F(" buzzerHz="));
    console_->print(config_.feedbackBuzzerHz);
    console_->print(F(" buzzerMs="));
    console_->print(config_.feedbackBuzzerMs);
    console_->print(F(" successMs="));
    console_->println(config_.feedbackSuccessMs);
    return;
  }

  if (sub == "on") {
    config_.feedbackEnabled = true;
    setupFeedback();
    console_->println(F("OK feedback on"));
    return;
  }
  if (sub == "off") {
    config_.feedbackEnabled = false;
    stopFeedbackBuzzer();
    setFeedbackLed(0, 0, 0);
    console_->println(F("OK feedback off"));
    return;
  }
  if (sub == "buzzer") {
    const uint32_t hz = nextToken(rest).toInt();
    const uint32_t ms = nextToken(rest).toInt();
    if (hz < 100UL || hz > 20000UL || ms > 5000UL) {
      console_->println(F("ERR feedback buzzer <100..20000_hz> <0..5000_ms>"));
      return;
    }
    config_.feedbackBuzzerHz = hz;
    config_.feedbackBuzzerMs = static_cast<uint16_t>(ms);
    console_->println(F("OK feedback buzzer updated"));
    return;
  }
  if (sub == "success_ms") {
    const uint32_t ms = nextToken(rest).toInt();
    if (ms > 60000UL) {
      console_->println(F("ERR feedback success_ms <0..60000>"));
      return;
    }
    config_.feedbackSuccessMs = static_cast<uint16_t>(ms);
    console_->println(F("OK feedback success_ms updated"));
    return;
  }
  if (sub == "idle" || sub == "success") {
    const uint32_t red = nextToken(rest).toInt();
    const uint32_t green = nextToken(rest).toInt();
    const uint32_t blue = nextToken(rest).toInt();
    if (red > 65535UL || green > 65535UL || blue > 65535UL) {
      console_->println(F("ERR feedback idle|success <r16> <g16> <b16>"));
      return;
    }
    if (sub == "idle") {
      config_.feedbackIdleRed = static_cast<uint16_t>(red);
      config_.feedbackIdleGreen = static_cast<uint16_t>(green);
      config_.feedbackIdleBlue = static_cast<uint16_t>(blue);
      showFeedbackIdle();
    } else {
      config_.feedbackSuccessRed = static_cast<uint16_t>(red);
      config_.feedbackSuccessGreen = static_cast<uint16_t>(green);
      config_.feedbackSuccessBlue = static_cast<uint16_t>(blue);
    }
    console_->println(F("OK feedback color updated"));
    return;
  }
  if (sub == "test") {
    triggerFeedbackSuccess();
    console_->println(F("OK feedback test"));
    return;
  }

  console_->println(F("ERR feedback status|on|off|buzzer <hz> <ms>|success_ms <ms>|idle <r> <g> <b>|success <r> <g> <b>|test"));
}

void NetworkRfidReader::handleButtonCommand(String rest) {
  const String sub = lowerCopy(nextToken(rest));
  if (sub.length() == 0) {
    console_->println(F("ERR button status|on|off|timing <wifi_ms> <reset_ms>"));
    return;
  }
  if (sub == "status") {
    console_->print(F("button enabled="));
    console_->print(config_.configButtonEnabled ? F("yes") : F("no"));
    console_->print(F(" pin="));
    console_->print(config_.pins.configButton);
    console_->print(F(" activeLow="));
    console_->print(config_.pins.configButtonActiveLow ? F("yes") : F("no"));
    console_->print(F(" pressed="));
    console_->print(readConfigButtonPressed() ? F("yes") : F("no"));
    console_->print(F(" wifiMs="));
    console_->print(config_.buttonWifiConfigMs);
    console_->print(F(" resetMs="));
    console_->println(config_.buttonFactoryResetMs);
    return;
  }

  if (sub == "on") {
    config_.configButtonEnabled = true;
    setupButton();
    console_->println(F("OK button on"));
    return;
  }
  if (sub == "off") {
    config_.configButtonEnabled = false;
    setupButton();
    console_->println(F("OK button off"));
    return;
  }
  if (sub == "timing") {
    const uint32_t wifi_ms = nextToken(rest).toInt();
    const uint32_t reset_ms = nextToken(rest).toInt();
    if (wifi_ms < 1000UL || reset_ms <= wifi_ms || reset_ms > 60000UL) {
      console_->println(F("ERR button timing <wifi_ms>=1000..59999 <reset_ms>wifi_ms+1..60000"));
      return;
    }
    config_.buttonWifiConfigMs = static_cast<uint16_t>(wifi_ms);
    config_.buttonFactoryResetMs = static_cast<uint16_t>(reset_ms);
    console_->println(F("OK button timing updated"));
    return;
  }

  console_->println(F("ERR button status|on|off|timing <wifi_ms> <reset_ms>"));
}

void NetworkRfidReader::startConfigPortal() {
  if (!config_.configPortalEnabled || portalRunning_) {
    return;
  }

  WiFi.mode(WiFi.status() == WL_CONNECTED ? WIFI_AP_STA : WIFI_AP);

  IPAddress portal_ip(10, 10, 10, 10);
  IPAddress gateway(10, 10, 10, 10);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(portal_ip, gateway, subnet);

  String ssid = config_.configPortalSsid.length() > 0 ? config_.configPortalSsid : defaultPortalSsid();
  const bool open_ap = config_.configPortalPassword.length() == 0;
  const char* password = open_ap ? "" : config_.configPortalPassword.c_str();
  const uint8_t ap_channel = 6;
  if (!WiFi.softAP(ssid.c_str(), password, ap_channel, 0, 4)) {
    if (console_ != nullptr) {
      console_->println(F("ERR portal AP start failed"));
    }
    return;
  }

  portalDns_ = new DNSServer();
  portalDns_->setErrorReplyCode(DNSReplyCode::NoError);
  portalDns_->start(53, "*", portal_ip);

  portalServer_ = new WebServer(config_.configPortalPort == 0 ? 80 : config_.configPortalPort);
  portalServer_->on("/", HTTP_GET, [this]() { handlePortalRoot(); });
  portalServer_->on("/save", HTTP_POST, [this]() { handlePortalSave(); });
  portalServer_->on("/reboot", HTTP_POST, [this]() { handlePortalReboot(); });
  portalServer_->onNotFound([this]() { handlePortalNotFound(); });
  portalServer_->begin();

  portalRunning_ = true;
  portalStartMs_ = millis();
  if (console_ != nullptr) {
    console_->print(F("Portal AP ssid="));
    console_->print(ssid);
    console_->print(F(" ip="));
    console_->print(WiFi.softAPIP());
    console_->print(F(" port="));
    console_->print(config_.configPortalPort == 0 ? 80 : config_.configPortalPort);
    console_->println(F(" url=http://10.10.10.10/"));
  }
}

void NetworkRfidReader::stopConfigPortal() {
  if (portalServer_ != nullptr) {
    portalServer_->stop();
    delete portalServer_;
    portalServer_ = nullptr;
  }
  if (portalDns_ != nullptr) {
    portalDns_->stop();
    delete portalDns_;
    portalDns_ = nullptr;
  }
  if (portalRunning_) {
    WiFi.softAPdisconnect(false);
    portalRunning_ = false;
    portalStartMs_ = 0;
  }
}

void NetworkRfidReader::serviceConfigPortal() {
  if (!config_.configPortalEnabled && portalRunning_) {
    stopConfigPortal();
    if (config_.wifiSsid.length() > 0) {
      startWifi();
    }
    return;
  }

  if (portalRunning_ && portalStartMs_ != 0 && (millis() - portalStartMs_) >= kPortalAutoRebootMs) {
    if (console_ != nullptr) {
      console_->println(F("Portal timeout, rebooting"));
    }
    delay(100);
    ESP.restart();
  }

  if (portalDns_ != nullptr) {
    portalDns_->processNextRequest();
  }
  if (portalServer_ != nullptr) {
    portalServer_->handleClient();
  }
}

void NetworkRfidReader::handlePortalRoot() {
  sendPortalPage("");
}

void NetworkRfidReader::handlePortalSave() {
  if (portalServer_ == nullptr) {
    return;
  }

  const String new_portal_password = portalServer_->arg("portal_password");
  if (new_portal_password.length() > 0 && new_portal_password.length() < 8) {
    sendPortalPage("AP password must be empty or at least 8 characters.");
    return;
  }
  const NetworkRfidHfRole old_hf_role = config_.hfRole;
  const bool old_auto_lf = config_.autoStartLf;
  const bool old_auto_hf = config_.autoInitHf;
  const NetworkRfidProductInterfaceMode old_iface_mode = config_.productInterfaceMode;
  const bool old_iface_enabled = config_.hardwareUartEnabled;
  const bool old_iface_events = config_.hardwareUartEchoEvents;
  const bool old_iface_commands = config_.hardwareUartCommands;
  const uint32_t old_uart_baud = config_.hardwareUartBaud;
  const uint8_t old_wiegand_bits = config_.wiegandBits;
  const uint16_t old_pulse_us = config_.productPulseUs;
  const uint16_t old_pulse_gap_us = config_.productPulseGapUs;
  const uint8_t old_aba_digits = config_.abaDigits;
  const bool old_aba_source_cn = config_.abaUseCardNumber;
  const bool old_feedback_enabled = config_.feedbackEnabled;
  const bool old_button_enabled = config_.configButtonEnabled;

  NetworkRfidTcpMode new_tcp_mode = NetworkRfidTcpMode::Off;
  const String mode = lowerCopy(portalServer_->arg("tcp_mode"));
  if (mode == "client") {
    new_tcp_mode = NetworkRfidTcpMode::Client;
  } else if (mode == "server") {
    new_tcp_mode = NetworkRfidTcpMode::Server;
  } else if (mode == "elechouse") {
    new_tcp_mode = NetworkRfidTcpMode::ElechouseTest;
  }

  const uint16_t new_portal_port = parsePortForm(portalServer_, "portal_port", config_.configPortalPort == 0 ? 80 : config_.configPortalPort);
  const uint16_t new_tcp_listen = parsePortForm(portalServer_, "tcp_listen_port", config_.tcpListenPort);
  if (new_tcp_mode == NetworkRfidTcpMode::Server &&
      new_tcp_listen == new_portal_port) {
    sendPortalPage("TCP server port conflicts with portal port.");
    return;
  }

  String new_elechouse_code = portalServer_->arg("elechouse_code");
  new_elechouse_code.trim();
  if (new_tcp_mode == NetworkRfidTcpMode::ElechouseTest) {
    if (!isValidElechouseSessionCode(new_elechouse_code)) {
      sendPortalPage("ELECHOUSE session code is required. Allowed: A-Z a-z 0-9 _ . : -");
      return;
    }
  } else if (new_elechouse_code.length() > 0 &&
             !isValidElechouseSessionCode(new_elechouse_code)) {
    sendPortalPage("ELECHOUSE session code contains invalid characters.");
    return;
  }

  config_.wifiSsid = portalServer_->arg("wifi_ssid");
  config_.wifiPassword = portalServer_->arg("wifi_password");
  config_.tcpMode = new_tcp_mode;

  if (new_tcp_mode == NetworkRfidTcpMode::Client) {
    config_.tcpHost = portalServer_->arg("tcp_host");
    config_.tcpPort = parsePortForm(portalServer_, "tcp_port", config_.tcpPort);
  } else if (new_tcp_mode == NetworkRfidTcpMode::Server) {
    config_.tcpListenPort = new_tcp_listen;
  } else if (new_tcp_mode == NetworkRfidTcpMode::ElechouseTest) {
    config_.elechouseSessionCode = new_elechouse_code;
  } else if (new_elechouse_code.length() > 0) {
    config_.elechouseSessionCode = new_elechouse_code;
  }

  if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    config_.tcpEchoEvents = true;
    config_.tcpCommands = true;
  } else if (config_.tcpMode != NetworkRfidTcpMode::Off) {
    config_.tcpEchoEvents = parseBoolForm(portalServer_, "tcp_echo");
    config_.tcpCommands = parseBoolForm(portalServer_, "tcp_commands");
  }

  const String iface_mode = lowerCopy(portalServer_->arg("iface_mode"));
  if (iface_mode == "wiegand") {
    config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Wiegand;
  } else if (iface_mode == "aba") {
    config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Aba;
  } else {
    config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Uart;
  }
  config_.hardwareUartEnabled = parseBoolForm(portalServer_, "uart_enabled");
  config_.hardwareUartEchoEvents = parseBoolForm(portalServer_, "uart_echo");
  if (config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Uart) {
    config_.hardwareUartCommands = parseBoolForm(portalServer_, "uart_commands");
    config_.hardwareUartBaud = parseUIntForm(portalServer_, "uart_baud", config_.hardwareUartBaud);
    if (config_.hardwareUartBaud < 1200UL) {
      config_.hardwareUartBaud = 1200UL;
    } else if (config_.hardwareUartBaud > 3000000UL) {
      config_.hardwareUartBaud = 3000000UL;
    }
  } else if (config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Wiegand) {
    config_.wiegandBits = static_cast<uint8_t>(parseUIntForm(portalServer_, "wiegand_bits", config_.wiegandBits));
    if (!isSupportedWiegandBits(config_.wiegandBits)) {
      config_.wiegandBits = 34;
    }
    config_.productPulseUs = static_cast<uint16_t>(parseUIntForm(portalServer_, "pulse_us", config_.productPulseUs));
    config_.productPulseGapUs = static_cast<uint16_t>(parseUIntForm(portalServer_, "pulse_gap_us", config_.productPulseGapUs));
  } else {
    config_.productPulseUs = static_cast<uint16_t>(parseUIntForm(portalServer_, "pulse_us", config_.productPulseUs));
    config_.productPulseGapUs = static_cast<uint16_t>(parseUIntForm(portalServer_, "pulse_gap_us", config_.productPulseGapUs));
    config_.abaDigits = static_cast<uint8_t>(parseUIntForm(portalServer_, "aba_digits", config_.abaDigits));
    if (config_.abaDigits > 32) {
      config_.abaDigits = 32;
    }
    config_.abaUseCardNumber = lowerCopy(portalServer_->arg("aba_source")) == "cn";
  }
  if (config_.productPulseUs < kMinProductPulseUs) {
    config_.productPulseUs = kMinProductPulseUs;
  } else if (config_.productPulseUs > kMaxProductPulseUs) {
    config_.productPulseUs = kMaxProductPulseUs;
  }
  if (config_.productPulseGapUs < kMinProductPulseGapUs) {
    config_.productPulseGapUs = kMinProductPulseGapUs;
  } else if (config_.productPulseGapUs > kMaxProductPulseGapUs) {
    config_.productPulseGapUs = kMaxProductPulseGapUs;
  }

  config_.autoStartLf = parseBoolForm(portalServer_, "auto_lf");
  config_.autoInitHf = parseBoolForm(portalServer_, "auto_hf");
  config_.echoEventsToSerial = parseBoolForm(portalServer_, "echo_serial");
  config_.feedbackEnabled = parseBoolForm(portalServer_, "feedback_enabled");
  config_.configButtonEnabled = parseBoolForm(portalServer_, "button_enabled");

  const String hf_role = lowerCopy(portalServer_->arg("hf_role"));
  if (hf_role == "card") {
    config_.hfRole = NetworkRfidHfRole::CardEmulation;
  } else if (hf_role == "p2p") {
    sendPortalPage("HF P2P is disabled in this firmware.");
    return;
  } else {
    config_.hfRole = NetworkRfidHfRole::Scan;
  }

  if (config_.hfRole == NetworkRfidHfRole::CardEmulation) {
    String hf_card_uid = portalServer_->arg("hf_card_uid");
    hf_card_uid.trim();
    if (hf_card_uid.length() > 0) {
      uint8_t uid[RFAL_NFCID1_TRIPLE_LEN] = {};
      size_t uid_len = 0;
      if (!parseHexBytes(hf_card_uid, uid, sizeof(uid), uid_len) ||
          (uid_len != 4 && uid_len != 7)) {
        sendPortalPage("HF card UID must be 4 or 7 hex bytes.");
        return;
      }
      config_.hfCardUid = hexBytes(uid, uid_len, true);
    }

    const String card_type = lowerCopy(portalServer_->arg("hf_card_type"));
    if (card_type == "nfc-a-t2t") {
      config_.hfCardType = NetworkRfidHfCardType::NfcAType2;
    } else {
      config_.hfCardType = NetworkRfidHfCardType::NfcAType4;
    }

    const String ndef_type = lowerCopy(portalServer_->arg("hf_ndef_type"));
    if (ndef_type == "text") {
      config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Text;
    } else if (ndef_type == "vcard") {
      config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Vcard;
    } else if (ndef_type == "wifi") {
      config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Wifi;
    } else {
      config_.hfCardPayloadType = NetworkRfidHfCardPayloadType::Url;
    }

    if (config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Wifi) {
      config_.hfCardWifiSsid = portalServer_->arg("hf_ndef_wifi_ssid");
      config_.hfCardWifiPassword = portalServer_->arg("hf_ndef_wifi_password");
      limitString(config_.hfCardWifiSsid, kMaxHfCardWifiSsidLen);
      limitString(config_.hfCardWifiPassword, kMaxHfCardWifiPasswordLen);
    } else {
      config_.hfCardPayload = portalServer_->arg("hf_ndef_payload");
      config_.hfCardPayload.trim();
      if (config_.hfCardPayload.length() == 0 &&
          config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Url) {
        config_.hfCardPayload = F("https://www.elechouse.com/");
      }
      limitString(config_.hfCardPayload, kMaxHfCardPayloadLen);
    }
  }

  if (config_.hfRole == NetworkRfidHfRole::Scan) {
    uint16_t hf_techs = 0;
    if (parseBoolForm(portalServer_, "hf_tech_a")) {
      hf_techs |= RFAL_NFC_POLL_TECH_A;
    }
    if (parseBoolForm(portalServer_, "hf_tech_b")) {
      hf_techs |= RFAL_NFC_POLL_TECH_B;
    }
    if (parseBoolForm(portalServer_, "hf_tech_f")) {
      hf_techs |= RFAL_NFC_POLL_TECH_F;
    }
    if (parseBoolForm(portalServer_, "hf_tech_v")) {
      hf_techs |= RFAL_NFC_POLL_TECH_V;
    }
    config_.hfTechs = hf_techs == 0 ? RFAL_NFC_POLL_TECH_A : hf_techs;
  }

  config_.configPortalEnabled = true;
  config_.configPortalSsid = portalServer_->arg("portal_ssid");
  config_.configPortalPassword = new_portal_password;
  config_.configPortalPort = new_portal_port == 0 ? 80 : new_portal_port;

  const bool saved = saveConfig();
  restartStation();
  restartTcpSocket();
  if (old_iface_mode != config_.productInterfaceMode ||
      old_iface_enabled != config_.hardwareUartEnabled ||
      old_iface_events != config_.hardwareUartEchoEvents ||
      old_iface_commands != config_.hardwareUartCommands ||
      old_uart_baud != config_.hardwareUartBaud ||
      old_wiegand_bits != config_.wiegandBits ||
      old_pulse_us != config_.productPulseUs ||
      old_pulse_gap_us != config_.productPulseGapUs ||
      old_aba_digits != config_.abaDigits ||
      old_aba_source_cn != config_.abaUseCardNumber) {
    restartProductInterface();
  }
  if (old_feedback_enabled != config_.feedbackEnabled) {
    setupFeedback();
  }
  if (old_button_enabled != config_.configButtonEnabled) {
    setupButton();
  }

  const bool hf_role_changed = old_hf_role != config_.hfRole;
  const bool auto_lf_changed = old_auto_lf != config_.autoStartLf;
  const bool auto_hf_changed = old_auto_hf != config_.autoInitHf;
  if (!config_.autoInitHf && hfReady_) {
    releaseHf();
  } else if (config_.autoInitHf && !hfReady_) {
    hfReady_ = setupHf();
  } else if (hfReady_) {
    stopHfDiscovery();
    if (hf_role_changed) {
      stopHfCardEmulation();
    }
  }

  if (auto_lf_changed || auto_hf_changed) {
    if (config_.autoStartLf) {
      setActiveSlot(NetworkRfidSlot::LF);
    } else {
      setLfCapture(false);
      setLfCarrier(false);
      if (isHfEnabled()) {
        setActiveSlot(NetworkRfidSlot::HF);
      }
    }
  }

  if (!saved) {
    sendPortalPage("Save failed.");
    return;
  }

  sendPortalPage("Saved. Rebooting...");
  delay(800);
  ESP.restart();
}

void NetworkRfidReader::handlePortalReboot() {
  if (portalServer_ != nullptr) {
    portalServer_->send(200, "text/plain", "Rebooting\n");
  }
  delay(200);
  ESP.restart();
}

void NetworkRfidReader::handlePortalNotFound() {
  if (portalServer_ == nullptr) {
    return;
  }
  portalServer_->sendHeader("Location", "http://10.10.10.10/", true);
  portalServer_->send(302, "text/plain", "");
}

void NetworkRfidReader::sendPortalPage(const String& message) {
  if (portalServer_ == nullptr) {
    return;
  }

  const bool tcp_off = config_.tcpMode == NetworkRfidTcpMode::Off;
  const bool tcp_client = config_.tcpMode == NetworkRfidTcpMode::Client;
  const bool tcp_server = config_.tcpMode == NetworkRfidTcpMode::Server;
  const bool tcp_elechouse = config_.tcpMode == NetworkRfidTcpMode::ElechouseTest;
  const bool iface_uart = config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Uart;
  const bool iface_wiegand = config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Wiegand;
  const bool iface_aba = config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Aba;
  const bool hf_role_scan = config_.hfRole == NetworkRfidHfRole::Scan;
  const bool hf_role_card = config_.hfRole == NetworkRfidHfRole::CardEmulation;
  const bool hf_card_t4t = config_.hfCardType == NetworkRfidHfCardType::NfcAType4;
  const bool hf_card_t2t = config_.hfCardType == NetworkRfidHfCardType::NfcAType2;
  const bool ndef_url = config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Url;
  const bool ndef_text = config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Text;
  const bool ndef_vcard = config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Vcard;
  const bool ndef_wifi = config_.hfCardPayloadType == NetworkRfidHfCardPayloadType::Wifi;
  const String ap_ssid = config_.configPortalSsid.length() > 0 ? config_.configPortalSsid : defaultPortalSsid();

  String page;
  page.reserve(25000);
  page += F("<!doctype html><html><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>RFID Reader Config</title><style>");
  page += F("body{font-family:Arial,sans-serif;margin:0;background:#f6f7f9;color:#17202a}");
  page += F("main{max-width:760px;margin:0 auto;padding:18px}h1{font-size:24px;margin:0 0 14px}");
  page += F("section{background:#fff;border:1px solid #d9dee7;border-radius:8px;margin:12px 0;padding:14px}");
  page += F("h2{font-size:16px;margin:0 0 10px}.row{display:grid;grid-template-columns:170px 1fr;gap:8px;align-items:center;margin:8px 0}");
  page += F("input,select,textarea{box-sizing:border-box;width:100%;padding:8px;border:1px solid #c9d0da;border-radius:6px;font-size:15px}textarea{min-height:86px;resize:vertical}");
  page += F(".checks label{display:inline-flex;gap:6px;align-items:center;margin:4px 14px 4px 0}.checks input{width:auto}");
  page += F(".msg{padding:9px 10px;background:#eaf4ff;border:1px solid #b8dafb;border-radius:6px;margin:10px 0}");
  page += F(".note{font-size:13px;color:#566273;margin:8px 0;line-height:1.45}.kv{display:grid;grid-template-columns:170px 1fr;gap:8px;margin:6px 0;font-size:14px}");
  page += F(".actions{display:flex;gap:10px;flex-wrap:wrap}button{padding:9px 14px;border:0;border-radius:6px;background:#165dff;color:white;font-size:15px}");
  page += F("button.secondary{background:#4d5968}[hidden]{display:none!important}@media(max-width:620px){.row,.kv{grid-template-columns:1fr}}</style>");
  page += F("<script>function v(n){var s=document.querySelector('[name='+n+']');return s?s.value:'';}function m(e,a,x){var r=e.getAttribute(a);return !r||r.split(',').indexOf(x)>=0;}function syncAll(){var t=v('tcp_mode'),i=v('iface_mode'),h=v('hf_role'),n=v('hf_ndef_type');document.querySelectorAll('[data-tcp],[data-iface],[data-hfrole],[data-ndef]').forEach(function(e){var show=m(e,'data-tcp',t)&&m(e,'data-iface',i)&&m(e,'data-hfrole',h)&&m(e,'data-ndef',n);e.hidden=!show;e.querySelectorAll('input,select,textarea').forEach(function(x){x.disabled=!show;});});}document.addEventListener('DOMContentLoaded',function(){['tcp_mode','iface_mode','hf_role','hf_ndef_type'].forEach(function(n){var s=document.querySelector('[name='+n+']');if(s)s.addEventListener('change',syncAll);});syncAll();});</script>");
  page += F("</head><body><main>");
  page += F("<h1>RFID Reader Config</h1>");
  if (message.length() > 0) {
    page += F("<div class='msg'>");
    page += htmlEscape(message);
    page += F("</div>");
  }
  page += F("<section><h2>Status</h2>");
  page += F("<div>AP IP: 10.10.10.10</div><div>Portal URL: http://10.10.10.10/</div><div>STA IP: ");
  page += htmlEscape(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(F("disconnected")));
  page += F("</div><div>TCP: ");
  page += tcpModeName(config_.tcpMode);
  page += F("</div></section>");

  page += F("<form method='post' action='/save'>");
  page += F("<section><h2>WiFi</h2>");
  page += F("<div class='row'><label>SSID</label><input name='wifi_ssid' value='");
  page += htmlEscape(config_.wifiSsid);
  page += F("'></div><div class='row'><label>Password</label><input name='wifi_password' type='password' value='");
  page += htmlEscape(config_.wifiPassword);
  page += F("'></div></section>");

  page += F("<section><h2>TCP Socket</h2>");
  page += F("<div class='row'><label>Mode</label><select name='tcp_mode'>");
  page += F("<option value='off'");
  page += tcp_off ? F(" selected") : F("");
  page += F(">Off</option><option value='client'");
  page += tcp_client ? F(" selected") : F("");
  page += F(">Client</option><option value='server'");
  page += tcp_server ? F(" selected") : F("");
  page += F(">Server</option><option value='elechouse'");
  page += tcp_elechouse ? F(" selected") : F("");
  page += F(">ELECHOUSE Test</option></select></div>");
  page += F("<div class='row' data-tcp='elechouse'><label>ELECHOUSE code</label><input name='elechouse_code' maxlength='64' value='");
  page += htmlEscape(config_.elechouseSessionCode);
  page += F("'></div>");
  page += F("<div class='note' data-tcp='elechouse'>ELECHOUSE Test uses fixed target www.elechouse.com:9000. TCP events and commands are enabled automatically.</div>");
  page += F("<div class='row' data-tcp='client'><label>Client host</label><input name='tcp_host' value='");
  page += htmlEscape(config_.tcpHost);
  page += F("'></div><div class='row' data-tcp='client'><label>Client port</label><input name='tcp_port' type='number' min='1' max='65535' value='");
  page += String(config_.tcpPort);
  page += F("'></div><div class='row' data-tcp='server'><label>Server port</label><input name='tcp_listen_port' type='number' min='1' max='65535' value='");
  page += String(config_.tcpListenPort);
  page += F("'></div><div class='checks' data-tcp='client,server'><label><input type='checkbox' name='tcp_echo'");
  page += config_.tcpEchoEvents ? F(" checked") : F("");
  page += F(">TCP events</label><label><input type='checkbox' name='tcp_commands'");
  page += config_.tcpCommands ? F(" checked") : F("");
  page += F(">TCP commands</label></div></section>");

  page += F("<section><h2>Product Interface</h2>");
  page += F("<div>Fixed pins: RX/D0/Clock GPIO");
  page += String(config_.pins.hardwareUartRx);
  page += F(", TX/D1/Data GPIO");
  page += String(config_.pins.hardwareUartTx);
  page += F("</div>");
  page += F("<div class='row'><label>Mode</label><select name='iface_mode'><option value='uart'");
  page += iface_uart ? F(" selected") : F("");
  page += F(">UART</option><option value='wiegand'");
  page += iface_wiegand ? F(" selected") : F("");
  page += F(">Wiegand D0/D1</option><option value='aba'");
  page += iface_aba ? F(" selected") : F("");
  page += F(">ABA Clock/Data</option></select></div>");
  page += F("<div class='row' data-iface='uart'><label>UART baud</label><input name='uart_baud' type='number' min='1200' max='3000000' value='");
  page += String(config_.hardwareUartBaud);
  page += F("'></div><div class='row' data-iface='wiegand'><label>Wiegand bits</label><select name='wiegand_bits'><option value='26'");
  page += config_.wiegandBits == 26 ? F(" selected") : F("");
  page += F(">26</option><option value='34'");
  page += config_.wiegandBits == 34 ? F(" selected") : F("");
  page += F(">34</option><option value='37'");
  page += config_.wiegandBits == 37 ? F(" selected") : F("");
  page += F(">37</option><option value='56'");
  page += config_.wiegandBits == 56 ? F(" selected") : F("");
  page += F(">56</option></select></div>");
  page += F("<div class='row' data-iface='wiegand,aba'><label>Pulse us</label><input name='pulse_us' type='number' min='20' max='1000' value='");
  page += String(config_.productPulseUs);
  page += F("'></div><div class='row' data-iface='wiegand,aba'><label>Pulse gap us</label><input name='pulse_gap_us' type='number' min='200' max='20000' value='");
  page += String(config_.productPulseGapUs);
  page += F("'></div><div class='row' data-iface='aba'><label>ABA digits</label><input name='aba_digits' type='number' min='0' max='32' value='");
  page += String(config_.abaDigits);
  page += F("'></div><div class='row' data-iface='aba'><label>ABA source</label><select name='aba_source'><option value='raw'");
  page += config_.abaUseCardNumber ? F("") : F(" selected");
  page += F(">Raw</option><option value='cn'");
  page += config_.abaUseCardNumber ? F(" selected") : F("");
  page += F(">CN</option></select></div>");
  page += F("<div class='checks'><label><input type='checkbox' name='uart_enabled'");
  page += config_.hardwareUartEnabled ? F(" checked") : F("");
  page += F(">Interface enabled</label><label><input type='checkbox' name='uart_echo'");
  page += config_.hardwareUartEchoEvents ? F(" checked") : F("");
  page += F(">Interface events</label><label data-iface='uart'><input type='checkbox' name='uart_commands'");
  page += config_.hardwareUartCommands ? F(" checked") : F("");
  page += F(">UART commands</label></div></section>");

  page += F("<section><h2>Reader</h2>");
  page += F("<div class='checks'><label><input type='checkbox' name='auto_lf'");
  page += config_.autoStartLf ? F(" checked") : F("");
  page += F(">LF enabled</label><label><input type='checkbox' name='auto_hf'");
  page += config_.autoInitHf ? F(" checked") : F("");
  page += F(">HF enabled</label><label><input type='checkbox' name='echo_serial'");
  page += config_.echoEventsToSerial ? F(" checked") : F("");
  page += F(">USB serial events</label></div>");
  page += F("<div class='note'>Output format, LF/HF windows, dedupe, and reconnect interval are command-line settings.</div></section>");

  page += F("<section><h2>Feedback and Button</h2>");
  page += F("<div class='checks'><label><input type='checkbox' name='feedback_enabled'");
  page += config_.feedbackEnabled ? F(" checked") : F("");
  page += F(">Feedback enabled</label><label><input type='checkbox' name='button_enabled'");
  page += config_.configButtonEnabled ? F(" checked") : F("");
  page += F(">Button enabled</label></div>");
  page += F("<div class='note'>Buzzer, LED colors, and button timing are command-line settings. Button feedback: 5s one beep, 10s two beeps.</div></section>");

  page += F("<section><h2>HF</h2>");
  page += F("<div class='row'><label>Role</label><select name='hf_role'><option value='scan'");
  page += hf_role_scan ? F(" selected") : F("");
  page += F(">Scan</option><option value='card'");
  page += hf_role_card ? F(" selected") : F("");
  page += F(">Card emulation</option></select></div>");
  page += F("<div>Fixed bus: I2C, SCL GPIO");
  page += String(config_.pins.hfSck);
  page += F(", SDA GPIO");
  page += String(config_.pins.hfMiso);
  page += F(", IRQ GPIO");
  page += String(config_.pins.hfIrq);
  page += F("</div>");
  page += F("<div class='row' data-hfrole='card'><label>Card type</label><select name='hf_card_type'><option value='nfc-a-t4t'");
  page += hf_card_t4t ? F(" selected") : F("");
  page += F(">NFC-A Type 4 Tag</option><option value='nfc-a-t2t'");
  page += hf_card_t2t ? F(" selected") : F("");
  page += F(">NFC-A Type 2 Tag</option></select></div>");
  page += F("<div class='row' data-hfrole='card'><label>Card UID</label><input name='hf_card_uid' value='");
  page += htmlEscape(config_.hfCardUid);
  page += F("'></div>");
  page += F("<div class='kv' data-hfrole='card'><div>Simulated card</div><div>");
  page += hfCardTypeName(config_.hfCardType);
  page += F(" / NDEF</div></div>");
  page += F("<div class='row' data-hfrole='card'><label>NDEF type</label><select name='hf_ndef_type'><option value='url'");
  page += ndef_url ? F(" selected") : F("");
  page += F(">URL</option><option value='text'");
  page += ndef_text ? F(" selected") : F("");
  page += F(">Text</option><option value='vcard'");
  page += ndef_vcard ? F(" selected") : F("");
  page += F(">vCard</option><option value='wifi'");
  page += ndef_wifi ? F(" selected") : F("");
  page += F(">WiFi</option></select></div>");
  page += F("<div class='row' data-hfrole='card' data-ndef='url'><label>URL</label><input name='hf_ndef_payload' maxlength='512' value='");
  page += htmlEscape(config_.hfCardPayload);
  page += F("'></div>");
  page += F("<div class='row' data-hfrole='card' data-ndef='text,vcard'><label>Payload</label><textarea name='hf_ndef_payload' maxlength='512'>");
  page += htmlEscape(config_.hfCardPayload);
  page += F("</textarea></div>");
  page += F("<div class='row' data-hfrole='card' data-ndef='wifi'><label>WiFi SSID</label><input name='hf_ndef_wifi_ssid' maxlength='32' value='");
  page += htmlEscape(config_.hfCardWifiSsid);
  page += F("'></div><div class='row' data-hfrole='card' data-ndef='wifi'><label>WiFi password</label><input name='hf_ndef_wifi_password' type='password' maxlength='64' value='");
  page += htmlEscape(config_.hfCardWifiPassword);
  page += F("'></div>");
  page += F("<div class='note' data-hfrole='card'>Type 4 is best for phones. Type 2 is useful for PN532 and common NFC readers. UID supports 4 or 7 hex bytes; Type 2 is most compatible with 7 bytes.</div>");
  page += F("<div class='checks' data-hfrole='scan'>");
  page += F("<label><input type='checkbox' name='hf_tech_a'");
  page += (config_.hfTechs & RFAL_NFC_POLL_TECH_A) ? F(" checked") : F("");
  page += F(">ISO14443A</label><label><input type='checkbox' name='hf_tech_b'");
  page += (config_.hfTechs & RFAL_NFC_POLL_TECH_B) ? F(" checked") : F("");
  page += F(">ISO14443B</label><label><input type='checkbox' name='hf_tech_f'");
  page += (config_.hfTechs & RFAL_NFC_POLL_TECH_F) ? F(" checked") : F("");
  page += F(">NFC-F</label><label><input type='checkbox' name='hf_tech_v'");
  page += (config_.hfTechs & RFAL_NFC_POLL_TECH_V) ? F(" checked") : F("");
  page += F(">ISO15693</label></div>");
  page += F("<div class='note' data-hfrole='scan'>These protocol switches are used only by Scan mode.</div>");
  page += F("<div class='note'>HF SPI/I2C speed is fixed by command-line settings and is not shown here.</div></section>");

  page += F("<section><h2>Hotspot</h2>");
  page += F("<div class='row'><label>AP SSID</label><input name='portal_ssid' value='");
  page += htmlEscape(ap_ssid);
  page += F("'></div><div class='row'><label>AP password</label><input name='portal_password' type='password' value='");
  page += htmlEscape(config_.configPortalPassword);
  page += F("'></div><div class='row'><label>Portal port</label><input name='portal_port' type='number' min='1' max='65535' value='");
  page += String(config_.configPortalPort == 0 ? 80 : config_.configPortalPort);
  page += F("'></div></section>");

  page += F("<div class='actions'><button type='submit'>Save</button></form>");
  page += F("<form method='post' action='/reboot'><button class='secondary' type='submit'>Reboot</button></form></div>");
  page += F("</main></body></html>");

  portalServer_->send(200, "text/html", page);
}

void NetworkRfidReader::restartStation() {
  restartTcpSocket();
  if (config_.wifiSsid.length() == 0) {
    wifiConnectInProgress_ = false;
    WiFi.disconnect(false, true);
    if (portalRunning_) {
      WiFi.mode(WIFI_AP);
    }
    return;
  }

  wifiConnectInProgress_ = false;
  WiFi.disconnect(false, false);
  delay(50);
  startWifi();
}

String NetworkRfidReader::defaultPortalSsid() const {
  return String(F("ELECHOUSE_RFID"));
}

void NetworkRfidReader::startWifi() {
  if (config_.wifiSsid.length() == 0) {
    return;
  }
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress_ = false;
    return;
  }
  if (wifiConnectInProgress_ && (millis() - lastWifiBeginMs_) < kStationConnectTimeoutMs) {
    return;
  }

  WiFi.setHostname(NETWORK_RFID_WIFI_HOSTNAME);
  WiFi.mode(portalRunning_ ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(true);
  lastWifiDisconnectReason_ = 0;
  WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPassword.c_str());
  lastWifiBeginMs_ = millis();
  wifiConnectInProgress_ = true;
}

void NetworkRfidReader::serviceNetwork() {
  serviceWifiStation();

  if (config_.tcpMode == NetworkRfidTcpMode::Off) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (config_.tcpMode == NetworkRfidTcpMode::Server) {
    if (server_ == nullptr || activeServerPort_ != config_.tcpListenPort) {
      restartTcpSocket();
    }
    if (server_ != nullptr) {
      WiFiClient incoming = server_->available();
      if (incoming) {
        if (serverClient_.connected()) {
          serverClient_.stop();
        }
        serverClient_ = incoming;
        serverClient_.setNoDelay(true);
        serverClientLine_ = "";
      }
    }
    serviceTcpCommands();
  } else if (config_.tcpMode == NetworkRfidTcpMode::Client ||
             config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    ensureTcpClient();
    serviceTcpCommands();
  }
}

void NetworkRfidReader::serviceWifiStation() {
  if (config_.wifiSsid.length() == 0) {
    return;
  }

  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress_ = false;
    return;
  }

  const uint32_t now = millis();
  const uint32_t reconnect_ms = config_.tcpReconnectMs < 1000 ? 1000 : config_.tcpReconnectMs;
  if (wifiConnectInProgress_ && (now - lastWifiBeginMs_) < kStationConnectTimeoutMs) {
    return;
  }
  if ((now - lastWifiBeginMs_) < reconnect_ms) {
    return;
  }

  wifiConnectInProgress_ = false;
  WiFi.disconnect(false, false);
  delay(10);
  startWifi();
}

void NetworkRfidReader::restartTcpSocket() {
  if (tcpClient_.connected()) {
    tcpClient_.stop();
  }
  if (serverClient_.connected()) {
    serverClient_.stop();
  }
  if (server_ != nullptr) {
    server_->stop();
    delete server_;
    server_ = nullptr;
  }
  activeServerPort_ = 0;
  tcpClientLine_ = "";
  serverClientLine_ = "";
  elechouseHelloSent_ = false;
  elechouseBrokerOk_ = false;
  elechouseLastPingMs_ = 0;

  if (config_.tcpMode == NetworkRfidTcpMode::Server && WiFi.status() == WL_CONNECTED) {
    if (portalRunning_ && config_.tcpListenPort == (config_.configPortalPort == 0 ? 80 : config_.configPortalPort)) {
      if (console_ != nullptr) {
        console_->println(F("ERR tcp server port conflicts with portal port"));
      }
      return;
    }
    server_ = new WiFiServer(config_.tcpListenPort);
    server_->begin();
    activeServerPort_ = config_.tcpListenPort;
  }
}

bool NetworkRfidReader::ensureTcpClient() {
  const bool elechouse_mode = config_.tcpMode == NetworkRfidTcpMode::ElechouseTest;
  if ((config_.tcpMode != NetworkRfidTcpMode::Client && !elechouse_mode) ||
      WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (elechouse_mode) {
    if (config_.elechouseSessionCode.length() == 0) {
      return false;
    }
  } else if (config_.tcpHost.length() == 0) {
    return false;
  }
  if (tcpClient_.connected()) {
    return true;
  }

  const uint32_t now = millis();
  const uint32_t reconnect_ms = config_.tcpReconnectMs < 1000 ? 1000 : config_.tcpReconnectMs;
  if ((now - lastTcpConnectMs_) < reconnect_ms) {
    return false;
  }

  lastTcpConnectMs_ = now;
  tcpClient_.stop();
  elechouseHelloSent_ = false;
  elechouseBrokerOk_ = false;
  const char* host = elechouse_mode ? kElechouseBrokerHost : config_.tcpHost.c_str();
  const uint16_t port = elechouse_mode ? kElechouseBrokerPort : config_.tcpPort;
  if (tcpClient_.connect(host, port)) {
    tcpClient_.setNoDelay(true);
    tcpClientLine_ = "";
    if (elechouse_mode) {
      sendElechouseHello();
    }
    return true;
  }
  return false;
}

void NetworkRfidReader::sendNetworkLine(const String& line) {
  if (config_.tcpMode == NetworkRfidTcpMode::Off || !config_.tcpEchoEvents) {
    return;
  }

  if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    if (ensureTcpClient() && elechouseBrokerOk_) {
      tcpClient_.print(line);
    }
    return;
  }

  if (config_.tcpMode == NetworkRfidTcpMode::Client) {
    if (ensureTcpClient()) {
      tcpClient_.print(line);
    }
  } else if (config_.tcpMode == NetworkRfidTcpMode::Server) {
    if (serverClient_.connected()) {
      serverClient_.print(line);
    }
  }
}

void NetworkRfidReader::serviceTcpCommands() {
  if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    serviceElechouseBroker();
    return;
  }

  if (!config_.tcpCommands) {
    return;
  }

  if (config_.tcpMode == NetworkRfidTcpMode::Client && tcpClient_.connected()) {
    handleSerialStream(tcpClient_, tcpClientLine_);
  } else if (config_.tcpMode == NetworkRfidTcpMode::Server && serverClient_.connected()) {
    handleSerialStream(serverClient_, serverClientLine_);
  }
}

void NetworkRfidReader::serviceElechouseBroker() {
  if (config_.tcpMode != NetworkRfidTcpMode::ElechouseTest) {
    return;
  }
  if (!ensureTcpClient()) {
    return;
  }

  while (tcpClient_.available() > 0) {
    const int value = tcpClient_.read();
    if (value < 0) {
      return;
    }
    const char c = static_cast<char>(value);
    if (c == '\r' || c == '\n') {
      tcpClientLine_.trim();
      if (tcpClientLine_.length() > 0) {
        const bool handled = handleElechouseBrokerLine(tcpClientLine_);
        if (!handled && config_.tcpCommands) {
          Stream* previous_console = console_;
          console_ = &tcpClient_;
          handleCommand(tcpClientLine_);
          console_ = previous_console;
        }
      }
      tcpClientLine_ = "";
    } else if (c >= 32 && c <= 126) {
      if (tcpClientLine_.length() < 220) {
        tcpClientLine_ += c;
      } else {
        tcpClientLine_ = "";
      }
    }
  }

  if (elechouseBrokerOk_ && (millis() - elechouseLastPingMs_) >= kElechouseHeartbeatMs) {
    tcpClient_.print(F("PING\n"));
    elechouseLastPingMs_ = millis();
  }
}

bool NetworkRfidReader::handleElechouseBrokerLine(const String& line) {
  if (!elechouseBrokerOk_) {
    if (line.startsWith("OK ")) {
      elechouseBrokerOk_ = true;
      elechouseLastPingMs_ = millis();
      if (console_ != nullptr) {
        console_->print(F("ELECHOUSE broker "));
        console_->println(line);
      }
      return true;
    }
    if (line.startsWith("ERR ")) {
      if (console_ != nullptr) {
        console_->print(F("ELECHOUSE broker "));
        console_->println(line);
      }
      tcpClient_.stop();
      elechouseHelloSent_ = false;
      elechouseBrokerOk_ = false;
      return true;
    }
    return true;
  }

  if (line == "PONG" || line.startsWith("OK ")) {
    return true;
  }
  if (line == "PING") {
    tcpClient_.print(F("PONG\n"));
    return true;
  }
  return false;
}

void NetworkRfidReader::sendElechouseHello() {
  if (!tcpClient_.connected() || config_.elechouseSessionCode.length() == 0) {
    return;
  }
  String hello = F("HELLO ");
  hello += config_.elechouseSessionCode;
  hello += ' ';
  hello += elechouseDeviceId();
  hello += '\n';
  tcpClient_.print(hello);
  elechouseHelloSent_ = true;
  elechouseBrokerOk_ = false;
  elechouseLastPingMs_ = millis();
}

void NetworkRfidReader::sendElechouseCard(const RfidCardEvent& event) {
  if (config_.tcpMode != NetworkRfidTcpMode::ElechouseTest || !config_.tcpEchoEvents) {
    return;
  }
  if (ensureTcpClient() && elechouseBrokerOk_) {
    tcpClient_.print(formatElechouseCardEvent(event));
  }
}

String NetworkRfidReader::formatElechouseCardEvent(const RfidCardEvent& event) const {
  String line = F("{\"type\":\"card\",\"band\":\"");
  line += jsonEscape(event.band);
  line += F("\",\"card_type\":\"");
  line += jsonEscape(event.type);
  line += F("\",\"uid\":\"");
  line += jsonEscape(event.id);
  line += F("\",\"ms\":");
  line += String(event.ms);
  line += F("}\n");
  return line;
}

String NetworkRfidReader::elechouseDeviceId() const {
  String id = NETWORK_RFID_WIFI_HOSTNAME;
  id += '-';
  id += WiFi.macAddress();
  return id;
}

void NetworkRfidReader::setupFeedback() {
  feedbackLedReady_ = false;
  feedbackBuzzerActive_ = false;
  feedbackShowingSuccess_ = false;
  feedbackBuzzerStopMs_ = 0;
  feedbackSuccessUntilMs_ = 0;

  if (config_.pins.feedbackBuzzer >= 0) {
    pinMode(config_.pins.feedbackBuzzer, OUTPUT);
    digitalWrite(config_.pins.feedbackBuzzer, LOW);
  }

  if (config_.pins.feedbackLed >= 0) {
#if NETWORK_RFID_HAS_RMT
    feedbackLedReady_ = rmtInit(config_.pins.feedbackLed, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, kWs2816RmtHz);
    if (feedbackLedReady_) {
      rmtSetEOT(config_.pins.feedbackLed, LOW);
    } else if (console_ != nullptr) {
      console_->println(F("ERR feedback LED RMT init failed"));
    }
#else
    if (console_ != nullptr) {
      console_->println(F("ERR feedback LED requires Arduino ESP32 core 3 RMT"));
    }
#endif
  }

  if (config_.feedbackEnabled) {
    showFeedbackIdle();
  } else {
    setFeedbackLed(0, 0, 0);
  }
}

void NetworkRfidReader::serviceFeedback() {
  const uint32_t now = millis();
  if (feedbackBuzzerActive_ && static_cast<int32_t>(now - feedbackBuzzerStopMs_) >= 0) {
    stopFeedbackBuzzer();
  }
  if (feedbackShowingSuccess_ && static_cast<int32_t>(now - feedbackSuccessUntilMs_) >= 0) {
    feedbackShowingSuccess_ = false;
    showFeedbackIdle();
  }
}

void NetworkRfidReader::triggerFeedbackSuccess() {
  if (!config_.feedbackEnabled) {
    return;
  }

  feedbackShowingSuccess_ = true;
  feedbackSuccessUntilMs_ = millis() + config_.feedbackSuccessMs;
  setFeedbackLed(config_.feedbackSuccessRed, config_.feedbackSuccessGreen, config_.feedbackSuccessBlue);
  startFeedbackBuzzer();
}

void NetworkRfidReader::showFeedbackIdle() {
  if (!config_.feedbackEnabled) {
    return;
  }
  setFeedbackLed(config_.feedbackIdleRed, config_.feedbackIdleGreen, config_.feedbackIdleBlue);
}

void NetworkRfidReader::setFeedbackLed(uint16_t red, uint16_t green, uint16_t blue) {
#if NETWORK_RFID_HAS_RMT
  if (!feedbackLedReady_ || config_.pins.feedbackLed < 0) {
    return;
  }

  rmt_data_t symbols[kWs2816BitsPerPixel] = {};
  size_t index = 0;
  appendWs2816Word(symbols, index, green);
  appendWs2816Word(symbols, index, red);
  appendWs2816Word(symbols, index, blue);
  if (!rmtWrite(config_.pins.feedbackLed, symbols, index, RMT_WAIT_FOR_EVER) && console_ != nullptr) {
    console_->println(F("ERR feedback LED write failed"));
  }
  delayMicroseconds(300);
#else
  (void)red;
  (void)green;
  (void)blue;
#endif
}

void NetworkRfidReader::startFeedbackBuzzer() {
  if (!config_.feedbackEnabled || config_.pins.feedbackBuzzer < 0 || config_.feedbackBuzzerMs == 0) {
    return;
  }

  feedbackBuzzerStopMs_ = millis() + config_.feedbackBuzzerMs;
  if (feedbackBuzzerActive_) {
    return;
  }

  const uint32_t hz = config_.feedbackBuzzerHz == 0 ? 2700UL : config_.feedbackBuzzerHz;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(config_.pins.feedbackBuzzer, hz, 8)) {
    if (console_ != nullptr) {
      console_->println(F("ERR feedback buzzer ledcAttach failed"));
    }
    return;
  }
  ledcWrite(config_.pins.feedbackBuzzer, 128);
#else
  ledcSetup(config_.pins.feedbackBuzzerLedcChannel, hz, 8);
  ledcAttachPin(config_.pins.feedbackBuzzer, config_.pins.feedbackBuzzerLedcChannel);
  ledcWrite(config_.pins.feedbackBuzzerLedcChannel, 128);
#endif
  feedbackBuzzerActive_ = true;
}

void NetworkRfidReader::stopFeedbackBuzzer() {
  if (config_.pins.feedbackBuzzer < 0) {
    feedbackBuzzerActive_ = false;
    return;
  }

  if (feedbackBuzzerActive_) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(config_.pins.feedbackBuzzer, 0);
    ledcDetach(config_.pins.feedbackBuzzer);
#else
    ledcWrite(config_.pins.feedbackBuzzerLedcChannel, 0);
    ledcDetachPin(config_.pins.feedbackBuzzer);
#endif
  }
  pinMode(config_.pins.feedbackBuzzer, OUTPUT);
  digitalWrite(config_.pins.feedbackBuzzer, LOW);
  feedbackBuzzerActive_ = false;
}

void NetworkRfidReader::beepFeedback(uint8_t count) {
  if (!config_.feedbackEnabled ||
      config_.pins.feedbackBuzzer < 0 ||
      config_.feedbackBuzzerMs == 0 ||
      count == 0) {
    return;
  }

  for (uint8_t i = 0; i < count; i++) {
    stopFeedbackBuzzer();
    startFeedbackBuzzer();
    delay(config_.feedbackBuzzerMs);
    stopFeedbackBuzzer();
    if ((i + 1U) < count) {
      delay(kButtonBeepGapMs);
    }
  }
}

void NetworkRfidReader::setupButton() {
  buttonLastRawPressed_ = false;
  buttonStablePressed_ = false;
  buttonWifiConfigFired_ = false;
  buttonFactoryResetFired_ = false;
  buttonLastRawChangeMs_ = millis();
  buttonPressStartMs_ = 0;

  if (!config_.configButtonEnabled || config_.pins.configButton < 0) {
    return;
  }

  pinMode(config_.pins.configButton, config_.pins.configButtonActiveLow ? INPUT_PULLUP : INPUT);
  buttonLastRawPressed_ = readConfigButtonPressed();
  buttonStablePressed_ = buttonLastRawPressed_;
  if (buttonStablePressed_) {
    buttonPressStartMs_ = millis();
  }
}

void NetworkRfidReader::serviceButton() {
  if (!config_.configButtonEnabled || config_.pins.configButton < 0) {
    return;
  }

  const uint32_t now = millis();
  const bool raw_pressed = readConfigButtonPressed();
  if (raw_pressed != buttonLastRawPressed_) {
    buttonLastRawPressed_ = raw_pressed;
    buttonLastRawChangeMs_ = now;
    return;
  }
  if ((now - buttonLastRawChangeMs_) < kButtonDebounceMs) {
    return;
  }

  if (raw_pressed != buttonStablePressed_) {
    buttonStablePressed_ = raw_pressed;
    if (buttonStablePressed_) {
      buttonPressStartMs_ = now;
      buttonWifiConfigFired_ = false;
      buttonFactoryResetFired_ = false;
    } else {
      buttonPressStartMs_ = 0;
      buttonWifiConfigFired_ = false;
      buttonFactoryResetFired_ = false;
    }
  }

  if (!buttonStablePressed_ || buttonPressStartMs_ == 0) {
    return;
  }

  const uint32_t held_ms = now - buttonPressStartMs_;
  if (!buttonWifiConfigFired_ && held_ms >= config_.buttonWifiConfigMs) {
    buttonWifiConfigFired_ = true;
    beepFeedback(1);
    enterDefaultWifiConfig();
  }
  if (!buttonFactoryResetFired_ && held_ms >= config_.buttonFactoryResetMs) {
    buttonFactoryResetFired_ = true;
    beepFeedback(2);
    restoreDefaultConfig();
  }
}

bool NetworkRfidReader::readConfigButtonPressed() const {
  if (!config_.configButtonEnabled || config_.pins.configButton < 0) {
    return false;
  }
  const int level = digitalRead(config_.pins.configButton);
  return config_.pins.configButtonActiveLow ? (level == LOW) : (level == HIGH);
}

void NetworkRfidReader::enterDefaultWifiConfig() {
  config_.configPortalEnabled = true;
  config_.configPortalSsid = "";
  config_.configPortalPassword = "";
  config_.configPortalPort = 80;

  if (portalRunning_) {
    stopConfigPortal();
  }
  startConfigPortal();

  if (console_ != nullptr) {
    console_->println(F("BUTTON wifi config portal: SSID=ELECHOUSE_RFID IP=10.10.10.10"));
  }
}

void NetworkRfidReader::restoreDefaultConfig() {
  if (console_ != nullptr) {
    console_->println(F("BUTTON factory reset: clearing saved config and rebooting"));
  }
  clearSavedConfig();
  delay(150);
  ESP.restart();
}

void NetworkRfidReader::setupHardwareUart() {
  releaseHardwareUart();
  hardwareUartLine_ = "";

  if (!config_.hardwareUartEnabled ||
      config_.productInterfaceMode != NetworkRfidProductInterfaceMode::Uart ||
      (config_.pins.hardwareUartRx < 0 && config_.pins.hardwareUartTx < 0)) {
    return;
  }

  const uint32_t baud = config_.hardwareUartBaud == 0 ? 115200UL : config_.hardwareUartBaud;
  hardwareUart_.begin(baud, SERIAL_8N1,
                      static_cast<int8_t>(config_.pins.hardwareUartRx),
                      static_cast<int8_t>(config_.pins.hardwareUartTx));
  hardwareUartReady_ = true;

  if (console_ != nullptr) {
    console_->print(F("UART1 ready baud="));
    console_->print(baud);
    console_->print(F(" rx="));
    console_->print(config_.pins.hardwareUartRx);
    console_->print(F(" tx="));
    console_->println(config_.pins.hardwareUartTx);
  }
}

void NetworkRfidReader::releaseHardwareUart() {
  if (hardwareUartReady_) {
    hardwareUart_.flush();
    hardwareUart_.end();
  }
  hardwareUartReady_ = false;
  hardwareUartLine_ = "";
}

void NetworkRfidReader::restartHardwareUart() {
  config_.productInterfaceMode = NetworkRfidProductInterfaceMode::Uart;
  restartProductInterface();
}

void NetworkRfidReader::setupProductInterface() {
  releaseProductInterface();

  if (!config_.hardwareUartEnabled) {
    return;
  }

  if (config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Uart) {
    setupHardwareUart();
  } else {
    setupPulseInterface();
  }
}

void NetworkRfidReader::releaseProductInterface() {
  releaseHardwareUart();
  releasePulseInterface();
}

void NetworkRfidReader::restartProductInterface() {
  if (config_.hardwareUartEnabled) {
    setupProductInterface();
  } else {
    releaseProductInterface();
  }
}

void NetworkRfidReader::setupPulseInterface() {
  productInterfacePulseReady_ = false;

  if (!config_.hardwareUartEnabled ||
      config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Uart ||
      config_.pins.hardwareUartRx < 0 ||
      config_.pins.hardwareUartTx < 0) {
    return;
  }

  pinMode(config_.pins.hardwareUartRx, OUTPUT_OPEN_DRAIN);
  pinMode(config_.pins.hardwareUartTx, OUTPUT_OPEN_DRAIN);
  digitalWrite(config_.pins.hardwareUartRx, HIGH);
  digitalWrite(config_.pins.hardwareUartTx, HIGH);
  productInterfacePulseReady_ = true;

  if (console_ != nullptr) {
    console_->print(F("Product interface ready mode="));
    console_->print(productInterfaceModeName(config_.productInterfaceMode));
    console_->print(F(" d0/clock="));
    console_->print(config_.pins.hardwareUartRx);
    console_->print(F(" d1/data="));
    console_->println(config_.pins.hardwareUartTx);
  }
}

void NetworkRfidReader::releasePulseInterface() {
  if (productInterfacePulseReady_) {
    if (config_.pins.hardwareUartRx >= 0) {
      pinMode(config_.pins.hardwareUartRx, INPUT);
    }
    if (config_.pins.hardwareUartTx >= 0) {
      pinMode(config_.pins.hardwareUartTx, INPUT);
    }
  }
  productInterfacePulseReady_ = false;
}

void NetworkRfidReader::sendProductInterfaceEvent(const RfidCardEvent& event) {
  if (!config_.hardwareUartEnabled || !config_.hardwareUartEchoEvents) {
    return;
  }

  if (config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Wiegand) {
    sendWiegandEvent(event);
  } else if (config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Aba) {
    sendAbaEvent(event);
  }
}

bool NetworkRfidReader::sendWiegandEvent(const RfidCardEvent& event) {
  if (!productInterfacePulseReady_ || !isSupportedWiegandBits(config_.wiegandBits)) {
    return false;
  }

  uint64_t value = 0;
  if (!eventDataValue(event, value)) {
    return false;
  }

  const uint8_t data_bits = config_.wiegandBits - 2U;
  if (data_bits < 64) {
    value &= (1ULL << data_bits) - 1ULL;
  }

  const uint8_t first_half = data_bits / 2U;
  const uint8_t second_half = data_bits - first_half;
  const uint64_t second_mask = second_half >= 64 ? UINT64_MAX : ((1ULL << second_half) - 1ULL);
  const uint64_t second_value = value & second_mask;
  const uint64_t first_value = value >> second_half;

  const bool even_parity = (countBits64(first_value) & 1U) != 0;
  const bool odd_parity = (countBits64(second_value) & 1U) == 0;

  sendWiegandBit(even_parity);
  for (int bit = data_bits - 1; bit >= 0; --bit) {
    sendWiegandBit((value & (1ULL << bit)) != 0);
  }
  sendWiegandBit(odd_parity);
  return true;
}

bool NetworkRfidReader::sendAbaEvent(const RfidCardEvent& event) {
  if (!productInterfacePulseReady_) {
    return false;
  }

  String digits = eventDataDecimal(event, config_.abaUseCardNumber);
  if (digits.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < digits.length(); i++) {
    const char c = digits.charAt(i);
    if (c < '0' || c > '9') {
      return false;
    }
  }

  if (config_.abaDigits > 0) {
    while (digits.length() < config_.abaDigits) {
      digits = String('0') + digits;
    }
    if (digits.length() > config_.abaDigits) {
      digits = digits.substring(digits.length() - config_.abaDigits);
    }
  }

  uint8_t lrc = 0;
  sendAbaNibble(0x0B, lrc, true);
  for (size_t i = 0; i < digits.length(); i++) {
    sendAbaNibble(static_cast<uint8_t>(digits.charAt(i) - '0'), lrc, true);
    if ((i & 0x07U) == 0x07U) {
      delay(0);
    }
  }
  sendAbaNibble(0x0F, lrc, true);
  sendAbaNibble(lrc & 0x0F, lrc, false);
  digitalWrite(config_.pins.hardwareUartRx, HIGH);
  digitalWrite(config_.pins.hardwareUartTx, HIGH);
  return true;
}

void NetworkRfidReader::sendWiegandBit(bool bit) {
  const int pin = bit ? config_.pins.hardwareUartTx : config_.pins.hardwareUartRx;
  digitalWrite(pin, LOW);
  delayMicroseconds(config_.productPulseUs);
  digitalWrite(pin, HIGH);
  delayMicroseconds(config_.productPulseGapUs);
}

void NetworkRfidReader::sendAbaNibble(uint8_t value, uint8_t& lrc, bool includeInLrc) {
  value &= 0x0F;
  if (includeInLrc) {
    lrc ^= value;
  }

  uint8_t parity_count = 0;
  for (uint8_t bit = 0; bit < 4; bit++) {
    const bool one = (value & (1U << bit)) != 0;
    if (one) {
      parity_count++;
    }
    sendAbaBit(one);
  }
  sendAbaBit((parity_count & 1U) == 0);
}

void NetworkRfidReader::sendAbaBit(bool bit) {
  digitalWrite(config_.pins.hardwareUartTx, bit ? LOW : HIGH);
  delayMicroseconds(20);
  digitalWrite(config_.pins.hardwareUartRx, LOW);
  delayMicroseconds(config_.productPulseUs);
  digitalWrite(config_.pins.hardwareUartRx, HIGH);
  delayMicroseconds(config_.productPulseGapUs);
  digitalWrite(config_.pins.hardwareUartTx, HIGH);
}

void NetworkRfidReader::emitCard(const char* band, const char* type, const String& id) {
  RfidCardEvent event;
  event.band = band;
  event.type = type;
  event.id = id;
  event.ms = millis();

  if (shouldSuppressDuplicate(event)) {
    return;
  }

  triggerFeedbackSuccess();

  const String line = formatEvent(event);
  if (config_.echoEventsToSerial && primaryConsole_ != nullptr) {
    primaryConsole_->print(line);
  }
  if (config_.hardwareUartEchoEvents &&
      config_.productInterfaceMode == NetworkRfidProductInterfaceMode::Uart &&
      hardwareUartReady_) {
    hardwareUart_.print(line);
  }
  sendProductInterfaceEvent(event);
  if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    sendElechouseCard(event);
  } else {
    sendNetworkLine(line);
  }

  if (callback_ != nullptr) {
    callback_(event, callbackContext_);
  }
}

bool NetworkRfidReader::shouldSuppressDuplicate(const RfidCardEvent& event) {
  String key = event.band;
  key += '|';
  key += event.type;
  key += '|';
  key += event.id;

  lastEventKey_ = key;
  lastEventMs_ = event.ms;

  if (config_.duplicateSuppressMs == 0) {
    return false;
  }

  for (size_t i = 0; i < DuplicateCacheSize; i++) {
    if (duplicateKeys_[i] == key) {
      if ((event.ms - duplicateMs_[i]) < config_.duplicateSuppressMs) {
        return true;
      }
      duplicateMs_[i] = event.ms;
      return false;
    }
  }

  duplicateKeys_[duplicateNext_] = key;
  duplicateMs_[duplicateNext_] = event.ms;
  duplicateNext_ = (duplicateNext_ + 1U) % DuplicateCacheSize;
  return false;
}

String NetworkRfidReader::formatEvent(const RfidCardEvent& event) const {
  if (config_.outputFormat == NetworkRfidOutputFormat::CsvLine) {
    String line = event.band;
    line += ',';
    line += event.type;
    line += ',';
    line += event.id;
    line += ',';
    line += String(event.ms);
    line += '\n';
    return line;
  }

  String line = F("{\"band\":\"");
  line += jsonEscape(event.band);
  line += F("\",\"type\":\"");
  line += jsonEscape(event.type);
  line += F("\",\"id\":\"");
  line += jsonEscape(event.id);
  line += F("\",\"ms\":");
  line += String(event.ms);
  line += F("}\n");
  return line;
}

void NetworkRfidReader::printHelp() {
  if (console_ == nullptr) {
    return;
  }
  console_->println();
  console_->println(F("ESP32S3 LF/HF Network RFID"));
  console_->println(F("Commands:"));
  console_->println(F("  help | status | pins"));
  console_->println(F("  wifi status | wifi scan [ssid] | wifi set <ssid> <password> | wifi reconnect | wifi clear"));
  console_->println(F("  tcp status | tcp client <host> <port> | tcp server <port> | tcp off | tcp events on|off | tcp commands on|off"));
  console_->println(F("  elechouse status | elechouse on <session_code> | elechouse off | elechouse reconnect | elechouse clear"));
  console_->println(F("  interface status|mode uart|wiegand|aba|on|off|events on|off|commands on|off|baud <baud>"));
  console_->println(F("  interface pulse <us> <gap_us> | interface wiegand bits <26|34|37|56> | interface aba digits <0..32> | interface aba source raw|cn"));
  console_->println(F("  feedback status|on|off|buzzer <hz> <ms>|success_ms <ms>|idle <r> <g> <b>|success <r> <g> <b>|test"));
  console_->println(F("  button status|on|off|timing <wifi_ms> <reset_ms>"));
  console_->println(F("  portal on|off|status|ssid <ssid> [password]"));
  console_->println(F("  lf init | lf off | lf status | lf freq <hz> | lf scan [start stop step ms]"));
  console_->println(F("  lf raw <count> | lf hid [ms] | lf indala [samples]"));
  console_->println(F("  hf probe | hf speed <hz> | hf init | hf off | hf status"));
  console_->println(F("  hf mode scan|card | hf tech a|b|f|v on|off"));
  console_->println(F("  hf card status|uid <hex>|type nfc-a-t4t|nfc-a-t2t|ndef url|text|vcard|wifi <payload>"));
  console_->println(F("  format json|line"));
  console_->println(F("  window <lf_ms> <hf_ms>"));
  console_->println(F("  dedupe <ms>"));
  console_->println(F("  auto lf|hf on|off"));
  console_->println(F("  save | load | clear | reboot | test"));
}

void NetworkRfidReader::printStatus() {
  if (console_ == nullptr) {
    return;
  }

  uint32_t captured = 0;
  uint32_t dropped = 0;
  noInterrupts();
  captured = capturedPulses_;
  dropped = droppedPulses_;
  interrupts();

  console_->print(F("slot="));
  console_->print(slotName(activeSlot_));
  console_->print(F(" hfBus="));
  console_->print(hfBusModeName(config_.pins.hfBusMode));
  console_->print(F(" hfRole="));
  console_->print(hfRoleName(config_.hfRole));
  console_->print(F(" hfReady="));
  console_->print(hfReady_ ? F("yes") : F("no"));
  console_->print(F(" lfCarrier="));
  console_->print(lfCarrierEnabled_ ? F("on") : F("off"));
  console_->print(F(" lfCapture="));
  console_->print(lfCaptureEnabled_ ? F("on") : F("off"));
  console_->print(F(" pulses="));
  console_->print(captured);
  console_->print(F(" dropped="));
  console_->println(dropped);

  console_->print(F("wifi="));
  if (WiFi.status() == WL_CONNECTED) {
    console_->print(WiFi.localIP().toString());
  } else {
    console_->print(wifiStatusName(WiFi.status()));
    console_->print(F("("));
    console_->print(static_cast<int>(WiFi.status()));
    console_->print(F(")"));
  }
  console_->print(F(" tcp="));
  console_->print(tcpModeName(config_.tcpMode));
  console_->print(F(" tcpEvents="));
  console_->print(config_.tcpEchoEvents ? F("on") : F("off"));
  console_->print(F(" tcpCommands="));
  console_->print(config_.tcpCommands ? F("on") : F("off"));
  console_->print(F(" clientConnected="));
  console_->print(tcpClient_.connected() ? F("yes") : F("no"));
  console_->print(F(" serverClient="));
  console_->print(serverClient_.connected() ? F("yes") : F("no"));
  if (config_.tcpMode == NetworkRfidTcpMode::ElechouseTest) {
    console_->print(F(" ehSession="));
    console_->print(config_.elechouseSessionCode);
    console_->print(F(" ehOk="));
    console_->print(elechouseBrokerOk_ ? F("yes") : F("no"));
  }
  console_->println();

  console_->print(F("portal="));
  console_->print(portalRunning_ ? WiFi.softAPIP().toString() : String(F("off")));
  console_->print(F(" ssid="));
  console_->println(config_.configPortalSsid.length() > 0 ? config_.configPortalSsid : defaultPortalSsid());

  console_->print(F("feedback="));
  console_->print(config_.feedbackEnabled ? F("on") : F("off"));
  console_->print(F(" led="));
  console_->print(config_.pins.feedbackLed);
  console_->print(F(" buzzer="));
  console_->print(config_.pins.feedbackBuzzer);
  console_->print(F(" buzzerHz="));
  console_->println(config_.feedbackBuzzerHz);

  console_->print(F("button="));
  console_->print(config_.configButtonEnabled ? F("on") : F("off"));
  console_->print(F(" pin="));
  console_->print(config_.pins.configButton);
  console_->print(F(" pressed="));
  console_->print(readConfigButtonPressed() ? F("yes") : F("no"));
  console_->print(F(" wifiMs="));
  console_->print(config_.buttonWifiConfigMs);
  console_->print(F(" resetMs="));
  console_->println(config_.buttonFactoryResetMs);

  console_->print(F("interface="));
  console_->print(productInterfaceModeName(config_.productInterfaceMode));
  console_->print(F(" enabled="));
  console_->print(config_.hardwareUartEnabled ? F("on") : F("off"));
  console_->print(F(" uartReady="));
  console_->print(hardwareUartReady_ ? F("yes") : F("no"));
  console_->print(F(" pulseReady="));
  console_->print(productInterfacePulseReady_ ? F("yes") : F("no"));
  console_->print(F(" baud="));
  console_->print(config_.hardwareUartBaud);
  console_->print(F(" rxD0Clock="));
  console_->print(config_.pins.hardwareUartRx);
  console_->print(F(" txD1Data="));
  console_->print(config_.pins.hardwareUartTx);
  console_->print(F(" events="));
  console_->print(config_.hardwareUartEchoEvents ? F("on") : F("off"));
  console_->print(F(" commands="));
  console_->print(config_.hardwareUartCommands ? F("on") : F("off"));
  console_->print(F(" wgBits="));
  console_->print(config_.wiegandBits);
  console_->print(F(" pulseUs="));
  console_->print(config_.productPulseUs);
  console_->print(F(" gapUs="));
  console_->print(config_.productPulseGapUs);
  console_->print(F(" abaDigits="));
  if (config_.abaDigits == 0) {
    console_->print(F("auto"));
  } else {
    console_->print(config_.abaDigits);
  }
  console_->print(F(" abaSource="));
  console_->println(config_.abaUseCardNumber ? F("cn") : F("raw"));
}

void NetworkRfidReader::printPins() {
  if (console_ == nullptr) {
    return;
  }
  console_->print(F("LF OUT="));
  console_->print(config_.pins.lfOut);
  console_->print(F(" DATA="));
  console_->print(config_.pins.lfData);
  console_->print(F(" PULL="));
  console_->print(config_.pins.lfPull);
  console_->print(F(" ADC="));
  console_->print(config_.pins.lfAdc);
  console_->print(F(" CARRIER="));
  console_->println(config_.pins.lfCarrierDetect);

  console_->print(F("HF mode="));
  console_->print(hfBusModeName(config_.pins.hfBusMode));
  console_->print(F(" spiBus="));
  console_->print(config_.pins.hfSpiBus);
  console_->print(F(" SCK/SCL="));
  console_->print(config_.pins.hfSck);
  console_->print(F(" MOSI="));
  console_->print(config_.pins.hfMosi);
  console_->print(F(" MISO/SDA="));
  console_->print(config_.pins.hfMiso);
  console_->print(F(" CS="));
  console_->print(config_.pins.hfCs);
  console_->print(F(" IRQ="));
  console_->print(config_.pins.hfIrq);
  console_->print(F(" RESET="));
  console_->print(config_.pins.hfReset);
  console_->print(F(" SPI="));
  console_->print(config_.hfSpiHz);
  console_->print(F(" I2C="));
  console_->println(config_.hfI2cHz);

  console_->print(F("FEEDBACK LED="));
  console_->print(config_.pins.feedbackLed);
  console_->print(F(" BUZZER="));
  console_->print(config_.pins.feedbackBuzzer);
  console_->print(F(" BUZZER_CH="));
  console_->println(config_.pins.feedbackBuzzerLedcChannel);

  console_->print(F("BUTTON="));
  console_->print(config_.pins.configButton);
  console_->print(F(" ACTIVE_LOW="));
  console_->println(config_.pins.configButtonActiveLow ? F("yes") : F("no"));

  console_->print(F("PRODUCT RX/D0/CLOCK="));
  console_->print(config_.pins.hardwareUartRx);
  console_->print(F(" TX/D1/DATA="));
  console_->print(config_.pins.hardwareUartTx);
  console_->print(F(" BAUD="));
  console_->print(config_.hardwareUartBaud);
  console_->print(F(" MODE="));
  console_->println(productInterfaceModeName(config_.productInterfaceMode));
}

bool NetworkRfidReader::parseBoolForm(WebServer* server, const char* name) {
  return server != nullptr && server->hasArg(name);
}

uint16_t NetworkRfidReader::parsePortForm(WebServer* server, const char* name, uint16_t fallback) {
  if (server == nullptr || !server->hasArg(name)) {
    return fallback;
  }
  const int value = server->arg(name).toInt();
  if (value <= 0 || value > 65535) {
    return fallback;
  }
  return static_cast<uint16_t>(value);
}

uint32_t NetworkRfidReader::parseUIntForm(WebServer* server, const char* name, uint32_t fallback) {
  if (server == nullptr || !server->hasArg(name)) {
    return fallback;
  }
  String value = server->arg(name);
  value.trim();
  if (value.length() == 0) {
    return fallback;
  }
  return static_cast<uint32_t>(value.toInt());
}

String NetworkRfidReader::nextToken(String& rest) {
  rest.trim();
  if (rest.length() == 0) {
    return "";
  }
  const int pos = rest.indexOf(' ');
  if (pos < 0) {
    const String token = rest;
    rest = "";
    return token;
  }
  const String token = rest.substring(0, pos);
  rest = rest.substring(pos + 1);
  rest.trim();
  return token;
}

String NetworkRfidReader::lowerCopy(String value) {
  value.toLowerCase();
  return value;
}

String NetworkRfidReader::hexBytes(const uint8_t* data, size_t length, bool spaces) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  out.reserve((length * 3) + 1);
  for (size_t i = 0; i < length; i++) {
    if (spaces && i > 0) {
      out += ' ';
    }
    out += hex[(data[i] >> 4) & 0x0F];
    out += hex[data[i] & 0x0F];
  }
  return out;
}

String NetworkRfidReader::htmlEscape(const String& value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (c == '&') {
      out += F("&amp;");
    } else if (c == '<') {
      out += F("&lt;");
    } else if (c == '>') {
      out += F("&gt;");
    } else if (c == '"') {
      out += F("&quot;");
    } else if (c == '\'') {
      out += F("&#39;");
    } else {
      out += c;
    }
  }
  return out;
}

String NetworkRfidReader::jsonEscape(const String& value) {
  String out;
  out.reserve(value.length() + 4);
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += F("\\n");
    } else if (c == '\r') {
      out += F("\\r");
    } else if (c == '\t') {
      out += F("\\t");
    } else {
      out += c;
    }
  }
  return out;
}

const char* NetworkRfidReader::tcpModeName(NetworkRfidTcpMode mode) {
  switch (mode) {
    case NetworkRfidTcpMode::Client:
      return "client";
    case NetworkRfidTcpMode::Server:
      return "server";
    case NetworkRfidTcpMode::ElechouseTest:
      return "elechouse";
    case NetworkRfidTcpMode::Off:
    default:
      return "off";
  }
}

const char* NetworkRfidReader::wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    case WL_STOPPED:
      return "STOPPED";
    case WL_NO_SHIELD:
      return "NO_SHIELD";
    default:
      return "UNKNOWN";
  }
}

const char* NetworkRfidReader::slotName(NetworkRfidSlot slot) {
  return slot == NetworkRfidSlot::HF ? "HF" : "LF";
}

const char* NetworkRfidReader::hfBusModeName(NetworkRfidHfBusMode mode) {
  return mode == NetworkRfidHfBusMode::I2c ? "i2c" : "spi";
}

const char* NetworkRfidReader::hfRoleName(NetworkRfidHfRole role) {
  switch (role) {
    case NetworkRfidHfRole::CardEmulation:
      return "card";
    case NetworkRfidHfRole::P2p:
      return "p2p";
    case NetworkRfidHfRole::Scan:
    default:
      return "scan";
  }
}

const char* NetworkRfidReader::hfCardTypeName(NetworkRfidHfCardType type) {
  switch (type) {
    case NetworkRfidHfCardType::NfcAType2:
      return "nfc-a-t2t";
    case NetworkRfidHfCardType::NfcAType4:
    default:
      return "nfc-a-t4t";
  }
}

const char* NetworkRfidReader::hfCardPayloadTypeName(NetworkRfidHfCardPayloadType type) {
  switch (type) {
    case NetworkRfidHfCardPayloadType::Text:
      return "text";
    case NetworkRfidHfCardPayloadType::Vcard:
      return "vcard";
    case NetworkRfidHfCardPayloadType::Wifi:
      return "wifi";
    case NetworkRfidHfCardPayloadType::Url:
    default:
      return "url";
  }
}

const char* NetworkRfidReader::productInterfaceModeName(NetworkRfidProductInterfaceMode mode) {
  switch (mode) {
    case NetworkRfidProductInterfaceMode::Wiegand:
      return "wiegand";
    case NetworkRfidProductInterfaceMode::Aba:
      return "aba";
    case NetworkRfidProductInterfaceMode::Uart:
    default:
      return "uart";
  }
}

const char* NetworkRfidReader::hfTypeName(rfalNfcDevType type) {
  switch (type) {
    case RFAL_NFC_LISTEN_TYPE_NFCA:
    case RFAL_NFC_POLL_TYPE_NFCA:
      return "ISO14443A";
    case RFAL_NFC_LISTEN_TYPE_NFCB:
    case RFAL_NFC_POLL_TYPE_NFCB:
      return "ISO14443B";
    case RFAL_NFC_LISTEN_TYPE_NFCF:
    case RFAL_NFC_POLL_TYPE_NFCF:
      return "NFC-F";
    case RFAL_NFC_LISTEN_TYPE_NFCV:
    case RFAL_NFC_POLL_TYPE_NFCV:
      return "ISO15693";
    case RFAL_NFC_LISTEN_TYPE_ST25TB:
      return "ST25TB";
    case RFAL_NFC_LISTEN_TYPE_AP2P:
    case RFAL_NFC_POLL_TYPE_AP2P:
      return "AP2P";
    default:
      return "HF-UNKNOWN";
  }
}

const char* NetworkRfidReader::hfLmStateName(rfalLmState state) {
  switch (state) {
    case RFAL_LM_STATE_POWER_OFF:
      return "POWER_OFF";
    case RFAL_LM_STATE_IDLE:
      return "IDLE";
    case RFAL_LM_STATE_READY_A:
      return "READY_A";
    case RFAL_LM_STATE_READY_B:
      return "READY_B";
    case RFAL_LM_STATE_READY_F:
      return "READY_F";
    case RFAL_LM_STATE_ACTIVE_A:
      return "ACTIVE_A";
    case RFAL_LM_STATE_CARDEMU_4A:
      return "CARDEMU_4A";
    case RFAL_LM_STATE_CARDEMU_4B:
      return "CARDEMU_4B";
    case RFAL_LM_STATE_CARDEMU_3:
      return "CARDEMU_3";
    case RFAL_LM_STATE_TARGET_A:
      return "TARGET_A";
    case RFAL_LM_STATE_TARGET_F:
      return "TARGET_F";
    case RFAL_LM_STATE_SLEEP_A:
      return "SLEEP_A";
    case RFAL_LM_STATE_SLEEP_B:
      return "SLEEP_B";
    case RFAL_LM_STATE_READY_Ax:
      return "READY_Ax";
    case RFAL_LM_STATE_ACTIVE_Ax:
      return "ACTIVE_Ax";
    case RFAL_LM_STATE_SLEEP_AF:
      return "SLEEP_AF";
    case RFAL_LM_STATE_NOT_INIT:
    default:
      return "NOT_INIT";
  }
}

bool NetworkRfidReader::isHfP2pDevice(const rfalNfcDevice* device) {
  if (device == nullptr) {
    return false;
  }
  return device->rfInterface == RFAL_NFC_INTERFACE_NFCDEP ||
         device->type == RFAL_NFC_LISTEN_TYPE_AP2P ||
         device->type == RFAL_NFC_POLL_TYPE_AP2P;
}

bool NetworkRfidReader::parseHexBytes(const String& value, uint8_t* data, size_t maxLength, size_t& length) {
  length = 0;
  if (data == nullptr || maxLength == 0) {
    return false;
  }

  int high_nibble = -1;
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    int nibble = -1;
    if (c >= '0' && c <= '9') {
      nibble = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      nibble = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      nibble = c - 'A' + 10;
    } else if (c == ' ' || c == ':' || c == '-' || c == ',') {
      continue;
    } else {
      return false;
    }

    if (high_nibble < 0) {
      high_nibble = nibble;
      continue;
    }

    if (length >= maxLength) {
      return false;
    }
    data[length++] = static_cast<uint8_t>((high_nibble << 4) | nibble);
    high_nibble = -1;
  }

  return high_nibble < 0 && length > 0;
}

bool NetworkRfidReader::isValidElechouseSessionCode(const String& value) {
  if (value.length() == 0 || value.length() > 64) {
    return false;
  }
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    const bool ok = (c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    c == '_' || c == '.' || c == ':' || c == '-';
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool NetworkRfidReader::parseEventDataBytes(const String& id, uint8_t* data, size_t maxLength, size_t& length) {
  length = 0;
  if (data == nullptr || maxLength == 0) {
    return false;
  }

  String source = id;
  String lower = lowerCopy(source);
  const int raw_pos = lower.indexOf("raw=");
  if (raw_pos >= 0) {
    source = source.substring(raw_pos + 4);
  } else {
    const int bit_pos = lower.indexOf("-bit");
    if (bit_pos >= 0) {
      const int space_pos = source.indexOf(' ', bit_pos);
      if (space_pos >= 0) {
        source = source.substring(space_pos + 1);
      }
    }
  }

  lower = lowerCopy(source);
  const int ascii_pos = lower.indexOf(" ascii=");
  if (ascii_pos >= 0) {
    source = source.substring(0, ascii_pos);
  }
  source.trim();

  int high_nibble = -1;
  for (size_t i = 0; i < source.length(); i++) {
    const char c = source.charAt(i);
    int nibble = -1;
    if (c >= '0' && c <= '9') {
      nibble = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      nibble = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      nibble = c - 'A' + 10;
    } else if (c == ' ' || c == ':' || c == '-' || c == ',' || c == '\t') {
      continue;
    } else {
      return length > 0 && high_nibble < 0;
    }

    if (high_nibble < 0) {
      high_nibble = nibble;
      continue;
    }

    if (length >= maxLength) {
      return false;
    }
    data[length++] = static_cast<uint8_t>((high_nibble << 4) | nibble);
    high_nibble = -1;
  }

  return high_nibble < 0 && length > 0;
}

bool NetworkRfidReader::eventDataValue(const RfidCardEvent& event, uint64_t& value) {
  value = 0;
  uint8_t bytes[16] = {};
  size_t length = 0;
  if (!parseEventDataBytes(event.id, bytes, sizeof(bytes), length)) {
    String digits;
    if (extractDecimalField(event.id, "CN=", digits)) {
      for (size_t i = 0; i < digits.length(); i++) {
        value = (value * 10ULL) + static_cast<uint64_t>(digits.charAt(i) - '0');
      }
      return true;
    }
    return false;
  }

  const size_t start = length > 8 ? length - 8 : 0;
  for (size_t i = start; i < length; i++) {
    value = (value << 8) | bytes[i];
  }
  return true;
}

String NetworkRfidReader::eventDataDecimal(const RfidCardEvent& event, bool useCardNumber) {
  String digits;
  if (useCardNumber && extractDecimalField(event.id, "CN=", digits)) {
    return digits;
  }

  String id = event.id;
  id.trim();
  bool decimal_only = id.length() > 0;
  for (size_t i = 0; i < id.length(); i++) {
    const char c = id.charAt(i);
    if (c < '0' || c > '9') {
      decimal_only = false;
      break;
    }
  }
  if (decimal_only) {
    return id;
  }

  uint64_t value = 0;
  if (eventDataValue(event, value)) {
    return uint64ToDecimalString(value);
  }

  if (extractDecimalField(event.id, "CN=", digits)) {
    return digits;
  }
  return "";
}

bool NetworkRfidReader::extractDecimalField(const String& value, const char* field, String& digits) {
  digits = "";
  if (field == nullptr) {
    return false;
  }

  String lower_value = lowerCopy(value);
  String lower_field = field;
  lower_field.toLowerCase();
  const int pos = lower_value.indexOf(lower_field);
  if (pos < 0) {
    return false;
  }

  const size_t start = static_cast<size_t>(pos) + strlen(field);
  for (size_t i = start; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (c < '0' || c > '9') {
      break;
    }
    digits += c;
  }
  return digits.length() > 0;
}

String NetworkRfidReader::uint64ToDecimalString(uint64_t value) {
  char buffer[21] = {};
  size_t index = sizeof(buffer) - 1;
  buffer[index] = '\0';
  do {
    buffer[--index] = static_cast<char>('0' + (value % 10ULL));
    value /= 10ULL;
  } while (value > 0 && index > 0);
  return String(&buffer[index]);
}

uint8_t NetworkRfidReader::countBits64(uint64_t value) {
  uint8_t count = 0;
  while (value != 0) {
    count += static_cast<uint8_t>(value & 1ULL);
    value >>= 1;
  }
  return count;
}

String NetworkRfidReader::asciiPreview(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return "";
  }

  String out;
  out.reserve(length);
  for (size_t i = 0; i < length; i++) {
    const uint8_t c = data[i];
    if (c < 0x20 || c > 0x7E) {
      return "";
    }
    if (c == '\\' || c == '"') {
      out += '\\';
    }
    out += static_cast<char>(c);
  }
  return out;
}
