#include "Em4100Decoder.h"

#include <string.h>

void Em4100Decoder::reset() {
  chip_count_ = 0;
}

void Em4100Decoder::appendChip(uint8_t chip) {
  chip = chip ? 1 : 0;
  if (chip_count_ < MaxChips) {
    chips_[chip_count_++] = chip;
    return;
  }

  memmove(chips_, chips_ + 1, MaxChips - 1);
  chips_[MaxChips - 1] = chip;
}

bool Em4100Decoder::pushPulse(
  bool level,
  uint32_t duration_us,
  Em4100Id& id,
  Em4100DecodeInfo& info) {
  if (duration_us < MinPulseUs) {
    return false;
  }

  if (duration_us > MaxPulseUs) {
    reset();
    return false;
  }

  uint32_t chip_repeat = (duration_us + (HalfBitUs / 2)) / HalfBitUs;
  if (chip_repeat == 0) {
    chip_repeat = 1;
  } else if (chip_repeat > 6) {
    reset();
    return false;
  }

  for (uint32_t i = 0; i < chip_repeat; i++) {
    appendChip(level ? 1 : 0);
  }

  if (chip_count_ < 128) {
    return false;
  }

  return tryDecode(id, info);
}

bool Em4100Decoder::tryDecode(Em4100Id& id, Em4100DecodeInfo& info) const {
  int8_t bits[MaxBits] = {};

  for (uint8_t pair_offset = 0; pair_offset < 2; pair_offset++) {
    for (uint8_t map = 0; map < 2; map++) {
      size_t bit_count = 0;
      for (size_t c = pair_offset; c + 1 < chip_count_ && bit_count < MaxBits; c += 2) {
        const uint8_t a = chips_[c];
        const uint8_t b = chips_[c + 1];
        if (a == b) {
          bits[bit_count++] = -1;
        } else if ((a == 0) && (b == 1)) {
          bits[bit_count++] = map ? 0 : 1;
        } else {
          bits[bit_count++] = map ? 1 : 0;
        }
      }

      for (size_t start = 0; start + 64 <= bit_count; start++) {
        if (frameIsValid(&bits[start], id)) {
          info.chip_count = chip_count_;
          info.pair_offset = pair_offset;
          info.manchester_map = map;
          info.frame_start = start;
          return true;
        }
      }
    }
  }

  return false;
}

bool Em4100Decoder::frameIsValid(const int8_t* frame, Em4100Id& id) {
  for (size_t i = 0; i < 64; i++) {
    if (frame[i] < 0) {
      return false;
    }
  }

  for (size_t i = 0; i < 9; i++) {
    if (frame[i] != 1) {
      return false;
    }
  }

  if (frame[63] != 0) {
    return false;
  }

  uint8_t data[10][4] = {};
  size_t pos = 9;

  for (size_t row = 0; row < 10; row++) {
    uint8_t parity = 0;
    for (size_t col = 0; col < 4; col++) {
      const uint8_t bit = frame[pos++] ? 1 : 0;
      data[row][col] = bit;
      parity ^= bit;
    }
    parity ^= frame[pos++] ? 1 : 0;
    if (parity != 0) {
      return false;
    }
  }

  for (size_t col = 0; col < 4; col++) {
    uint8_t parity = 0;
    for (size_t row = 0; row < 10; row++) {
      parity ^= data[row][col];
    }
    parity ^= frame[59 + col] ? 1 : 0;
    if (parity != 0) {
      return false;
    }
  }

  memset(id.bytes, 0, sizeof(id.bytes));
  size_t bit_index = 0;
  for (size_t row = 0; row < 10; row++) {
    for (size_t col = 0; col < 4; col++) {
      id.bytes[bit_index / 8] <<= 1;
      id.bytes[bit_index / 8] |= data[row][col];
      bit_index++;
    }
  }

  return true;
}
