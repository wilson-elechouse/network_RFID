#pragma once

// Board profile for the current Network RFID Reader mainboard with
// ESP32-S3 and ST25R3916B on I2C.

static constexpr const char* NETWORK_RFID_WIFI_HOSTNAME = "ELECHOUSE_RFID";

static constexpr int NETWORK_RFID_PIN_LF_OUT = 1;
static constexpr int NETWORK_RFID_PIN_LF_DATA = 2;
static constexpr int NETWORK_RFID_PIN_LF_CARRIER = 3;
static constexpr int NETWORK_RFID_PIN_LF_PULL = 4;
static constexpr int NETWORK_RFID_PIN_LF_ADC = 13;
static constexpr uint8_t NETWORK_RFID_LF_LEDC_CHANNEL = 0;

static constexpr uint8_t NETWORK_RFID_HF_SPI_BUS = ESP32S3_LFHF_DEFAULT_SPI_BUS;
static constexpr int NETWORK_RFID_PIN_HF_SCL = 5;
static constexpr int NETWORK_RFID_PIN_HF_MOSI = 6;
static constexpr int NETWORK_RFID_PIN_HF_SDA = 7;
static constexpr int NETWORK_RFID_PIN_HF_CS = 8;
static constexpr int NETWORK_RFID_PIN_HF_IRQ = 9;
static constexpr int NETWORK_RFID_PIN_HF_RESET = -1;
static constexpr bool NETWORK_RFID_HF_RESET_ACTIVE_LOW = true;
static constexpr int NETWORK_RFID_PIN_HF_MODE = -1;
static constexpr uint8_t NETWORK_RFID_HF_MODE_LEVEL = LOW;

static constexpr int NETWORK_RFID_PIN_FEEDBACK_LED = 11;
static constexpr int NETWORK_RFID_PIN_FEEDBACK_BUZZER = 12;
static constexpr uint8_t NETWORK_RFID_FEEDBACK_BUZZER_LEDC_CHANNEL = 1;

static constexpr int NETWORK_RFID_PIN_CONFIG_BUTTON = 10;
static constexpr bool NETWORK_RFID_CONFIG_BUTTON_ACTIVE_LOW = true;

static constexpr int NETWORK_RFID_PIN_UART_RX = 44;
static constexpr int NETWORK_RFID_PIN_UART_TX = 43;
