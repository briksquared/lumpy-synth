#pragma once

#include <Arduino.h>

// 16-step bass lane — scale degrees (0–6) or rest, played one octave down.
class BassSeq {
 public:
  using ScaleNoteFn = int (*)(int degree, void *ctx);

  static constexpr int kSteps = 16;
  static constexpr int8_t kRest = -1;

  void begin();
  void setScaleResolver(ScaleNoteFn fn, void *ctx);

  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  void setBpm(unsigned bpm);
  unsigned bpm() const { return bpm_; }

  void setSwing(float amount);
  float swing() const { return swing_; }

  void clear();
  void setStep(int index, int8_t degree);
  int8_t stepDegree(int index) const;

  void fillWalk();
  void doubleBar();
  void toggleStep(int index, int8_t degree);

  void reset();
  int currentStep() const { return stepIdx_; }

  void poll(unsigned long nowMs, void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);
  void clockTick(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);

  void printPattern(Stream &out) const;

 private:
  int8_t degrees_[kSteps]{};
  ScaleNoteFn scaleFn_ = nullptr;
  void *scaleCtx_ = nullptr;
  bool enabled_ = false;
  int stepIdx_ = 0;
  int playingNote_ = -1;
  unsigned bpm_ = 120;
  float swing_ = 0.0f;
  unsigned long lastStepMs_ = 0;

  unsigned stepIntervalMs() const;
  uint8_t noteForStep(int index) const;
  void advance(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);
};
