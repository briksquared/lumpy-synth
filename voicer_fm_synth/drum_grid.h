#pragma once

#include <Arduino.h>

// Lightweight kick + hat layer — 16 or 32 steps (one or two bars).
class DrumGrid {
 public:
  static constexpr int kMaxSteps = 32;
  static constexpr int kSteps = 16;

  void begin();
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  void setBpm(unsigned bpm);
  unsigned bpm() const { return bpm_; }

  void setSwing(float amount);
  float swing() const { return swing_; }

  void setLength(int steps);
  int length() const { return length_; }

  void clear();
  void fillFourOnFloor();
  void fill32();
  void fillHats();
  void doubleBar();
  void setKick(int step, bool on);
  void setHat(int step, bool on);
  uint8_t kickAt(int step) const;
  uint8_t hatAt(int step) const;

  void poll(unsigned long nowMs);
  void clockTick();

  void fireKick();
  void fireHat();

  float tickSample();
  void reset();
  int currentStep() const { return stepIdx_; }

  void printPattern(Stream &out) const;

 private:
  void advanceStep();
  void triggerKick();
  void triggerHat();

  bool enabled_ = false;
  uint8_t kick_[kMaxSteps]{};
  uint8_t hat_[kMaxSteps]{};
  int stepIdx_ = 0;
  int length_ = kSteps;
  unsigned bpm_ = 120;
  float swing_ = 0.0f;
  unsigned long lastStepMs_ = 0;

  float kickEnv_ = 0.0f;
  float kickPhase_ = 0.0f;
  float hatEnv_ = 0.0f;
  uint32_t hatRng_ = 0xC0FFEE01;

  unsigned stepIntervalMs() const;
};
