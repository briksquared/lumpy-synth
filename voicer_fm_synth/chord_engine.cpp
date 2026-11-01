#include "chord_engine.h"

static const int kMajor[] = {0, 4, 7, 12, 16, 19};
static const int kMinor[] = {0, 3, 7, 12, 15, 19};
static const int kSus4[] = {0, 5, 7, 12, 17, 19};
static const int kMaj7[] = {0, 4, 7, 11, 16, 19};
static const int kMin7[] = {0, 3, 7, 10, 15, 19};

const char *ChordEngine::typeName() const {
  switch (type_) {
    case ChordType::Major: return "maj";
    case ChordType::Minor: return "min";
    case ChordType::Sus4: return "sus4";
    case ChordType::Maj7: return "maj7";
    case ChordType::Min7: return "min7";
    default: return "?";
  }
}

void ChordEngine::buildTones() {
  const int *src = kMajor;
  switch (type_) {
    case ChordType::Minor: src = kMinor; break;
    case ChordType::Sus4: src = kSus4; break;
    case ChordType::Maj7: src = kMaj7; break;
    case ChordType::Min7: src = kMin7; break;
    default: break;
  }
  for (int i = 0; i < 6; i++) {
    tones_[i] = root_ + src[(i + inversion_) % 6];
  }
  toneCount_ = 6;
}

void ChordEngine::fillVoices(int out[6]) {
  buildTones();
  for (int i = 0; i < 6; i++) out[i] = tones_[i];
}

int ChordEngine::voiceNote(int voice) {
  buildTones();
  if (voice < 0 || voice >= toneCount_) return root_;
  return tones_[voice];
}

void ChordEngine::arpReset() { arpIdx_ = 0; }

void ChordEngine::arpAdvance() {
  buildTones();
  arpIdx_ = (arpIdx_ + 1) % toneCount_;
}

int ChordEngine::scaleNote(int degree) const {
  static const int kMaj[] = {0, 2, 4, 5, 7, 9, 11};
  static const int kMin[] = {0, 2, 3, 5, 7, 8, 10};
  static const int kPent[] = {0, 2, 4, 7, 9, 12, 14};
  static const int kBlues[] = {0, 3, 5, 6, 7, 10, 12};
  const int *table = kMaj;
  int count = 7;
  switch (scale_) {
    case ScaleMode::Minor: table = kMin; break;
    case ScaleMode::Pentatonic: table = kPent; break;
    case ScaleMode::Blues: table = kBlues; break;
    default: break;
  }
  const int idx = ((degree % count) + count) % count;
  return root_ + table[idx];
}

int ChordEngine::scaleNoteCount() const { return 7; }

void ChordEngine::nextScale() {
  scale_ = (ScaleMode)(((int)scale_ + 1) % (int)ScaleMode::Count);
}

const char *ChordEngine::scaleName() const {
  switch (scale_) {
    case ScaleMode::Major: return "maj";
    case ScaleMode::Minor: return "min";
    case ScaleMode::Pentatonic: return "pent";
    case ScaleMode::Blues: return "blues";
    default: return "?";
  }
}

int ChordEngine::quantizeToScale(int midiNote) const {
  int best = midiNote;
  int bestDist = 128;
  for (int octave = -3; octave <= 3; octave++) {
    for (int degree = 0; degree < 7; degree++) {
      const int cand = scaleNote(degree) + octave * 12;
      if (cand < 0 || cand > 127) continue;
      const int dist = abs(cand - midiNote);
      if (dist < bestDist) {
        bestDist = dist;
        best = cand;
      }
    }
  }
  return best;
}
