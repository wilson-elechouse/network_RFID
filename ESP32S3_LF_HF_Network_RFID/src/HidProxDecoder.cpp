#include "HidProxDecoder.h"

#include <string.h>

void HidProxDecoder::reset() {
  fsk_.reset();
  memset(h10301_words_, 0, sizeof(h10301_words_));
  memset(h10301_inverted_words_, 0, sizeof(h10301_inverted_words_));
  memset(generic_encoded_, 0, sizeof(generic_encoded_));
  memset(generic_inverted_encoded_, 0, sizeof(generic_inverted_encoded_));
  memset(&frame_voter_, 0, sizeof(frame_voter_));
  memset(&inverted_frame_voter_, 0, sizeof(inverted_frame_voter_));
  memset(pending_pulses_, 0, sizeof(pending_pulses_));
  memset(&filter_stats_, 0, sizeof(filter_stats_));
  pending_pulse_count_ = 0;
  fsk_bit_count_ = 0;
}

bool HidProxDecoder::pushPulse(
  bool level,
  uint32_t duration_us,
  HidH10301Id& h10301,
  HidGenericId& generic,
  HidDecodeInfo& info) {
  bool filtered_level = false;
  uint32_t filtered_duration_us = 0;
  if (!filterPulse(level, duration_us, filtered_level, filtered_duration_us)) {
    info.h10301 = false;
    info.generic = false;
    return false;
  }

  bool value = false;
  const uint32_t count = fsk_.feed(filtered_level, filtered_duration_us, value);
  bool decoded = false;
  info.h10301 = false;
  info.generic = false;

  for (uint32_t i = 0; i < count; i++) {
    pushFskBit(value);

    if (h10301CanDecode(h10301_words_)) {
      h10301Decode(h10301_words_, h10301);
      genericDecode(generic_encoded_, generic);
      info.h10301 = true;
      info.generic = true;
      decoded = true;
    } else if (h10301CanDecode(h10301_inverted_words_)) {
      h10301Decode(h10301_inverted_words_, h10301);
      genericDecode(generic_inverted_encoded_, generic);
      info.h10301 = true;
      info.generic = true;
      decoded = true;
    } else if (genericCanDecode(generic_encoded_)) {
      genericDecode(generic_encoded_, generic);
      info.generic = true;
      decoded = true;
    } else if (genericCanDecode(generic_inverted_encoded_)) {
      genericDecode(generic_inverted_encoded_, generic);
      info.generic = true;
      decoded = true;
    }

    if (!decoded && observeFrameBit(value, frame_voter_, h10301, generic, info)) {
      decoded = true;
    }
    if (!decoded && observeFrameBit(!value, inverted_frame_voter_, h10301, generic, info)) {
      decoded = true;
    }
  }

  if (decoded) {
    info.fsk_bits = fsk_bit_count_;
  }
  return decoded;
}

uint32_t HidProxDecoder::fskBitCount() const {
  return fsk_bit_count_;
}

HidPulseFilterStats HidProxDecoder::pulseFilterStats() const {
  return filter_stats_;
}

FskDemodStats HidProxDecoder::fskStats() const {
  return fsk_.stats();
}

void HidProxDecoder::copyEncodedPreview(char* out, size_t out_size) const {
  if (out == nullptr || out_size == 0) {
    return;
  }

  size_t bits_to_copy = fsk_bit_count_ < (GenericEncodedBytes * 8) ?
    fsk_bit_count_ : (GenericEncodedBytes * 8);
  if (bits_to_copy >= out_size) {
    bits_to_copy = out_size - 1;
  }

  const size_t start = (GenericEncodedBytes * 8) - bits_to_copy;
  for (size_t i = 0; i < bits_to_copy; i++) {
    out[i] = getBit(generic_encoded_, start + i) ? '1' : '0';
  }
  out[bits_to_copy] = '\0';
}

bool HidProxDecoder::filterPulse(
  bool level,
  uint32_t duration_us,
  bool& filtered_level,
  uint32_t& filtered_duration_us) {
  filter_stats_.raw_pulses++;
  if (duration_us == 0) {
    return false;
  }

  if (duration_us > MaxPulseUs) {
    pending_pulse_count_ = 0;
    fsk_.reset();
    filter_stats_.long_resets++;
    return false;
  }

  if (pending_pulse_count_ > 0 &&
      pending_pulses_[pending_pulse_count_ - 1].level == level) {
    pending_pulses_[pending_pulse_count_ - 1].duration_us += duration_us;
    filter_stats_.merged_same_level++;
  } else if (pending_pulse_count_ < 3) {
    pending_pulses_[pending_pulse_count_].level = level;
    pending_pulses_[pending_pulse_count_].duration_us = duration_us;
    pending_pulse_count_++;
  }

  if (pending_pulse_count_ < 3) {
    return false;
  }

  if (pending_pulses_[1].duration_us < MinPulseUs &&
      pending_pulses_[0].level == pending_pulses_[2].level) {
    pending_pulses_[0].duration_us += pending_pulses_[1].duration_us + pending_pulses_[2].duration_us;
    pending_pulse_count_ = 1;
    filter_stats_.dropped_glitches++;
    return false;
  }

  filtered_level = pending_pulses_[0].level;
  filtered_duration_us = pending_pulses_[0].duration_us;
  pending_pulses_[0] = pending_pulses_[1];
  pending_pulses_[1] = pending_pulses_[2];
  pending_pulses_[2] = PendingPulse();
  pending_pulse_count_ = 2;
  filter_stats_.filtered_pulses++;
  return true;
}

void HidProxDecoder::pushFskBit(bool bit) {
  pushFskBitTo(bit, h10301_words_, generic_encoded_);
  pushFskBitTo(!bit, h10301_inverted_words_, generic_inverted_encoded_);
  fsk_bit_count_++;
}

bool HidProxDecoder::observeFrameBit(
  bool bit,
  HidFrameVoter& voter,
  HidH10301Id& h10301,
  HidGenericId& generic,
  HidDecodeInfo& info) {
  voter.shift = (voter.shift << 1) | (bit ? 1U : 0U);

  if (voter.collecting) {
    appendFrameBit(voter, bit);
    if (voter.bit_index < HidFrameBits) {
      return false;
    }

    const bool exact_decoded = decodeFrame(voter.h10301_words, voter.generic_frame, h10301, generic, info);
    addFrameVotes(voter);
    resetFrameCapture(voter);
    if (exact_decoded) {
      resetFrameVotes(voter);
      return true;
    }
    return decodeVotedFrame(voter, h10301, generic, info);
  }

  if (voter.shift == Preamble) {
    startFrame(voter);
  }
  return false;
}

bool HidProxDecoder::decodeFrame(
  const uint32_t h10301_words[H10301EncodedWords],
  const uint8_t generic_frame[GenericEncodedBytes],
  HidH10301Id& h10301,
  HidGenericId& generic,
  HidDecodeInfo& info) {
  info.h10301 = false;
  info.generic = false;
  if (h10301CanDecode(h10301_words)) {
    h10301Decode(h10301_words, h10301);
    genericDecode(generic_frame, generic);
    info.h10301 = true;
    info.generic = true;
    return true;
  }

  if (genericCanDecodeFrame(generic_frame)) {
    genericDecode(generic_frame, generic);
    info.generic = true;
    return true;
  }
  return false;
}

bool HidProxDecoder::decodeVotedFrame(
  HidFrameVoter& voter,
  HidH10301Id& h10301,
  HidGenericId& generic,
  HidDecodeInfo& info) {
  if (voter.vote_count < 3) {
    return false;
  }

  uint32_t h10301_words[H10301EncodedWords] = {};
  uint8_t generic_frame[GenericEncodedBytes] = {};
  for (size_t i = 0; i < HidFrameBits; i++) {
    const bool bit = voter.votes[i] >= 0;
    pushH10301BitTo(bit, h10301_words);
    setBit(generic_frame, i, bit);
  }

  const bool decoded = decodeFrame(h10301_words, generic_frame, h10301, generic, info);
  if (decoded) {
    resetFrameVotes(voter);
  }
  return decoded;
}

void HidProxDecoder::addFrameVotes(HidFrameVoter& voter) {
  if (voter.vote_count >= 8) {
    resetFrameVotes(voter);
  }

  for (size_t i = 0; i < HidFrameBits; i++) {
    voter.votes[i] += getBit(voter.generic_frame, i) ? 1 : -1;
  }
  voter.vote_count++;
}

void HidProxDecoder::startFrame(HidFrameVoter& voter) {
  resetFrameCapture(voter);
  voter.collecting = true;
  for (int8_t i = 7; i >= 0; i--) {
    appendFrameBit(voter, (Preamble >> i) & 1U);
  }
}

void HidProxDecoder::appendFrameBit(HidFrameVoter& voter, bool bit) {
  if (voter.bit_index >= HidFrameBits) {
    return;
  }
  pushH10301BitTo(bit, voter.h10301_words);
  setBit(voter.generic_frame, voter.bit_index, bit);
  voter.bit_index++;
}

void HidProxDecoder::resetFrameCapture(HidFrameVoter& voter) {
  voter.collecting = false;
  voter.bit_index = 0;
  memset(voter.h10301_words, 0, sizeof(voter.h10301_words));
  memset(voter.generic_frame, 0, sizeof(voter.generic_frame));
}

void HidProxDecoder::resetFrameVotes(HidFrameVoter& voter) {
  voter.vote_count = 0;
  memset(voter.votes, 0, sizeof(voter.votes));
}

void HidProxDecoder::pushH10301BitTo(bool bit, uint32_t h10301_words[H10301EncodedWords]) {
  h10301_words[0] = (h10301_words[0] << 1) | ((h10301_words[1] >> 31) & 1U);
  h10301_words[1] = (h10301_words[1] << 1) | ((h10301_words[2] >> 31) & 1U);
  h10301_words[2] = (h10301_words[2] << 1) | (bit ? 1U : 0U);
}

void HidProxDecoder::pushFskBitTo(
  bool bit,
  uint32_t h10301_words[H10301EncodedWords],
  uint8_t generic_encoded[GenericEncodedBytes]) {
  pushH10301BitTo(bit, h10301_words);

  for (size_t i = 0; i + 1 < GenericEncodedBytes; i++) {
    generic_encoded[i] = (generic_encoded[i] << 1) | ((generic_encoded[i + 1] >> 7) & 1U);
  }
  generic_encoded[GenericEncodedBytes - 1] =
    (generic_encoded[GenericEncodedBytes - 1] << 1) | (bit ? 1U : 0U);
}

bool HidProxDecoder::h10301CanDecode(const uint32_t words[H10301EncodedWords]) {
  const uint8_t* encoded = reinterpret_cast<const uint8_t*>(words);
  if (encoded[3] != Preamble) {
    return false;
  }

  if (((words[0] >> 10) & 0x3FFF) != 0x1556) {
    return false;
  }

  const uint32_t format = ((words[0] & 0x3FF) << 12) | ((words[1] >> 20) & 0xFFF);
  if (format != 0x155556) {
    return false;
  }

  uint32_t result = 0;
  for (int8_t i = 9; i >= 0; i--) {
    const uint8_t pair = (words[1] >> (2 * i)) & 0x03;
    if (pair == 0x01) {
      result <<= 1;
    } else if (pair == 0x02) {
      result = (result << 1) | 1U;
    } else {
      return false;
    }
  }

  for (int8_t i = 15; i >= 0; i--) {
    const uint8_t pair = (words[2] >> (2 * i)) & 0x03;
    if (pair == 0x01) {
      result <<= 1;
    } else if (pair == 0x02) {
      result = (result << 1) | 1U;
    } else {
      return false;
    }
  }

  uint8_t parity_sum = 0;
  for (int8_t i = 0; i < 13; i++) {
    parity_sum += (result >> i) & 1U;
  }
  if ((parity_sum % 2) != 1) {
    return false;
  }

  parity_sum = 0;
  for (int8_t i = 13; i < 26; i++) {
    parity_sum += (result >> i) & 1U;
  }
  return (parity_sum % 2) == 0;
}

void HidProxDecoder::h10301Decode(const uint32_t words[H10301EncodedWords], HidH10301Id& id) {
  uint32_t result = 0;

  for (int8_t i = 9; i >= 0; i--) {
    const uint8_t pair = (words[1] >> (2 * i)) & 0x03;
    result = (result << 1) | (pair == 0x02 ? 1U : 0U);
  }

  for (int8_t i = 15; i >= 0; i--) {
    const uint8_t pair = (words[2] >> (2 * i)) & 0x03;
    result = (result << 1) | (pair == 0x02 ? 1U : 0U);
  }

  id.raw[0] = static_cast<uint8_t>(result >> 17);
  id.raw[1] = static_cast<uint8_t>(result >> 9);
  id.raw[2] = static_cast<uint8_t>(result >> 1);
  id.facility = id.raw[0];
  id.card = (static_cast<uint16_t>(id.raw[1]) << 8) | id.raw[2];
}

bool HidProxDecoder::genericCanDecode(const uint8_t encoded[GenericEncodedBytes]) {
  if (encoded[0] != Preamble || encoded[1 + GenericDataBytes] != Preamble) {
    return false;
  }

  return genericCanDecodeFrame(encoded);
}

bool HidProxDecoder::genericCanDecodeFrame(const uint8_t frame[GenericEncodedBytes]) {
  if (frame[0] != Preamble) {
    return false;
  }

  for (size_t i = 1; i <= GenericDataBytes; i++) {
    for (size_t n = 0; n < 4; n++) {
      const uint8_t pair = (frame[i] >> (n * 2)) & 0x03;
      if (pair == 0x00 || pair == 0x03) {
        return false;
      }
    }
  }

  return true;
}

void HidProxDecoder::genericDecode(const uint8_t encoded[GenericEncodedBytes], HidGenericId& id) {
  memset(id.data, 0, sizeof(id.data));
  size_t bit_index = 0;

  for (size_t i = 1; i <= GenericDataBytes; i++) {
    for (size_t n = 0; n < 4; n++) {
      const uint8_t pair = (encoded[i] >> (6 - (n * 2))) & 0x03;
      if (pair == 0x01) {
        setBit(id.data, bit_index, false);
      } else if (pair == 0x02) {
        setBit(id.data, bit_index, true);
      }
      bit_index++;
    }
  }

  id.bit_size = genericDecodeBitSize(id.data);
}

uint8_t HidProxDecoder::genericDecodeBitSize(const uint8_t data[GenericDecodedBytes]) {
  for (size_t bit_index = 0; bit_index < 6; bit_index++) {
    if (getBit(data, bit_index)) {
      return GenericDecodedBits - bit_index - 1;
    }
  }

  if (!getBit(data, 6)) {
    return 37;
  }

  size_t bit_index = 7;
  uint8_t size = 36;
  while (!getBit(data, bit_index) && size >= 26) {
    size--;
    bit_index++;
  }

  return size < 26 ? 0 : size;
}

bool HidProxDecoder::getBit(const uint8_t* data, size_t bit_index) {
  return (data[bit_index / 8] >> (7 - (bit_index % 8))) & 1U;
}

void HidProxDecoder::setBit(uint8_t* data, size_t bit_index, bool value) {
  const uint8_t mask = 1U << (7 - (bit_index % 8));
  if (value) {
    data[bit_index / 8] |= mask;
  } else {
    data[bit_index / 8] &= ~mask;
  }
}
