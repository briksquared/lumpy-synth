#pragma once

#include <Arduino.h>

// ADSR envelope 0..1 for ladder cutoff modulation.
class FilterEnv {
 public:
  void setAttackMs(float ms) { attackMs_ = ms < 0.5f ? 0.5f : ms; }
  void setDecayMs(float ms) { decayMs_ = ms < 1.0f ? 1.0f : ms; }
  void setSustain(float s) { sustain_ = constrain(s, 0.0f, 1.0f); }
  void setReleaseMs(float ms) { releaseMs_ = ms < 1.0f ? 1.0f : ms; }

  float attackMs() const { return attackMs_; }
  float decayMs() const { return decayMs_; }
  float sustain() const { return sustain_; }
  float releaseMs() const { return releaseMs_; }

  void trigger() {
    phase_ = Phase::Attack;
    if (attackMs_ <= 0.5f) {
      level_ = 1.0f;
      phase_ = Phase::Decay;
    }
  }

  void release() {
    if (phase_ != Phase::Idle) phase_ = Phase::Release;
  }

  void reset() {
    phase_ = Phase::Idle;
    level_ = 0.0f;
    lastMs_ = 0;
  }

  void poll(unsigned long nowMs) {
    if (phase_ == Phase::Idle) {
      lastMs_ = nowMs;
      return;
    }
    const unsigned long dt = (lastMs_ == 0 || nowMs < lastMs_) ? 0 : nowMs - lastMs_;
    lastMs_ = nowMs;
    if (dt == 0) return;
    const float step = (float)dt;
    switch (phase_) {
      case Phase::Attack:
        level_ += step / attackMs_;
        if (level_ >= 1.0f) {
          level_ = 1.0f;
          phase_ = Phase::Decay;
        }
        break;
      case Phase::Decay: {
        const float drop = step * (1.0f - sustain_) / decayMs_;
        level_ -= drop;
        if (level_ <= sustain_) {
          level_ = sustain_;
          phase_ = Phase::Sustain;
        }
        break;
      }
      case Phase::Sustain:
        break;
      case Phase::Release: {
        const float drop = step * level_ / releaseMs_;
        level_ -= drop;
        if (level_ <= 0.0f) {
          level_ = 0.0f;
          phase_ = Phase::Idle;
        }
        break;
      }
      default:
        break;
    }
  }

  float level() const { return level_; }
  bool active() const { return phase_ != Phase::Idle; }

 private:
  enum class Phase : uint8_t { Idle, Attack, Decay, Sustain, Release };
  Phase phase_ = Phase::Idle;
  float level_ = 0.0f;
  float attackMs_ = 6.0f;
  float decayMs_ = 200.0f;
  float sustain_ = 0.25f;
  float releaseMs_ = 320.0f;
  unsigned long lastMs_ = 0;
};
