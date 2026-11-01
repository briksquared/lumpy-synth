#pragma once

#include "StkAll.h"

// Mono lead voice using Voicer note tags — legato, portamento, optional unison double.
class MonoLead {
 public:
  void reset() {
    tag_ = -1;
    tag2_ = -1;
    currentNote_ = -1;
    gliding_ = false;
  }

  void setLegato(bool on) { legato_ = on; }
  bool legato() const { return legato_; }

  void setGlide(bool on) { glide_ = on; }
  bool glide() const { return glide_; }

  void setUnison(bool on) { unison_ = on; }
  bool unison() const { return unison_; }

  bool active() const { return tag_ >= 0; }
  bool gliding() const { return gliding_; }
  int currentNote() const { return currentNote_; }

  void noteOn(stk::Voicer &voicer, int note, float amp, int group) {
    if (legato_ && tag_ >= 0 && currentNote_ >= 0 && currentNote_ != note) {
      if (glide_) {
        glideFrom_ = (float)currentNote_;
        glideTo_ = (float)note;
        glidePos_ = 0.0f;
        gliding_ = true;
      } else {
        voicer.setFrequency(tag_, (stk::StkFloat)note);
        syncUnisonPitch(voicer, (float)note);
        gliding_ = false;
      }
      currentNote_ = note;
      ensureUnison(voicer, note, amp, group);
      return;
    }

    releaseVoices(voicer, amp);
    tag_ = voicer.noteOn((stk::StkFloat)note, (stk::StkFloat)amp, group);
    currentNote_ = note;
    gliding_ = false;
    spawnUnison(voicer, note, amp, group);
  }

  void noteOff(stk::Voicer &voicer, int note, float amp) {
    if (currentNote_ != note || tag_ < 0) return;
    releaseVoices(voicer, amp);
    reset();
  }

  void silence(stk::Voicer &voicer, float amp) {
    releaseVoices(voicer, amp);
    reset();
  }

  void tickGlide(stk::Voicer &voicer) {
    if (!gliding_ || tag_ < 0) return;
    glidePos_ += glideRate_;
    if (glidePos_ >= 1.0f) {
      voicer.setFrequency(tag_, (stk::StkFloat)glideTo_);
      syncUnisonPitch(voicer, glideTo_);
      gliding_ = false;
      return;
    }
    const float n = glideFrom_ + (glideTo_ - glideFrom_) * glidePos_;
    voicer.setFrequency(tag_, (stk::StkFloat)n);
    syncUnisonPitch(voicer, n);
  }

 private:
  static constexpr float kUnisonCents = 0.14f;
  static constexpr float kUnisonAmp = 0.58f;
  static constexpr float glideRate_ = 0.0025f;

  long tag_ = -1;
  long tag2_ = -1;
  int currentNote_ = -1;
  bool legato_ = true;
  bool glide_ = true;
  bool unison_ = false;
  bool gliding_ = false;
  float glideFrom_ = 0.0f;
  float glideTo_ = 0.0f;
  float glidePos_ = 0.0f;

  void releaseVoices(stk::Voicer &voicer, float amp) {
    if (tag_ >= 0) {
      voicer.noteOff(tag_, (stk::StkFloat)amp);
      tag_ = -1;
    }
    if (tag2_ >= 0) {
      voicer.noteOff(tag2_, (stk::StkFloat)amp);
      tag2_ = -1;
    }
  }

  void spawnUnison(stk::Voicer &voicer, int note, float amp, int group) {
    if (!unison_) return;
    tag2_ = voicer.noteOn((stk::StkFloat)(note + kUnisonCents), (stk::StkFloat)(amp * kUnisonAmp),
                          group);
  }

  void ensureUnison(stk::Voicer &voicer, int note, float amp, int group) {
    if (!unison_) {
      if (tag2_ >= 0) {
        voicer.noteOff(tag2_, (stk::StkFloat)amp);
        tag2_ = -1;
      }
      return;
    }
    if (tag2_ < 0) {
      spawnUnison(voicer, note, amp, group);
      return;
    }
    if (!gliding_) {
      voicer.setFrequency(tag2_, (stk::StkFloat)(note + kUnisonCents));
    }
  }

  void syncUnisonPitch(stk::Voicer &voicer, float midiNote) {
    if (unison_ && tag2_ >= 0) {
      voicer.setFrequency(tag2_, (stk::StkFloat)(midiNote + kUnisonCents));
    }
  }
};
