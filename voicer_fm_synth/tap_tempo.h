#pragma once

#include <Arduino.h>

// Tap-tempo helper — averages recent tap intervals.
class TapTempo {
 public:
  void reset() {
    count_ = 0;
    lastTapMs_ = 0;
  }

  // Returns BPM when a new estimate is available, else 0.
  unsigned tap(unsigned long nowMs) {
    if (lastTapMs_ > 0) {
      const unsigned long dt = nowMs - lastTapMs_;
      if (dt > 2500) {
        count_ = 0;
      } else if (dt >= 120) {
        intervals_[count_ % 4] = dt;
        if (count_ < 4) count_++;
        unsigned long sum = 0;
        for (int i = 0; i < count_; i++) sum += intervals_[i];
        const unsigned avg = (unsigned)(sum / (unsigned)count_);
        if (avg > 0) {
          lastBpm_ = constrain((unsigned)(60000UL / avg), 40u, 240u);
          lastTapMs_ = nowMs;
          return lastBpm_;
        }
      }
    }
    lastTapMs_ = nowMs;
    if (count_ == 0) count_ = 1;
    return 0;
  }

  unsigned lastBpm() const { return lastBpm_; }

 private:
  unsigned long lastTapMs_ = 0;
  unsigned long intervals_[4]{};
  int count_ = 0;
  unsigned lastBpm_ = 120;
};
