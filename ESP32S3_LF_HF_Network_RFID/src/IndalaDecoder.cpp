#include "IndalaDecoder.h"

#include <string.h>

void IndalaDecoder::reset() {
  memset(bits_, 0, sizeof(bits_));
  memset(short_pulse_hist_, 0, sizeof(short_pulse_hist_));
  bit_count_ = 0;
  time_us_ = 0;
  last_shift_us_ = 0;
  have_shift_ = false;
  phase_ = false;
  phase_shifts_ = 0;
  half_period_us_ = DefaultHalfPeriodUs;
  short_pulses_ = 0;
}

bool IndalaDecoder::pushPulse(
  bool level,
  uint32_t duration_us,
  IndalaId& id,
  IndalaDecodeInfo& info) {
  (void) level;

  if (duration_us == 0) {
    return false;
  }

  if (duration_us > 2000) {
    time_us_ = 0;
    last_shift_us_ = 0;
    have_shift_ = false;
    phase_ = false;
    return false;
  }

  observePulse(duration_us);

  const uint32_t end_time_us = time_us_ + duration_us;
  if (isPhaseShiftPulse(duration_us)) {
    const uint32_t shift_time_us = time_us_ + (duration_us / 2);
    if (have_shift_ && (shift_time_us - last_shift_us_) < (BitPeriodUs / 2)) {
      time_us_ = end_time_us;
      return false;
    }
    if (have_shift_) {
      appendRun(phase_, shift_time_us - last_shift_us_);
    }
    phase_ = !phase_;
    last_shift_us_ = shift_time_us;
    have_shift_ = true;
    phase_shifts_++;
  }

  time_us_ = end_time_us;

  if (bit_count_ < 96) {
    return false;
  }

  if (tryDecode(id, info)) {
    reset();
    return true;
  }

  return false;
}

bool IndalaDecoder::finish(IndalaId& id, IndalaDecodeInfo& info) {
  if (have_shift_ && time_us_ > last_shift_us_) {
    appendRun(phase_, time_us_ - last_shift_us_);
    last_shift_us_ = time_us_;
  }

  if (tryDecode(id, info)) {
    return true;
  }

  info.bit_count = static_cast<uint16_t>(bit_count_);
  info.phase_shifts = phase_shifts_;
  info.half_period_us = half_period_us_;
  memcpy(info.pulse_hist, short_pulse_hist_, sizeof(info.pulse_hist));
  copyPreview(info);
  return false;
}

bool IndalaDecoder::tryDecode(IndalaId& id, IndalaDecodeInfo& info) const {
  size_t start = 0;
  bool inverted = false;
  if (!findFrame(bits_, bit_count_, start, inverted)) {
    return false;
  }

  IndalaDecodeInfo decoded = {};
  if (!decodeFrame(&bits_[start], inverted, id, decoded)) {
    return false;
  }

  decoded.bit_count = static_cast<uint16_t>(bit_count_);
  decoded.frame_start = static_cast<uint16_t>(start);
  decoded.phase_shifts = phase_shifts_;
  decoded.half_period_us = half_period_us_;
  memcpy(decoded.pulse_hist, short_pulse_hist_, sizeof(decoded.pulse_hist));
  copyPreview(decoded);
  info = decoded;
  return true;
}

void IndalaDecoder::copyPreview(IndalaDecodeInfo& info) const {
  const size_t count = bit_count_ < 128 ? bit_count_ : 128;
  for (size_t i = 0; i < count; i++) {
    info.bit_preview[i] = bits_[i] ? '1' : '0';
  }
  info.bit_preview[count] = '\0';
  info.bit_preview_len = static_cast<uint8_t>(count);
}

void IndalaDecoder::appendBit(bool bit) {
  if (bit_count_ < MaxBits) {
    bits_[bit_count_++] = bit ? 1 : 0;
    return;
  }

  memmove(bits_, bits_ + 1, MaxBits - 1);
  bits_[MaxBits - 1] = bit ? 1 : 0;
}

void IndalaDecoder::appendRun(bool bit, uint32_t duration_us) {
  if (duration_us < (BitPeriodUs / 2)) {
    return;
  }

  uint32_t repeat = (duration_us + (BitPeriodUs / 2)) / BitPeriodUs;
  if (repeat == 0) {
    repeat = 1;
  } else if (repeat > 64) {
    repeat = 64;
  }

  for (uint32_t i = 0; i < repeat; i++) {
    appendBit(bit);
  }
}

bool IndalaDecoder::isPhaseShiftPulse(uint32_t duration_us) {
  const uint32_t half = DefaultHalfPeriodUs;
  const uint32_t min_shift = (half * 3U) - 1U;
  const uint32_t max_shift = half * 8U;
  return duration_us >= min_shift && duration_us <= max_shift;
}

void IndalaDecoder::observePulse(uint32_t duration_us) {
  constexpr size_t HistSize = sizeof(short_pulse_hist_) / sizeof(short_pulse_hist_[0]);
  if (duration_us == 0 || duration_us >= HistSize) {
    return;
  }

  short_pulse_hist_[duration_us]++;
  short_pulses_++;
  if (short_pulses_ < 48) {
    return;
  }

  uint16_t best_count = 0;
  uint8_t best_us = half_period_us_;
  for (uint8_t us = 4; us < HistSize; us++) {
    if (short_pulse_hist_[us] > best_count) {
      best_count = short_pulse_hist_[us];
      best_us = us;
    }
  }

  if (best_us >= 2 && best_us <= 8) {
    half_period_us_ = best_us;
  }
}

bool IndalaDecoder::findFrame(const uint8_t* bits, size_t bit_count, size_t& start, bool& inverted) {
  static const uint8_t preamble64[] = {
    1, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1
  };

  if (bit_count < 64) {
    return false;
  }

  for (size_t i = 0; i + 64 <= bit_count; i++) {
    for (uint8_t inv = 0; inv < 2; inv++) {
      bool match = true;
      for (size_t p = 0; p < sizeof(preamble64); p++) {
        const bool value = bits[i + p] ^ inv;
        if (value != (preamble64[p] != 0)) {
          match = false;
          break;
        }
      }
      if (match) {
        start = i;
        inverted = inv != 0;
        return true;
      }
    }
  }

  return false;
}

bool IndalaDecoder::decodeFrame(
  const uint8_t* frame,
  bool inverted,
  IndalaId& id,
  IndalaDecodeInfo& info) {
  memset(&id, 0, sizeof(id));
  id.bit_size = 64;
  id.facility = decodeFacility(frame, inverted);
  id.card = decodeCard(frame, inverted);
  frameToBytes(frame, inverted, id.raw);

  info.inverted = inverted;
  info.parity_ok = parityIsValid(frame, inverted);
  info.checksum_ok = checksumIsValid(frame, inverted);
  return info.parity_ok && info.checksum_ok;
}

bool IndalaDecoder::bitAt(const uint8_t* frame, size_t index, bool inverted) {
  return (frame[index] ^ (inverted ? 1 : 0)) != 0;
}

void IndalaDecoder::frameToBytes(const uint8_t* frame, bool inverted, uint8_t* bytes) {
  memset(bytes, 0, 8);
  for (size_t i = 0; i < 64; i++) {
    bytes[i / 8] <<= 1;
    bytes[i / 8] |= bitAt(frame, i, inverted) ? 1 : 0;
  }
}

uint8_t IndalaDecoder::decodeFacility(const uint8_t* frame, bool inverted) {
  uint8_t fc = 0;
  fc |= bitAt(frame, 57, inverted) ? (1U << 7) : 0;
  fc |= bitAt(frame, 49, inverted) ? (1U << 6) : 0;
  fc |= bitAt(frame, 44, inverted) ? (1U << 5) : 0;
  fc |= bitAt(frame, 47, inverted) ? (1U << 4) : 0;
  fc |= bitAt(frame, 48, inverted) ? (1U << 3) : 0;
  fc |= bitAt(frame, 53, inverted) ? (1U << 2) : 0;
  fc |= bitAt(frame, 39, inverted) ? (1U << 1) : 0;
  fc |= bitAt(frame, 58, inverted) ? (1U << 0) : 0;
  return fc;
}

uint16_t IndalaDecoder::decodeCard(const uint8_t* frame, bool inverted) {
  uint16_t cn = 0;
  cn |= bitAt(frame, 42, inverted) ? (1U << 15) : 0;
  cn |= bitAt(frame, 45, inverted) ? (1U << 14) : 0;
  cn |= bitAt(frame, 43, inverted) ? (1U << 13) : 0;
  cn |= bitAt(frame, 40, inverted) ? (1U << 12) : 0;
  cn |= bitAt(frame, 52, inverted) ? (1U << 11) : 0;
  cn |= bitAt(frame, 36, inverted) ? (1U << 10) : 0;
  cn |= bitAt(frame, 35, inverted) ? (1U << 9) : 0;
  cn |= bitAt(frame, 51, inverted) ? (1U << 8) : 0;
  cn |= bitAt(frame, 46, inverted) ? (1U << 7) : 0;
  cn |= bitAt(frame, 33, inverted) ? (1U << 6) : 0;
  cn |= bitAt(frame, 37, inverted) ? (1U << 5) : 0;
  cn |= bitAt(frame, 54, inverted) ? (1U << 4) : 0;
  cn |= bitAt(frame, 56, inverted) ? (1U << 3) : 0;
  cn |= bitAt(frame, 59, inverted) ? (1U << 2) : 0;
  cn |= bitAt(frame, 50, inverted) ? (1U << 1) : 0;
  cn |= bitAt(frame, 41, inverted) ? (1U << 0) : 0;
  return cn;
}

bool IndalaDecoder::parityIsValid(const uint8_t* frame, bool inverted) {
  uint8_t p1 = 1;
  uint8_t p2 = 1;

  for (size_t i = 33; i < 64; i++) {
    if (i == 34 || i == 38) {
      continue;
    }
    if (i & 1U) {
      p1 ^= bitAt(frame, i, inverted) ? 1 : 0;
    } else {
      p2 ^= bitAt(frame, i, inverted) ? 1 : 0;
    }
  }

  return bitAt(frame, 34, inverted) == (p1 != 0) &&
         bitAt(frame, 38, inverted) == (p2 != 0);
}

bool IndalaDecoder::checksumIsValid(const uint8_t* frame, bool inverted) {
  const uint16_t cn = decodeCard(frame, inverted);
  uint8_t sum = 0;
  sum += (cn >> 14) & 1U;
  sum += (cn >> 12) & 1U;
  sum += (cn >> 9) & 1U;
  sum += (cn >> 8) & 1U;
  sum += (cn >> 6) & 1U;
  sum += (cn >> 5) & 1U;
  sum += (cn >> 2) & 1U;
  sum += cn & 1U;

  const bool expected_b62 = (sum & 1U) == 0;
  const bool expected_b63 = !expected_b62;
  return bitAt(frame, 62, inverted) == expected_b62 &&
         bitAt(frame, 63, inverted) == expected_b63;
}
