#include "bass_seq.h"
#include "transport_timing.h"

void BassSeq::begin() {
  clear();
  bpm_ = 120;
}

void BassSeq::setScaleResolver(ScaleNoteFn fn, void *ctx) {
  scaleFn_ = fn;
  scaleCtx_ = ctx;
}

void BassSeq::setEnabled(bool on) {
  enabled_ = on;
  if (!on) playingNote_ = -1;
}

void BassSeq::setBpm(unsigned bpm) {
  bpm_ = constrain(bpm, 40u, 240u);
}

void BassSeq::setSwing(float amount) {
  swing_ = constrain(amount, 0.0f, 0.55f);
}

void BassSeq::clear() {
  for (int i = 0; i < kSteps; i++) degrees_[i] = kRest;
  stepIdx_ = 0;
  playingNote_ = -1;
}

void BassSeq::setStep(int index, int8_t degree) {
  if (index < 0 || index >= kSteps) return;
  degrees_[index] = (degree < 0 || degree > 6) ? kRest : degree;
}

int8_t BassSeq::stepDegree(int index) const {
  if (index < 0 || index >= kSteps) return kRest;
  return degrees_[index];
}

void BassSeq::fillWalk() {
  static const int8_t pat[kSteps] = {
      0, 4, 0, 2, 0, 4, 2, 4, 0, 4, 0, 2, 0, 1, 2, 0,
  };
  for (int i = 0; i < kSteps; i++) degrees_[i] = pat[i];
  stepIdx_ = 0;
}

void BassSeq::doubleBar() {
  static constexpr int kHalf = kSteps / 2;
  for (int i = 0; i < kHalf; i++) {
    degrees_[kHalf + i] = degrees_[i];
  }
}

void BassSeq::toggleStep(int index, int8_t degree) {
  if (index < 0 || index >= kSteps) return;
  if (degree < 0 || degree > 6) {
    degrees_[index] = kRest;
    return;
  }
  if (degrees_[index] == degree) {
    degrees_[index] = kRest;
  } else {
    degrees_[index] = degree;
  }
}

void BassSeq::reset() {
  stepIdx_ = 0;
  playingNote_ = -1;
  lastStepMs_ = millis();
}

unsigned BassSeq::stepIntervalMs() const {
  return (unsigned)(60000UL / (bpm_ * 4UL));
}

uint8_t BassSeq::noteForStep(int index) const {
  if (index < 0 || index >= kSteps) return 0;
  const int8_t deg = degrees_[index];
  if (deg < 0 || !scaleFn_) return 0;
  const int n = scaleFn_(deg, scaleCtx_) - 12;
  if (n < 1 || n > 127) return 0;
  return (uint8_t)n;
}

void BassSeq::advance(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx) {
  if (playingNote_ > 0 && stepFn) {
    stepFn((uint8_t)playingNote_, false, ctx);
  }
  const uint8_t n = noteForStep(stepIdx_);
  if (n > 0 && stepFn) {
    stepFn(n, true, ctx);
  }
  playingNote_ = n;
  stepIdx_ = (stepIdx_ + 1) % kSteps;
  lastStepMs_ = millis();
}

void BassSeq::poll(unsigned long nowMs, void (*stepFn)(uint8_t note, bool on, void *ctx),
                   void *ctx) {
  if (!enabled_) return;
  const int prev = (stepIdx_ + kSteps - 1) % kSteps;
  const unsigned wait = transportSwingInterval(stepIntervalMs(), prev, swing_);
  if ((unsigned long)(nowMs - lastStepMs_) < wait) return;
  advance(stepFn, ctx);
}

void BassSeq::clockTick(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx) {
  if (!enabled_) return;
  advance(stepFn, ctx);
}

void BassSeq::printPattern(Stream &out) const {
  out.print(F("bass "));
  for (int i = 0; i < kSteps; i++) {
    if (i > 0) out.print(' ');
    if (degrees_[i] < 0) {
      out.print('-');
    } else {
      out.print(degrees_[i]);
    }
  }
  out.println();
}
