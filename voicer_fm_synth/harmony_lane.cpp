#include "harmony_lane.h"

void HarmonyLane::setEnabled(bool on) {
  enabled_ = on;
  if (!on) playingNote_ = -1;
}

void HarmonyLane::setSemitones(int semi) {
  semitones_ = constrain(semi, 1, 12);
}

void HarmonyLane::onLeadStep(uint8_t leadNote, bool on, stk::Voicer &voicer, int group,
                            float amp) {
  if (!enabled_) return;
  if (!on) {
    if (playingNote_ > 0) {
      voicer.noteOff(playingNote_, (stk::StkFloat)amp, group);
      playingNote_ = -1;
    }
    return;
  }
  if (leadNote == 0) return;
  if (playingNote_ > 0) {
    voicer.noteOff(playingNote_, (stk::StkFloat)amp, group);
  }
  const int harm = (int)leadNote + semitones_;
  if (harm < 1 || harm > 127) {
    playingNote_ = -1;
    return;
  }
  voicer.noteOn((stk::StkFloat)harm, (stk::StkFloat)(amp * 0.82f), group);
  playingNote_ = harm;
}

void HarmonyLane::silence(stk::Voicer &voicer, int group, float amp) {
  if (playingNote_ > 0) {
    voicer.noteOff(playingNote_, (stk::StkFloat)amp, group);
  }
  playingNote_ = -1;
}
