#pragma once

#include <Arduino.h>

#include "FskDemod.h"

struct HidH10301Id {
  uint8_t facility = 0;
  uint16_t card = 0;
  uint8_t raw[3] = {};
};

struct HidGenericId {
  uint8_t data[6] = {};
  uint8_t bit_size = 0;
};

struct HidDecodeInfo {
  uint32_t fsk_bits = 0;
  bool h10301 = false;
  bool generic = false;
};

struct HidPulseFilterStats {
  uint32_t raw_pulses = 0;
  uint32_t filtered_pulses = 0;
  uint32_t merged_same_level = 0;
  uint32_t dropped_glitches = 0;
  uint32_t long_resets = 0;
};

class HidProxDecoder {
public:
  void reset();
  bool pushPulse(bool level, uint32_t duration_us, HidH10301Id& h10301, HidGenericId& generic, HidDecodeInfo& info);
  uint32_t fskBitCount() const;
  HidPulseFilterStats pulseFilterStats() const;
  FskDemodStats fskStats() const;
  void copyEncodedPreview(char* out, size_t out_size) const;

private:
  struct PendingPulse {
    bool level = false;
    uint32_t duration_us = 0;
  };

  static constexpr uint32_t MinPulseUs = 16;
  static constexpr uint32_t MaxPulseUs = 2000;
  static constexpr uint8_t Preamble = 0x1D;
  static constexpr size_t H10301EncodedWords = 3;
  static constexpr size_t HidFrameBits = 96;
  static constexpr size_t HidFrameBytes = HidFrameBits / 8;
  static constexpr size_t GenericEncodedBytes = 13;
  static constexpr size_t GenericDataBytes = 11;
  static constexpr size_t GenericDecodedBytes = 6;
  static constexpr size_t GenericDecodedBits = 44;

  struct HidFrameVoter {
    uint8_t shift = 0;
    bool collecting = false;
    uint8_t bit_index = 0;
    uint8_t vote_count = 0;
    uint32_t h10301_words[H10301EncodedWords] = {};
    uint8_t generic_frame[GenericEncodedBytes] = {};
    int16_t votes[HidFrameBits] = {};
  };

  FskDemod fsk_{44, 6, 100, 5};
  uint32_t h10301_words_[H10301EncodedWords] = {};
  uint32_t h10301_inverted_words_[H10301EncodedWords] = {};
  uint8_t generic_encoded_[GenericEncodedBytes] = {};
  uint8_t generic_inverted_encoded_[GenericEncodedBytes] = {};
  HidFrameVoter frame_voter_;
  HidFrameVoter inverted_frame_voter_;
  uint32_t fsk_bit_count_ = 0;
  PendingPulse pending_pulses_[3] = {};
  uint8_t pending_pulse_count_ = 0;
  HidPulseFilterStats filter_stats_;

  bool filterPulse(bool level, uint32_t duration_us, bool& filtered_level, uint32_t& filtered_duration_us);
  void pushFskBit(bool bit);
  static bool observeFrameBit(bool bit, HidFrameVoter& voter, HidH10301Id& h10301, HidGenericId& generic, HidDecodeInfo& info);
  static bool decodeFrame(const uint32_t h10301_words[H10301EncodedWords], const uint8_t generic_frame[GenericEncodedBytes], HidH10301Id& h10301, HidGenericId& generic, HidDecodeInfo& info);
  static bool decodeVotedFrame(HidFrameVoter& voter, HidH10301Id& h10301, HidGenericId& generic, HidDecodeInfo& info);
  static void addFrameVotes(HidFrameVoter& voter);
  static void startFrame(HidFrameVoter& voter);
  static void appendFrameBit(HidFrameVoter& voter, bool bit);
  static void resetFrameCapture(HidFrameVoter& voter);
  static void resetFrameVotes(HidFrameVoter& voter);
  static void pushH10301BitTo(bool bit, uint32_t h10301_words[H10301EncodedWords]);
  static void pushFskBitTo(bool bit, uint32_t h10301_words[H10301EncodedWords], uint8_t generic_encoded[GenericEncodedBytes]);
  static bool h10301CanDecode(const uint32_t words[H10301EncodedWords]);
  static void h10301Decode(const uint32_t words[H10301EncodedWords], HidH10301Id& id);
  static bool genericCanDecodeFrame(const uint8_t frame[GenericEncodedBytes]);
  static bool genericCanDecode(const uint8_t encoded[GenericEncodedBytes]);
  static void genericDecode(const uint8_t encoded[GenericEncodedBytes], HidGenericId& id);
  static uint8_t genericDecodeBitSize(const uint8_t data[GenericDecodedBytes]);
  static bool getBit(const uint8_t* data, size_t bit_index);
  static void setBit(uint8_t* data, size_t bit_index, bool value);
};
