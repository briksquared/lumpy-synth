#pragma once

#include <Arduino.h>
#include <math.h>

// 4-pole ZDF lowpass — cascaded TPT stages + feedback (Moog-style lead).
class ZdfLadder4 {
 public:
  void setSampleRate(float fs) { fs_ = fs > 1000.0f ? fs : 44100.0f; updateG(); }

  void setCutoff(float hz) {
    cutoffHz_ = constrain(hz, 20.0f, fs_ * 0.45f);
    updateG();
  }

  void setResonance(float r) { res_ = constrain(r, 0.0f, 0.95f); }

  float cutoffHz() const { return cutoffHz_; }
  float resonance() const { return res_; }

  void reset() {
    s_[0] = s_[1] = s_[2] = s_[3] = 0.0f;
  }

  float process(float x) {
    const float fb = res_ * s_[3];
    float v = (x - fb) * (1.0f + res_ * 0.55f);
    for (int i = 0; i < 4; i++) {
      const float v0 = (v - s_[i]) * g_;
      const float y = v0 + s_[i];
      s_[i] = y;
      v = y;
    }
    return s_[3];
  }

 private:
  void updateG() {
    const float f = tanf(3.14159265f * cutoffHz_ / fs_);
    g_ = f / (1.0f + f);
  }

  float fs_ = 44100.0f;
  float g_ = 0.5f;
  float cutoffHz_ = 1000.0f;
  float res_ = 0.35f;
  float s_[4]{};
};
