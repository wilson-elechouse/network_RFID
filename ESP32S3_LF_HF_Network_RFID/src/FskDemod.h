#pragma once

#include <Arduino.h>

struct FskDemodStats {
  uint32_t accepted_periods = 0;
  uint32_t low_periods = 0;
  uint32_t high_periods = 0;
  uint32_t rejected_short = 0;
  uint32_t rejected_long = 0;
  uint32_t recovered_long = 0;
  uint32_t transitions = 0;
};

class FskDemod {
public:
  FskDemod(uint32_t low_time_us, uint32_t low_pulses, uint32_t high_time_us, uint32_t high_pulses);

  void reset();
  uint32_t feed(bool level, uint32_t duration_us, bool& value);
  FskDemodStats stats() const;

private:
  uint32_t low_time_us_;
  uint32_t low_pulses_;
  uint32_t high_time_us_;
  uint32_t high_pulses_;
  bool invert_ = false;
  uint32_t mid_time_us_ = 0;
  uint32_t period_time_us_ = 0;
  uint32_t pulse_count_ = 0;
  bool last_pulse_ = false;
  FskDemodStats stats_;

  uint32_t processPeriod(bool pulse, bool& value);
};
