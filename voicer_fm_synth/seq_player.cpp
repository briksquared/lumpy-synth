#include "seq_player.h"
#include "transport_timing.h"

void SeqPlayer::begin() {
  clear();
  bpm_ = 120;
  length_ = kSteps;
  swing_ = 0.0f;
}

void SeqPlayer::setEnabled(bool on) {
  enabled_ = on;
  if (!on) {
    record_ = false;
    playingNote_ = -1;
  }
}

void SeqPlayer::setBpm(unsigned bpm) {
  bpm_ = constrain(bpm, 40u, 240u);
}

void SeqPlayer::setLength(int steps) {
  length_ = constrain(steps, 1, kMaxSteps);
  if (stepIdx_ >= length_) stepIdx_ = 0;
  if (recordIdx_ > length_) recordIdx_ = length_;
}

void SeqPlayer::setSwing(float amount) {
  swing_ = constrain(amount, 0.0f, 0.55f);
}

void SeqPlayer::clear() {
  for (int i = 0; i < kMaxSteps; i++) notes_[i] = kRest;
  stepIdx_ = 0;
  playingNote_ = -1;
  recordIdx_ = 0;
}

void SeqPlayer::setStep(int index, uint8_t midiNote) {
  if (index < 0 || index >= kMaxSteps) return;
  notes_[index] = midiNote;
}

uint8_t SeqPlayer::stepNote(int index) const {
  if (index < 0 || index >= kMaxSteps) return kRest;
  return notes_[index];
}

void SeqPlayer::fillScale(int (*scaleNoteFn)(int degree, void *ctx), void *ctx) {
  if (!scaleNoteFn) return;
  static const int8_t degrees[kSteps] = {
      0, 2, 4, 2, 0, 1, 2, 4, 2, 0, -1, 0, 1, 2, 1, 0,
  };
  const int n = length_ < kSteps ? length_ : kSteps;
  for (int i = 0; i < n; i++) {
    if (degrees[i] < 0) {
      notes_[i] = kRest;
    } else {
      notes_[i] = (uint8_t)constrain(scaleNoteFn(degrees[i], ctx), 0, 127);
    }
  }
  stepIdx_ = 0;
}

void SeqPlayer::fillScale32(int (*scaleNoteFn)(int degree, void *ctx), void *ctx) {
  if (!scaleNoteFn) return;
  static const int8_t degrees[kMaxSteps] = {
      0, 2, 4, 2, 0, 1, 2, 4, 2, 0, -1, 0, 1, 2, 1, 0,
      0, 2, 4, 5, 4, 2, 0, -1, 0, 1, 2, 4, 2, 0, 1, 0,
  };
  length_ = kMaxSteps;
  for (int i = 0; i < kMaxSteps; i++) {
    if (degrees[i] < 0) {
      notes_[i] = kRest;
    } else {
      notes_[i] = (uint8_t)constrain(scaleNoteFn(degrees[i], ctx), 0, 127);
    }
  }
  stepIdx_ = 0;
}

void SeqPlayer::doubleBar() {
  const int half = (length_ < kSteps) ? length_ : kSteps;
  const int newLen = half * 2;
  if (newLen > kMaxSteps) return;
  for (int i = 0; i < half; i++) {
    notes_[half + i] = notes_[i];
  }
  length_ = newLen;
  if (stepIdx_ >= length_) stepIdx_ = 0;
}

void SeqPlayer::reversePattern() {
  for (int i = 0; i < length_ / 2; i++) {
    const uint8_t t = notes_[i];
    notes_[i] = notes_[length_ - 1 - i];
    notes_[length_ - 1 - i] = t;
  }
}

void SeqPlayer::shiftSteps(int delta) {
  if (length_ <= 1) return;
  int d = delta % length_;
  if (d < 0) d += length_;
  if (d == 0) return;
  uint8_t tmp[kMaxSteps];
  for (int i = 0; i < length_; i++) tmp[i] = notes_[i];
  for (int i = 0; i < length_; i++) {
    notes_[i] = tmp[(i - d + length_) % length_];
  }
}

void SeqPlayer::setBounce(bool on) {
  bounce_ = on;
  bounceDir_ = 1;
}

void SeqPlayer::toggleStep(int index, uint8_t note) {
  if (index < 0 || index >= kMaxSteps || note == 0) return;
  if (notes_[index] == note) {
    notes_[index] = kRest;
  } else {
    notes_[index] = note;
  }
}

void SeqPlayer::fillRandom(int (*scaleNoteFn)(int degree, void *ctx), void *ctx,
                            int densityPct) {
  if (!scaleNoteFn) return;
  const int dens = constrain(densityPct, 10, 100);
  static const int8_t degrees[] = {0, 1, 2, 3, 4, 5, 6, 2, 4, 0, 1, 3, 5, 2, 0, 4};
  for (int i = 0; i < length_; i++) {
    if (random(100) >= dens) {
      notes_[i] = kRest;
    } else {
      const int deg = degrees[random((int)(sizeof(degrees) / sizeof(degrees[0])))];
      notes_[i] = (uint8_t)constrain(scaleNoteFn(deg, ctx), 1, 127);
    }
  }
  stepIdx_ = 0;
}

void SeqPlayer::startRecord() {
  record_ = true;
  overdub_ = false;
  recordIdx_ = 0;
  clear();
}

void SeqPlayer::startOverdub() {
  record_ = true;
  overdub_ = true;
  recordIdx_ = 0;
}

void SeqPlayer::stopRecord() {
  record_ = false;
  overdub_ = false;
}

void SeqPlayer::captureNote(uint8_t note) {
  if (!record_) return;
  while (recordIdx_ < length_ && overdub_ && notes_[recordIdx_] != kRest) {
    recordIdx_++;
  }
  if (recordIdx_ >= length_) return;
  notes_[recordIdx_++] = note;
}

void SeqPlayer::reset() {
  stepIdx_ = 0;
  bounceDir_ = 1;
  playingNote_ = -1;
  lastStepMs_ = millis();
}

unsigned SeqPlayer::stepIntervalMs() const {
  return (unsigned)(60000UL / (bpm_ * 4UL));
}

void SeqPlayer::advance(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx) {
  if (playingNote_ > 0 && stepFn) {
    stepFn((uint8_t)playingNote_, false, ctx);
  }
  const uint8_t n = notes_[stepIdx_];
  if (n > 0 && stepFn) {
    stepFn(n, true, ctx);
  }
  playingNote_ = n;
  if (bounce_ && length_ > 1) {
    if (stepIdx_ <= 0) bounceDir_ = 1;
    else if (stepIdx_ >= length_ - 1) bounceDir_ = -1;
    stepIdx_ += bounceDir_;
  } else {
    stepIdx_ = (stepIdx_ + 1) % length_;
  }
  lastStepMs_ = millis();
}

void SeqPlayer::poll(unsigned long nowMs, void (*stepFn)(uint8_t note, bool on, void *ctx),
                     void *ctx) {
  if (!enabled_ || record_) return;
  const int prev = (stepIdx_ + length_ - 1) % length_;
  const unsigned wait =
      transportSwingInterval(stepIntervalMs(), prev, swing_);
  if ((unsigned long)(nowMs - lastStepMs_) < wait) return;
  advance(stepFn, ctx);
}

void SeqPlayer::clockTick(void (*stepFn)(uint8_t note, bool on, void *ctx), void *ctx) {
  if (!enabled_ || record_) return;
  advance(stepFn, ctx);
}

void SeqPlayer::printPattern(Stream &out) const {
  for (int i = 0; i < length_; i++) {
    if (i > 0) out.print(' ');
    if (notes_[i] == kRest) {
      out.print('-');
    } else {
      out.print(notes_[i]);
    }
  }
  out.println();
}
