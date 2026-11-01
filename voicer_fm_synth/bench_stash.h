#pragma once

#include <Arduino.h>
#include "bench_config.h"
#include "chord_engine.h"
#include "preset_store.h"
#include "seq_player.h"
#include "bass_seq.h"
#include "drum_grid.h"
#include "mono_lead.h"
#include "harmony_lane.h"

// RAM snapshot for audition / undo (not persisted to EEPROM).
struct BenchLiveFlags {
  bool demoActive = false;
  bool midiQuantize = false;
  bool genMode = false;
  bool monoMode = false;
  bool seqAccent = true;
  int octaveShift = 0;
#if BENCH_DRUM_PADS
  bool drumPadMode = true;
  bool drumWriteMode = false;
#endif
#if BENCH_SEQUENCER
  bool seqWriteMode = false;
  bool seqGridMode = false;
#endif
#if BENCH_BASS_SEQ
  bool bassGridMode = false;
#endif
  bool muteSeq = false;
  bool muteBass = false;
  bool muteDrums = false;
};

class BenchStash {
 public:
  bool valid() const { return valid_; }
  void clear() { valid_ = false; }

  int activeSlot() const { return activeSlot_; }

  void capture(const ChordEngine &chords, const BenchLiveFlags &flags,
               const BenchTransportState &transport, const SeqPlayer *seq,
               const BassSeq *bass, const DrumGrid *drums, const HarmonyLane *harmony,
               const MonoLead &mono, int activeSlot);

  bool restore(ChordEngine &chords, BenchLiveFlags &flags, BenchTransportState &transport,
               SeqPlayer *seq, BassSeq *bass, DrumGrid *drums, HarmonyLane *harmony,
               MonoLead &mono, int &activeSlot) const;

 private:
  bool valid_ = false;
  int activeSlot_ = 0;
  ChordEngine chords_{};
  BenchLiveFlags flags_{};
  BenchTransportState transport_{};

  uint8_t seqNotes_[SeqPlayer::kMaxSteps]{};
  int seqLen_ = SeqPlayer::kSteps;
  unsigned seqBpm_ = 120;
  float seqSwing_ = 0.0f;
  bool seqOn_ = false;

  int8_t bassDeg_[BassSeq::kSteps]{};
  float bassSwing_ = 0.0f;
  bool bassOn_ = false;

  uint8_t drumKick_[DrumGrid::kMaxSteps]{};
  uint8_t drumHat_[DrumGrid::kMaxSteps]{};
  int drumLen_ = DrumGrid::kSteps;
  float drumSwing_ = 0.0f;
  bool drumsOn_ = false;

  bool harmOn_ = false;
  int harmSemi_ = 4;
  bool monoLegato_ = true;
  bool monoGlide_ = true;
  bool monoUnison_ = false;
};
