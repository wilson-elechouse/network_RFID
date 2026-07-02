#pragma once

#include <Arduino.h>

struct Em4100Id {
  uint8_t bytes[5] = {};
};

struct Em4100DecodeInfo {
  uint16_t chip_count = 0;
  uint8_t pair_offset = 0;
  uint8_t manchester_map = 0;
  uint16_t frame_start = 0;
};

class Em4100Decoder {
public:
  void reset();

  bool pushPulse(bool level, uint32_t duration_us, Em4100Id& id, Em4100DecodeInfo& info);
  bool tryDecode(Em4100Id& id, Em4100DecodeInfo& info) const;

private:
  static constexpr uint16_t HalfBitUs = 256;
  static constexpr uint16_t MinPulseUs = 96;
  static constexpr uint16_t MaxPulseUs = 1600;
  static constexpr size_t MaxChips = 768;
  static constexpr size_t MaxBits = 384;

  uint8_t chips_[MaxChips] = {};
  size_t chip_count_ = 0;

  void appendChip(uint8_t chip);
  static bool frameIsValid(const int8_t* frame, Em4100Id& id);
};
