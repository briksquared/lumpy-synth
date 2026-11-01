#pragma once

#include <Arduino.h>

enum class ChordType : uint8_t {
  Major = 0,
  Minor,
  Sus4,
  Maj7,
  Min7,
  Count
};

enum class ScaleMode : uint8_t {
  Major = 0,
  Minor,
  Pentatonic,
  Blues,
  Count
};

class ChordEngine {
 public:
  void setRoot(int midiNote) { root_ = constrain(midiNote, 36, 84); }
  int root() const { return root_; }

  void nextType() {
    type_ = (ChordType)(((int)type_ + 1) % (int)ChordType::Count);
  }

  void prevType() {
    int t = (int)type_ - 1;
    if (t < 0) t = (int)ChordType::Count - 1;
    type_ = (ChordType)t;
  }

  ChordType type() const { return type_; }
  void setType(ChordType t) { type_ = t; }

  const char *typeName() const;

  void setScale(ScaleMode mode) { scale_ = mode; }
  ScaleMode scale() const { return scale_; }
  void nextScale();
  const char *scaleName() const;

  void setInversion(int inv) { inversion_ = inv % 6; }
  int inversion() const { return inversion_; }
  void nextInversion() { inversion_ = (inversion_ + 1) % 6; }

  // Six-voice spread for current type + root + inversion.
  void fillVoices(int out[6]);

  // Arpeggiator — call at desired interval.
  void arpReset();
  bool arpEnabled() const { return arpOn_; }
  void setArp(bool on) { arpOn_ = on; arpReset(); }
  void arpToggle() { setArp(!arpOn_); }
  int arpStepIndex() const { return arpIdx_; }
  void arpAdvance();

  int voiceNote(int voice);

  // Diatonic degree 0–6 for one-octave keyboard (follows chord type).
  int scaleNote(int degree) const;
  int scaleNoteCount() const;

  // Snap incoming MIDI note to nearest tone in the active scale.
  int quantizeToScale(int midiNote) const;

 private:
  int root_ = 60;
  ChordType type_ = ChordType::Major;
  ScaleMode scale_ = ScaleMode::Major;
  int inversion_ = 0;
  bool arpOn_ = false;
  int arpIdx_ = 0;

  void buildTones();
  int tones_[6]{};
  int toneCount_ = 0;
};
