#include "bench_stash.h"

void BenchStash::capture(const ChordEngine &chords, const BenchLiveFlags &flags,
                         const BenchTransportState &transport, const SeqPlayer *seq,
                         const BassSeq *bass, const DrumGrid *drums, const HarmonyLane *harmony,
                         const MonoLead &mono, int activeSlot) {
  chords_ = chords;
  flags_ = flags;
  transport_ = transport;
  activeSlot_ = activeSlot;
#if BENCH_SEQUENCER
  if (seq) {
    seqLen_ = seq->length();
    seqBpm_ = seq->bpm();
    seqSwing_ = seq->swing();
    seqOn_ = seq->enabled();
    for (int i = 0; i < SeqPlayer::kMaxSteps; i++) {
      seqNotes_[i] = seq->stepNote(i);
    }
  }
#else
  (void)seq;
#endif
#if BENCH_BASS_SEQ
  if (bass) {
    bassOn_ = bass->enabled();
    bassSwing_ = bass->swing();
    for (int i = 0; i < BassSeq::kSteps; i++) {
      bassDeg_[i] = bass->stepDegree(i);
    }
  }
#else
  (void)bass;
#endif
#if BENCH_DRUM_GRID
  if (drums) {
    drumsOn_ = drums->enabled();
    drumLen_ = drums->length();
    drumSwing_ = drums->swing();
    for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
      drumKick_[i] = drums->kickAt(i);
      drumHat_[i] = drums->hatAt(i);
    }
  }
#else
  (void)drums;
#endif
#if BENCH_HARMONY
  if (harmony) {
    harmOn_ = harmony->enabled();
    harmSemi_ = harmony->semitones();
  }
#else
  (void)harmony;
#endif
  monoLegato_ = mono.legato();
  monoGlide_ = mono.glide();
  monoUnison_ = mono.unison();
  valid_ = true;
}

bool BenchStash::restore(ChordEngine &chords, BenchLiveFlags &flags, BenchTransportState &transport,
                         SeqPlayer *seq, BassSeq *bass, DrumGrid *drums, HarmonyLane *harmony,
                         MonoLead &mono, int &activeSlot) const {
  if (!valid_) return false;
  chords = chords_;
  flags = flags_;
  transport = transport_;
  activeSlot = activeSlot_;
#if BENCH_SEQUENCER
  if (seq) {
    seq->setLength(seqLen_);
    seq->setBpm(seqBpm_);
    seq->setSwing(seqSwing_);
    seq->setEnabled(seqOn_);
    for (int i = 0; i < SeqPlayer::kMaxSteps; i++) {
      seq->setStep(i, seqNotes_[i]);
    }
  }
#endif
#if BENCH_BASS_SEQ
  if (bass) {
    bass->setEnabled(bassOn_);
    bass->setSwing(bassSwing_);
    for (int i = 0; i < BassSeq::kSteps; i++) {
      bass->setStep(i, bassDeg_[i]);
    }
  }
#endif
#if BENCH_DRUM_GRID
  if (drums) {
    drums->setEnabled(drumsOn_);
    drums->setLength(drumLen_);
    drums->setSwing(drumSwing_);
    for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
      drums->setKick(i, drumKick_[i] != 0);
      drums->setHat(i, drumHat_[i] != 0);
    }
  }
#endif
#if BENCH_HARMONY
  if (harmony) {
    harmony->setSemitones(harmSemi_);
    harmony->setEnabled(harmOn_);
  }
#endif
  mono.setLegato(monoLegato_);
  mono.setGlide(monoGlide_);
  mono.setUnison(monoUnison_);
  return true;
}
