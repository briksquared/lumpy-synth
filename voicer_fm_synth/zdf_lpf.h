#pragma once

#include <Arduino.h>
#include <math.h>

// One-pole lowpass — TPT / ZDF style (stable under modulation).
class ZdfOnePole {
 public:
  void setSampleRate(float fs) { fs_ = fs > 1000.0f ? fs : 44100.0f; }

  void setCutoff(float hz) {
    const float fc = constrain(hz, 20.0f, fs_ * 0.45f);
    cutoffHz_ = fc;
    const float f = tanf(3.14159265f * fc / fs_);
    g_ = f / (1.0f + f);
  }

  float cutoffHz() const { return cutoffHz_; }

  void reset() { state_ = 0.0f; }

  float process(float x) {
    const float v = (x - state_) * g_;
    const float y = v + state_;
    state_ = y;
    return y;
  }

 private:
  float fs_ = 44100.0f;
  float g_ = 0.5f;
  float cutoffHz_ = 1000.0f;
  float state_ = 0.0f;
};
