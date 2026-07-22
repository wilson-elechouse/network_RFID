#include <Arduino.h>

#include <NetworkRfidReader.h>

NetworkRfidReader reader;

void setup() {
  // Keep BOOT/GPIO0 biased high after the application starts. This cannot
  // override a held BOOT button during reset, but helps if the pin is floating.
  pinMode(0, INPUT_PULLUP);

  Serial.begin(115200);
  const unsigned long start = millis();
  while (!Serial && (millis() - start) < 2000UL) {
    delay(10);
  }

  NetworkRfidConfig config;

  // Defaults match the current ESP32-S3 LF/HF wiring:
  // LF: OUT=GPIO1, DATA=GPIO2, CARRIER=GPIO3, PULL=GPIO4, ADC=GPIO13
  // HF I2C: SCL=GPIO5, SDA=GPIO7, IRQ=GPIO9, +5V, GND
  // Feedback: WS2816C DIN=GPIO11, buzzer drive=GPIO12
  // Button: GPIO10 active-low, 5s WiFi config portal, 10s factory reset
  // Product interface on two fixed pins:
  // UART RX/TX = GPIO44/GPIO43, Wiegand D0/D1 = GPIO44/GPIO43,
  // ABA Clock/Data = GPIO44/GPIO43.
  // HF I2C/SPI mode is selected by the HF module hardware. GPIO8 is only the default CS pin for SPI.
  //
  // Runtime settings can be changed over Serial and saved in NVS:
  //   wifi <ssid> <password>
  //   tcp client <host> <port>
  //   tcp server <port>
  //   portal status
  //   lf init
  //   hf bus i2c
  //   hf init
  //   interface mode uart|wiegand|aba
  //   wiegand bits 34
  //   save

  Serial.println("ESP32S3 LF/HF RFID boot");
  config.pins.hfBusMode = NetworkRfidHfBusMode::I2c;
  config.hfI2cHz = 400000UL;
  config.autoStartLf = true;
  config.autoInitHf = true;
  reader.begin(config, Serial, true);
}

void loop() {
  reader.loop();
  delay(0);
}
