#include "drum_grid.h"
#include "transport_timing.h"
#include <math.h>

void DrumGrid::begin() {
  clear();
  bpm_ = 120;
}

void DrumGrid::setEnabled(bool on) {
  enabled_ = on;
  if (!on) {
    kickEnv_ = 0.0f;
    hatEnv_ = 0.0f;
  }
}

void DrumGrid::setBpm(unsigned bpm) {
  bpm_ = constrain(bpm, 40u, 240u);
}

void DrumGrid::setSwing(float amount) {
  swing_ = constrain(amount, 0.0f, 0.55f);
}

void DrumGrid::setLength(int steps) {
  length_ = constrain(steps, 1, kMaxSteps);
  if (stepIdx_ >= length_) stepIdx_ = 0;
}

void DrumGrid::clear() {
  for (int i = 0; i < kMaxSteps; i++) {
    kick_[i] = 0;
    hat_[i] = 0;
  }
  stepIdx_ = 0;
  length_ = kSteps;
}

void DrumGrid::fillFourOnFloor() {
  length_ = kSteps;
  for (int i = 0; i < kSteps; i++) {
    kick_[i] = (i % 4 == 0) ? 1 : 0;
    hat_[i] = (i % 2 == 1) ? 1 : 0;
  }
}

void DrumGrid::fill32() {
  fillFourOnFloor();
  length_ = kMaxSteps;
  for (int i = kSteps; i < kMaxSteps; i++) {
    const int local = i - kSteps;
    kick_[i] = (local == 2 || local == 6 || local == 10 || local == 14) ? 1 : 0;
    hat_[i] = (local % 2 == 0) ? 1 : 0;
  }
}

void DrumGrid::fillHats() {
  for (int i = 0; i < length_; i++) {
    hat_[i] = (i % 2 == 1) ? 1 : 0;
  }
}

void DrumGrid::doubleBar() {
  if (length_ < kSteps) length_ = kSteps;
  for (int i = 0; i < kSteps; i++) {
    kick_[kSteps + i] = kick_[i];
    hat_[kSteps + i] = hat_[i];
  }
  length_ = kMaxSteps;
}

void DrumGrid::setKick(int step, bool on) {
  if (step < 0 || step >= kMaxSteps) return;
  kick_[step] = on ? 1 : 0;
}

void DrumGrid::setHat(int step, bool on) {
  if (step < 0 || step >= kMaxSteps) return;
  hat_[step] = on ? 1 : 0;
}

uint8_t DrumGrid::kickAt(int step) const {
  if (step < 0 || step >= kMaxSteps) return 0;
  return kick_[step];
}

uint8_t DrumGrid::hatAt(int step) const {
  if (step < 0 || step >= kMaxSteps) return 0;
  return hat_[step];
}

unsigned DrumGrid::stepIntervalMs() const {
  return (unsigned)(60000UL / (bpm_ * 4UL));
}

void DrumGrid::triggerKick() {
  kickEnv_ = 1.0f;
  kickPhase_ = 0.0f;
}

void DrumGrid::triggerHat() {
  hatEnv_ = 1.0f;
}

void DrumGrid::fireKick() { triggerKick(); }

void DrumGrid::fireHat() { triggerHat(); }

void DrumGrid::reset() {
  stepIdx_ = 0;
  lastStepMs_ = millis();
}

void DrumGrid::advanceStep() {
  if (kick_[stepIdx_]) triggerKick();
  if (hat_[stepIdx_]) triggerHat();
  stepIdx_ = (stepIdx_ + 1) % length_;
  lastStepMs_ = millis();
}

void DrumGrid::poll(unsigned long nowMs) {
  if (!enabled_) return;
  const int prev = (stepIdx_ + length_ - 1) % length_;
  const unsigned wait = transportSwingInterval(stepIntervalMs(), prev, swing_);
  if ((unsigned long)(nowMs - lastStepMs_) < wait) return;
  advanceStep();
}

void DrumGrid::clockTick() {
  if (!enabled_) return;
  advanceStep();
}

float DrumGrid::tickSample() {
  float out = 0.0f;

  if (kickEnv_ > 0.0001f) {
    kickPhase_ += 0.012f;
    const float body = sinf(kickPhase_ * 6.2831853f) * kickEnv_;
    out += body * 9000.0f;
    kickEnv_ *= 0.9992f;
  }

  if (hatEnv_ > 0.0001f) {
    hatRng_ = hatRng_ * 1664525u + 1013904223u;
    const float n = ((int)(hatRng_ >> 16) - 32768) / 32768.0f;
    out += n * hatEnv_ * 2200.0f;
    hatEnv_ *= 0.996f;
  }

  return out;
}

void DrumGrid::printPattern(Stream &out) const {
  out.print(F("drum len "));
  out.println(length_);
  out.print(F("kick "));
  for (int i = 0; i < length_; i++) out.print(kick_[i] ? 'X' : '.');
  out.println();
  out.print(F("hat  "));
  for (int i = 0; i < length_; i++) out.print(hat_[i] ? 'x' : '.');
  out.println();
}
