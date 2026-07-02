#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <Wire.h>

#include <rfal_nfc.h>
#include <rfal_rfst25r3916.h>
#include <st_errno.h>

#include "Em4100Decoder.h"
#include "HidProxDecoder.h"
#include "IndalaDecoder.h"

#ifndef ESP32S3_LFHF_DEFAULT_SPI_BUS
#if defined(FSPI)
#define ESP32S3_LFHF_DEFAULT_SPI_BUS FSPI
#else
#define ESP32S3_LFHF_DEFAULT_SPI_BUS HSPI
#endif
#endif

#include "NetworkRfidBoardProfile.h"

enum class NetworkRfidTcpMode : uint8_t {
  Off,
  Client,
  Server,
  ElechouseTest,
};

enum class NetworkRfidOutputFormat : uint8_t {
  Json,
  CsvLine,
};

enum class NetworkRfidSlot : uint8_t {
  LF,
  HF,
};

enum class NetworkRfidHfBusMode : uint8_t {
  Spi,
  I2c,
};

enum class NetworkRfidHfRole : uint8_t {
  Scan,
  CardEmulation,
  P2p,
};

enum class NetworkRfidHfCardPayloadType : uint8_t {
  Url,
  Text,
  Vcard,
  Wifi,
};

enum class NetworkRfidProductInterfaceMode : uint8_t {
  Uart,
  Wiegand,
  Aba,
};

struct NetworkRfidPins {
  int lfOut = NETWORK_RFID_PIN_LF_OUT;
  int lfData = NETWORK_RFID_PIN_LF_DATA;
  int lfCarrierDetect = NETWORK_RFID_PIN_LF_CARRIER;
  int lfPull = NETWORK_RFID_PIN_LF_PULL;
  int lfAdc = NETWORK_RFID_PIN_LF_ADC;
  uint8_t lfLedcChannel = NETWORK_RFID_LF_LEDC_CHANNEL;

  NetworkRfidHfBusMode hfBusMode = NetworkRfidHfBusMode::I2c;
  uint8_t hfSpiBus = NETWORK_RFID_HF_SPI_BUS;
  int hfSck = NETWORK_RFID_PIN_HF_SCL;
  int hfMosi = NETWORK_RFID_PIN_HF_MOSI;
  int hfMiso = NETWORK_RFID_PIN_HF_SDA;
  int hfCs = NETWORK_RFID_PIN_HF_CS;
  int hfIrq = NETWORK_RFID_PIN_HF_IRQ;
  int hfReset = NETWORK_RFID_PIN_HF_RESET;
  bool hfResetActiveLow = NETWORK_RFID_HF_RESET_ACTIVE_LOW;
  int hfMode = NETWORK_RFID_PIN_HF_MODE;
  uint8_t hfModeLevel = NETWORK_RFID_HF_MODE_LEVEL;

  int feedbackLed = NETWORK_RFID_PIN_FEEDBACK_LED;
  int feedbackBuzzer = NETWORK_RFID_PIN_FEEDBACK_BUZZER;
  uint8_t feedbackBuzzerLedcChannel = NETWORK_RFID_FEEDBACK_BUZZER_LEDC_CHANNEL;
  int configButton = NETWORK_RFID_PIN_CONFIG_BUTTON;
  bool configButtonActiveLow = NETWORK_RFID_CONFIG_BUTTON_ACTIVE_LOW;
  int hardwareUartRx = NETWORK_RFID_PIN_UART_RX;
  int hardwareUartTx = NETWORK_RFID_PIN_UART_TX;
};

struct NetworkRfidConfig {
  NetworkRfidPins pins;
  String wifiSsid;
  String wifiPassword;

  NetworkRfidTcpMode tcpMode = NetworkRfidTcpMode::Off;
  String tcpHost;
  uint16_t tcpPort = 9000;
  uint16_t tcpListenPort = 9000;
  NetworkRfidOutputFormat outputFormat = NetworkRfidOutputFormat::Json;
  bool tcpEchoEvents = true;
  bool tcpCommands = true;
  String elechouseSessionCode;

  uint32_t lfCarrierHz = 125000;
  uint32_t lfWindowMs = 350;
  uint32_t hfWindowMs = 350;
  uint32_t duplicateSuppressMs = 1000;
  uint32_t tcpReconnectMs = 3000;
  uint32_t hfSpiHz = 5000000UL;
  uint32_t hfI2cHz = 100000UL;
  uint16_t hfDiscoveryDurationMs = RFAL_ESP32_DEFAULT_DISCOVERY_DURATION_MS;
  uint16_t hfTechs = RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B |
                     RFAL_NFC_POLL_TECH_F | RFAL_NFC_POLL_TECH_V;
  NetworkRfidHfRole hfRole = NetworkRfidHfRole::Scan;
  String hfCardUid = "02 00 00 01";
  NetworkRfidHfCardPayloadType hfCardPayloadType = NetworkRfidHfCardPayloadType::Url;
  String hfCardPayload = "https://www.elechouse.com/";
  String hfCardWifiSsid;
  String hfCardWifiPassword;
  String hfP2pMessage = "ELECHOUSE RFID P2P";
  bool configPortalEnabled = true;
  String configPortalSsid;
  String configPortalPassword;
  uint16_t configPortalPort = 80;
  bool echoEventsToSerial = true;
  bool autoStartLf = true;
  bool autoInitHf = true;

  bool feedbackEnabled = true;
  uint32_t feedbackBuzzerHz = 2700;
  uint16_t feedbackBuzzerMs = 80;
  uint16_t feedbackSuccessMs = 2000;
  uint16_t feedbackIdleRed = 0x4000;
  uint16_t feedbackIdleGreen = 0x0000;
  uint16_t feedbackIdleBlue = 0x0000;
  uint16_t feedbackSuccessRed = 0x0000;
  uint16_t feedbackSuccessGreen = 0x4000;
  uint16_t feedbackSuccessBlue = 0x0000;

  bool configButtonEnabled = true;
  uint16_t buttonWifiConfigMs = 5000;
  uint16_t buttonFactoryResetMs = 10000;

  bool hardwareUartEnabled = true;
  uint32_t hardwareUartBaud = 115200;
  bool hardwareUartEchoEvents = true;
  bool hardwareUartCommands = true;

  NetworkRfidProductInterfaceMode productInterfaceMode = NetworkRfidProductInterfaceMode::Uart;
  uint8_t wiegandBits = 34;
  uint16_t productPulseUs = 80;
  uint16_t productPulseGapUs = 1800;
  uint8_t abaDigits = 0;
  bool abaUseCardNumber = false;
};

struct RfidCardEvent {
  const char* band = "";
  const char* type = "";
  String id;
  uint32_t ms = 0;
};

using NetworkRfidCardCallback = void (*)(const RfidCardEvent& event, void* context);

class NetworkRfidReader {
public:
  NetworkRfidReader();
  ~NetworkRfidReader();

  bool begin(const NetworkRfidConfig& config = NetworkRfidConfig(),
             Stream& console = Serial,
             bool loadSavedConfig = true);
  void loop();
  void end();

  const NetworkRfidConfig& config() const;
  bool saveConfig();
  bool loadConfig();
  void clearSavedConfig();
  void setCardCallback(NetworkRfidCardCallback callback, void* context = nullptr);

  void printHelp();
  void printStatus();
  void printPins();

private:
  struct PulseSample {
    uint16_t durationUs;
    uint8_t level;
  };

  static constexpr size_t PulseQueueSize = 2048;
  static_assert((PulseQueueSize & (PulseQueueSize - 1)) == 0, "PulseQueueSize must be power of two");
  static constexpr size_t DuplicateCacheSize = 8;
  static constexpr size_t HfT4tFileMaxLen = 768;

  static NetworkRfidReader* activeInstance_;
  static void IRAM_ATTR onLfDataEdgeStatic();
  static void onHfStateChangeStatic(rfalNfcState state);
  static void onWiFiEventStatic(arduino_event_id_t event, arduino_event_info_t info);

  void IRAM_ATTR onLfDataEdge();
  bool popPulse(PulseSample& pulse);
  void resetPulseQueue();
  void setLfCarrier(bool enabled);
  void setLfCapture(bool enabled);
  void setActiveSlot(NetworkRfidSlot slot);
  bool isLfEnabled() const;
  bool isHfEnabled() const;
  void releaseHf();
  void processLfPulses();
  void processLfHidPulses(uint16_t max_pulses);

  bool setupHf();
  bool probeHfIdentity();
  void serviceHf();
  void restartHfRole();
  void startHfDiscovery();
  void stopHfDiscovery();
  void onHfStateChange(rfalNfcState state);
  bool startHfCardEmulation();
  void stopHfCardEmulation();
  void serviceHfCardEmulation();
  void resetHfCardProtocol();
  void serviceHfCardIsoDep();
  bool handleHfCardListenFrame(const uint8_t* data, size_t length);
  bool handleHfCardIsoDepFrame(const uint8_t* frame, size_t length);
  bool sendHfCardIsoDepFrame(const uint8_t* frame, size_t length, bool expectRx);
  size_t buildHfNdefMessage(uint8_t* out, size_t maxLength) const;
  size_t buildHfNdefFile(uint8_t* out, size_t maxLength) const;
  size_t buildHfT4tApduResponse(const uint8_t* apdu, size_t apduLen, uint8_t* out, size_t maxLength);
  bool beginHfDataExchange(rfalNfcDevice* device);
  void serviceHfDataExchange();
  uint16_t activeHfTechs() const;

  void handleSerial();
  void handleSerialStream(Stream& stream, String& line);
  void handleCommand(String line);
  void handlePinCommand(String rest);
  void handleTcpCommand(String rest);
  void handleElechouseCommand(String rest);
  void handleWifiCommand(String rest);
  void handleLfCommand(String rest);
  void handleLfHidCommand(String rest);
  void handleLfScanCommand(String rest);
  void handleHfCommand(String rest);
  void handlePortalCommand(String rest);
  void handleLfIndalaCommand(String rest);
  void handleUartCommand(String rest);
  void handleInterfaceCommand(String rest);
  void handleFeedbackCommand(String rest);
  void handleButtonCommand(String rest);

  void startWifi();
  void serviceNetwork();
  void serviceWifiStation();
  void restartTcpSocket();
  bool ensureTcpClient();
  void sendNetworkLine(const String& line);
  void serviceTcpCommands();
  void serviceElechouseBroker();
  bool handleElechouseBrokerLine(const String& line);
  void sendElechouseHello();
  void sendElechouseCard(const RfidCardEvent& event);
  String formatElechouseCardEvent(const RfidCardEvent& event) const;
  String elechouseDeviceId() const;

  void setupFeedback();
  void serviceFeedback();
  void triggerFeedbackSuccess();
  void showFeedbackIdle();
  void setFeedbackLed(uint16_t red, uint16_t green, uint16_t blue);
  void startFeedbackBuzzer();
  void stopFeedbackBuzzer();
  void beepFeedback(uint8_t count);
  void setupButton();
  void serviceButton();
  bool readConfigButtonPressed() const;
  void enterDefaultWifiConfig();
  void restoreDefaultConfig();
  void setupHardwareUart();
  void releaseHardwareUart();
  void restartHardwareUart();
  void setupProductInterface();
  void releaseProductInterface();
  void restartProductInterface();
  void setupPulseInterface();
  void releasePulseInterface();
  void sendProductInterfaceEvent(const RfidCardEvent& event);
  bool sendWiegandEvent(const RfidCardEvent& event);
  bool sendAbaEvent(const RfidCardEvent& event);
  void sendWiegandBit(bool bit);
  void sendAbaNibble(uint8_t value, uint8_t& lrc, bool includeInLrc);
  void sendAbaBit(bool bit);

  void startConfigPortal();
  void stopConfigPortal();
  void serviceConfigPortal();
  void handlePortalRoot();
  void handlePortalSave();
  void handlePortalReboot();
  void handlePortalNotFound();
  void sendPortalPage(const String& message);
  void restartStation();
  String defaultPortalSsid() const;

  void emitCard(const char* band, const char* type, const String& id);
  bool shouldSuppressDuplicate(const RfidCardEvent& event);
  String formatEvent(const RfidCardEvent& event) const;
  bool captureIndalaSamples(uint16_t requestedSamples, IndalaId& id, IndalaDecodeInfo& info, size_t& samplesRead);

  static bool parseBoolForm(WebServer* server, const char* name);
  static uint16_t parsePortForm(WebServer* server, const char* name, uint16_t fallback);
  static uint32_t parseUIntForm(WebServer* server, const char* name, uint32_t fallback);
  static String nextToken(String& rest);
  static String lowerCopy(String value);
  static String hexBytes(const uint8_t* data, size_t length, bool spaces = true);
  static String htmlEscape(const String& value);
  static String jsonEscape(const String& value);
  static const char* tcpModeName(NetworkRfidTcpMode mode);
  static const char* wifiStatusName(wl_status_t status);
  static const char* slotName(NetworkRfidSlot slot);
  static const char* hfBusModeName(NetworkRfidHfBusMode mode);
  static const char* hfRoleName(NetworkRfidHfRole role);
  static const char* hfCardPayloadTypeName(NetworkRfidHfCardPayloadType type);
  static const char* productInterfaceModeName(NetworkRfidProductInterfaceMode mode);
  static const char* hfTypeName(rfalNfcDevType type);
  static const char* hfLmStateName(rfalLmState state);
  static bool isHfP2pDevice(const rfalNfcDevice* device);
  static bool parseHexBytes(const String& value, uint8_t* data, size_t maxLength, size_t& length);
  static bool isValidElechouseSessionCode(const String& value);
  static bool parseEventDataBytes(const String& id, uint8_t* data, size_t maxLength, size_t& length);
  static bool eventDataValue(const RfidCardEvent& event, uint64_t& value);
  static String eventDataDecimal(const RfidCardEvent& event, bool useCardNumber);
  static bool extractDecimalField(const String& value, const char* field, String& digits);
  static String uint64ToDecimalString(uint64_t value);
  static uint8_t countBits64(uint64_t value);
  static String asciiPreview(const uint8_t* data, size_t length);

  NetworkRfidConfig config_;
  Stream* console_ = &Serial;
  Stream* primaryConsole_ = &Serial;
  HardwareSerial hardwareUart_{1};
  bool hardwareUartReady_ = false;
  bool productInterfacePulseReady_ = false;

  SPIClass* spi_ = nullptr;
  TwoWire* i2c_ = nullptr;
  RfalRfST25R3916Class* hfReader_ = nullptr;
  RfalNfcClass* nfc_ = nullptr;
  rfalNfcDiscoverParam hfDiscover_ = {};
  bool hfReady_ = false;
  bool hfDiscoveryActive_ = false;
  uint32_t lastHfDiscoverAttemptMs_ = 0;
  rfalLmConfPA hfCardEmuA_ = {};
  bool hfCardEmulationActive_ = false;
  uint32_t lastHfCardEmuAttemptMs_ = 0;
  uint8_t hfListenRxBuf_[RFAL_NFC_RF_BUF_LEN] = {};
  uint16_t hfListenRxBits_ = 0;
  rfalLmState hfLastLmState_ = RFAL_LM_STATE_NOT_INIT;
  bool hfCardIsoDepActive_ = false;
  uint32_t hfCardIsoDepStartMs_ = 0;
  uint8_t hfCardExpectedBlock_ = 0;
  uint8_t hfCardSelectedFile_ = 0;
  uint8_t hfCardTxBuf_[RFAL_NFC_RF_BUF_LEN] = {};
  uint16_t hfCardTxLen_ = 0;
  uint8_t hfCardLastTxBuf_[RFAL_NFC_RF_BUF_LEN] = {};
  uint16_t hfCardLastTxLen_ = 0;
  uint8_t hfCardNdefFile_[HfT4tFileMaxLen] = {};
  uint8_t hfP2pTxBuf_[RFAL_NFC_RF_BUF_LEN] = {};
  bool hfDataExchangeActive_ = false;
  uint8_t* hfExchangeRxData_ = nullptr;
  uint16_t* hfExchangeRxLen_ = nullptr;
  uint32_t hfDataExchangeStartMs_ = 0;

  Preferences prefs_;
  bool prefsOpen_ = false;
  WiFiClient tcpClient_;
  WiFiClient serverClient_;
  WiFiServer* server_ = nullptr;
  uint16_t activeServerPort_ = 0;
  uint32_t lastWifiBeginMs_ = 0;
  uint32_t lastTcpConnectMs_ = 0;
  bool wifiConnectInProgress_ = false;
  uint8_t lastWifiDisconnectReason_ = 0;
  uint32_t lastWifiDisconnectMs_ = 0;
  wifi_event_id_t wifiDisconnectEventId_ = 0;
  bool wifiDisconnectEventRegistered_ = false;
  bool elechouseHelloSent_ = false;
  bool elechouseBrokerOk_ = false;
  uint32_t elechouseLastPingMs_ = 0;
  DNSServer* portalDns_ = nullptr;
  WebServer* portalServer_ = nullptr;
  bool portalRunning_ = false;
  uint32_t portalStartMs_ = 0;

  volatile PulseSample pulseQueue_[PulseQueueSize] = {};
  volatile uint16_t pulseHead_ = 0;
  volatile uint16_t pulseTail_ = 0;
  volatile uint32_t lastEdgeUs_ = 0;
  volatile uint8_t lastDataLevel_ = 0;
  volatile uint32_t droppedPulses_ = 0;
  volatile uint32_t capturedPulses_ = 0;
  bool lfCaptureEnabled_ = false;
  bool lfCarrierEnabled_ = false;
  uint16_t lfRawDumpRemaining_ = 0;

  bool feedbackLedReady_ = false;
  bool feedbackBuzzerActive_ = false;
  bool feedbackShowingSuccess_ = false;
  uint32_t feedbackBuzzerStopMs_ = 0;
  uint32_t feedbackSuccessUntilMs_ = 0;

  bool buttonLastRawPressed_ = false;
  bool buttonStablePressed_ = false;
  bool buttonWifiConfigFired_ = false;
  bool buttonFactoryResetFired_ = false;
  uint32_t buttonLastRawChangeMs_ = 0;
  uint32_t buttonPressStartMs_ = 0;

  Em4100Decoder em4100_;
  HidProxDecoder hidProx_;
  IndalaDecoder indala_;
  Em4100Id lastEm4100_ = {};
  HidH10301Id lastH10301_ = {};
  HidGenericId lastHidGeneric_ = {};
  bool haveLastEm4100_ = false;
  bool haveLastH10301_ = false;
  bool haveLastHidGeneric_ = false;
  uint32_t lastH10301EmitMs_ = 0;

  NetworkRfidSlot activeSlot_ = NetworkRfidSlot::LF;
  uint32_t slotStartMs_ = 0;
  String serialLine_;
  String hardwareUartLine_;
  String tcpClientLine_;
  String serverClientLine_;

  String lastEventKey_;
  uint32_t lastEventMs_ = 0;
  String duplicateKeys_[DuplicateCacheSize];
  uint32_t duplicateMs_[DuplicateCacheSize] = {};
  uint8_t duplicateNext_ = 0;
  NetworkRfidCardCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
};
