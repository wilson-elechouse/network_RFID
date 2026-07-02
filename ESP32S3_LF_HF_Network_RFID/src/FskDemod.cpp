#include "FskDemod.h"

FskDemod::FskDemod(
  uint32_t low_time_us,
  uint32_t low_pulses,
  uint32_t high_time_us,
  uint32_t high_pulses) {
  if (low_time_us > high_time_us) {
    const uint32_t tmp_time = high_time_us;
    high_time_us = low_time_us;
    low_time_us = tmp_time;

    const uint32_t tmp_pulses = high_pulses;
    high_pulses = low_pulses;
    low_pulses = tmp_pulses;

    invert_ = true;
  }

  low_time_us_ = low_time_us;
  low_pulses_ = low_pulses;
  high_time_us_ = high_time_us;
  high_pulses_ = high_pulses;
  mid_time_us_ = low_time_us_ + ((high_time_us_ - low_time_us_) / 2);
  reset();
}

void FskDemod::reset() {
  period_time_us_ = 0;
  pulse_count_ = 0;
  last_pulse_ = false;
  stats_ = FskDemodStats();
}

uint32_t FskDemod::feed(bool level, uint32_t duration_us, bool& value) {
  if (level) {
    period_time_us_ = duration_us;
    return 0;
  }

  period_time_us_ += duration_us;
  if (period_time_us_ < low_time_us_) {
    stats_.rejected_short++;
    pulse_count_ = 0;
    return 0;
  }

  if (period_time_us_ >= high_time_us_) {
    for (uint32_t repeat = 2; repeat <= 4; repeat++) {
      const uint32_t divided_time_us = (period_time_us_ + (repeat / 2)) / repeat;
      if (divided_time_us >= low_time_us_ && divided_time_us < high_time_us_) {
        const bool pulse = divided_time_us >= mid_time_us_;
        uint32_t data_count = 0;
        for (uint32_t i = 0; i < repeat; i++) {
          data_count += processPeriod(pulse, value);
        }
        stats_.recovered_long++;
        return data_count;
      }
    }

    stats_.rejected_long++;
    pulse_count_ = 0;
    return 0;
  }

  const bool pulse = period_time_us_ >= mid_time_us_;
  return processPeriod(pulse, value);
}

FskDemodStats FskDemod::stats() const {
  return stats_;
}

uint32_t FskDemod::processPeriod(bool pulse, bool& value) {
  stats_.accepted_periods++;
  if (pulse) {
    stats_.high_periods++;
  } else {
    stats_.low_periods++;
  }
  pulse_count_++;

  if (last_pulse_ == pulse) {
    return 0;
  }

  uint32_t data_count = pulse_count_ + 2;
  if (last_pulse_) {
    data_count /= high_pulses_;
    value = !invert_;
  } else {
    data_count /= low_pulses_;
    value = invert_;
  }

  pulse_count_ = 0;
  last_pulse_ = pulse;
  stats_.transitions++;
  return data_count;
}
