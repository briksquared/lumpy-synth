#pragma once

#include <Arduino.h>
#include "StkAll.h"

// Plays a fixed interval above the lead sequencer note.
class HarmonyLane {
 public:
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  void setSemitones(int semi);
  int semitones() const { return semitones_; }

  void onLeadStep(uint8_t leadNote, bool on, stk::Voicer &voicer, int group, float amp);

  void silence(stk::Voicer &voicer, int group, float amp);

 private:
  bool enabled_ = false;
  int semitones_ = 4;
  int playingNote_ = -1;
};
