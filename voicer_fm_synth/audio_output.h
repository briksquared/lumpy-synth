#pragma once

#include <Arduino.h>
#include <math.h>

// Lightweight stereo widen + tone control for the final mix.
class AudioOutput {
 public:
  void setWidth(float w) { width_ = constrain(w, 0.0f, 1.0f); }
  void setTone(float t) { tone_ = constrain(t, 0.0f, 1.0f); }

  void process(float mono, int16_t &left, int16_t &right) {
    const float bright = 0.05f + tone_ * 0.9f;
    lpState_ += bright * (mono - lpState_);
    const float dry = lpState_;

    const float wet = width_ * delayTap_;
    delayTap_ = dry;
    const float l = dry * (1.0f - width_ * 0.25f) + wet * width_ * 0.35f;
    const float r = dry * (1.0f - width_ * 0.25f) - wet * width_ * 0.35f;

    left = clip16(l);
    right = clip16(r);
    peak_ = peak_ * 0.995f + fabsf(mono) * 0.005f;
  }

  float peak() const { return peak_; }

 private:
  static int16_t clip16(float x) {
    if (x > 32767.0f) return 32767;
    if (x < -32768.0f) return -32768;
    return (int16_t)x;
  }

  float width_ = 0.45f;
  float tone_ = 0.85f;
  float lpState_ = 0.0f;
  float delayTap_ = 0.0f;
  float peak_ = 0.0f;
};
