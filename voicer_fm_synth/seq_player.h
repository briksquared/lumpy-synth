#pragma once

#include <Arduino.h>

// Monophonic sequencer — up to 32 steps (default 16).
class SeqPlayer {
 public:
  static constexpr int kMaxSteps = 32;
  static constexpr int kSteps = 16;
  static constexpr uint8_t kRest = 0;

  void begin();
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  void setBpm(unsigned bpm);
  unsigned bpm() const { return bpm_; }

  void setLength(int steps);
  int length() const { return length_; }

  void setSwing(float amount);
  float swing() const { return swing_; }

  void clear();
  void setStep(int index, uint8_t midiNote);
  uint8_t stepNote(int index) const;

  void fillScale(int (*scaleNoteFn)(int degree, void *ctx), void *ctx);
  void fillScale32(int (*scaleNoteFn)(int degree, void *ctx), void *ctx);

  void doubleBar();
  void reversePattern();
  void shiftSteps(int delta);

  void setBounce(bool on);
  bool bounce() const { return bounce_; }

  void toggleStep(int index, uint8_t note);
  void fillRandom(int (*scaleNoteFn)(int degree, void *ctx), void *ctx, int densityPct);

  void startRecord();
  void startOverdub();
  void stopRecord();
  bool recording() const { return record_; }
  bool overdub() const { return overdub_; }
  void captureNote(uint8_t note);

  void reset();
  int currentStep() const { return stepIdx_; }
  int playingNote() const { return playingNote_; }

  void poll(unsigned long nowMs, void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);
  void clockTick(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);

  void printPattern(Stream &out) const;

 private:
  uint8_t notes_[kMaxSteps]{};
  bool enabled_ = false;
  bool record_ = false;
  bool overdub_ = false;
  int recordIdx_ = 0;
  int stepIdx_ = 0;
  int playingNote_ = -1;
  int length_ = kSteps;
  float swing_ = 0.0f;
  bool bounce_ = false;
  int bounceDir_ = 1;
  unsigned bpm_ = 120;
  unsigned long lastStepMs_ = 0;

  unsigned stepIntervalMs() const;
  void advance(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx);
};
