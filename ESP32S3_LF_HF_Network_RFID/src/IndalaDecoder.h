#pragma once

#include <Arduino.h>

struct IndalaId {
  uint8_t facility = 0;
  uint16_t card = 0;
  uint8_t raw[8] = {};
  uint8_t bit_size = 0;
};

struct IndalaDecodeInfo {
  uint16_t bit_count = 0;
  uint16_t frame_start = 0;
  uint16_t phase_shifts = 0;
  uint8_t half_period_us = 0;
  uint16_t pulse_hist[16] = {};
  char bit_preview[129] = {};
  uint8_t bit_preview_len = 0;
  bool inverted = false;
  bool parity_ok = false;
  bool checksum_ok = false;
};

class IndalaDecoder {
public:
  void reset();

  bool pushPulse(bool level, uint32_t duration_us, IndalaId& id, IndalaDecodeInfo& info);
  bool finish(IndalaId& id, IndalaDecodeInfo& info);
  bool tryDecode(IndalaId& id, IndalaDecodeInfo& info) const;

private:
  static constexpr size_t MaxBits = 512;
  static constexpr uint16_t BitPeriodUs = 256;
  static constexpr uint8_t DefaultHalfPeriodUs = 4;

  uint8_t bits_[MaxBits] = {};
  size_t bit_count_ = 0;
  uint32_t time_us_ = 0;
  uint32_t last_shift_us_ = 0;
  bool have_shift_ = false;
  bool phase_ = false;
  uint16_t phase_shifts_ = 0;
  uint8_t half_period_us_ = DefaultHalfPeriodUs;
  uint16_t short_pulse_hist_[16] = {};
  uint16_t short_pulses_ = 0;

  void appendBit(bool bit);
  void appendRun(bool bit, uint32_t duration_us);
  bool isPhaseShiftPulse(uint32_t duration_us);
  void observePulse(uint32_t duration_us);

  static bool findFrame(const uint8_t* bits, size_t bit_count, size_t& start, bool& inverted);
  static bool decodeFrame(const uint8_t* frame, bool inverted, IndalaId& id, IndalaDecodeInfo& info);
  static bool bitAt(const uint8_t* frame, size_t index, bool inverted);
  static void frameToBytes(const uint8_t* frame, bool inverted, uint8_t* bytes);
  static uint8_t decodeFacility(const uint8_t* frame, bool inverted);
  static uint16_t decodeCard(const uint8_t* frame, bool inverted);
  static bool parityIsValid(const uint8_t* frame, bool inverted);
  static bool checksumIsValid(const uint8_t* frame, bool inverted);
  void copyPreview(IndalaDecodeInfo& info) const;
};
