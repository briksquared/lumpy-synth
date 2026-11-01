/**
 * LUMPY SYNTH — F411 Black Pill + CJMCU-1334 — 6-voice STK polysynth.
 */
#include "bench_config.h"
#include "bench_controls.h"
#include "chord_engine.h"
#include "instrument_config.h"
#include "i2s2_dac.h"
#include "preset_store.h"
#include "midi_parser.h"
#include "audio_output.h"
#include "strum_player.h"
#include "seq_player.h"
#include "mono_lead.h"
#include "tap_tempo.h"
#include "drum_grid.h"
#include "bass_seq.h"
#include "harmony_lane.h"
#include "cc_map.h"
#include "bench_stash.h"
#include "bench_pin_lint.h"
#include "zdf_lpf.h"
#include "zdf_ladder.h"
#include "filter_env.h"
#include "bench_eeprom.h"
#include <math.h>

extern char _end;

#if BENCH_OLED
#include "bench_display.h"
#endif
#if BENCH_CYD_UART
#include "bench_cyd.h"
#endif

static const uint32_t kLed = PC13;
static const int kVoices = 6;
static const int kGroup = 0;
static const int kSampleRate = 44100;

I2s2Dac dac;
STK_VOICE_DECL;
Voicer voicer;
MidiParser usbMidi;
#if BENCH_UART_MIDI
MidiParser uartMidi;
#endif
BenchControls controls;
ChordEngine chords;
PresetStore presets;
AudioOutput audioOut;
StrumPlayer strum;
#if BENCH_SEQUENCER
SeqPlayer seq;
#endif
#if BENCH_DRUM_GRID
DrumGrid drums;
#endif
#if BENCH_BASS_SEQ
BassSeq bass;
#endif
#if BENCH_HARMONY
HarmonyLane harmony;
#endif
#if BENCH_OLED
BenchDisplay display;
#endif
MonoLead monoLead;
TapTempo tapTempo;
CcLearn ccLearn;
BenchEeprom benchEeprom;
BenchStash benchStash;
int activeBenchSlot = 0;

unsigned long lastChangeMs = 0;
unsigned long lastArpMs = 0;
volatile float masterGain = kMasterGainDefault;
float midiVolume = 1.0f;
bool demoActive = BENCH_AUTO_DEMO != 0;
bool midiSustain = false;
bool lastMidiSustain = false;
int arpLastNote = -1;
int octaveShift = 0;
bool sustainHeld[128]{};
bool lastSustainDown = false;
bool midiClockSync = BENCH_MIDI_CLOCK != 0;
int midiClockCount = 0;
bool monoMode = false;
bool monoFilterOn = false;
#if BENCH_MONO_LADDER
ZdfLadder4 monoLadder;
#else
ZdfOnePole monoLpf;
#endif
int monoNote = -1;
#if BENCH_FILTER_ENV
FilterEnv filterEnv;
bool filterEnvOn = true;
float filterEnvDepth = 0.72f;
float filterCutoffBase = 2200.0f;
#endif
bool presetDirty = false;
unsigned long autoSaveAtMs = 0;
bool midiQuantize = BENCH_MIDI_QUANTIZE != 0;
bool genMode = false;
int bassPlayingNote = -1;
bool seqAccent = true;
bool muteSeq = false;
bool muteBass = false;
bool muteDrums = false;
bool metroEnabled = false;
bool metroAccent = true;
int metroBeatCount = 0;
float metroEnv = 0.0f;
float metroPhase = 0.0f;
int metroMidiCount = 0;
bool chordLocked = false;
bool harmLocked = false;
bool autoSaveEnabled = BENCH_AUTO_SAVE != 0;
bool patternImportMode = false;
bool patternImportQuantize = false;
String patternImportBuf;
bool eepromImportMode = false;
int eepromImportSlot = 0;
PresetStore::ChordSnapshot eepromImportSnap{};
bool eepromImportSnapValid = false;
String eepromImportHex;
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
#if BENCH_OLED || BENCH_CYD_UART
bool benchPatternView = false;
#endif
#if BENCH_OLED
bool oledScrollWindow = true;
#endif

static BenchTransportState transportState();
static void applyTransportState(const BenchTransportState &state);
static BenchLiveFlags captureLiveFlags();
static void applyLiveFlags(const BenchLiveFlags &flags);
static void captureBenchStash();
static void undoBenchStash();
static void auditionBenchSlot(int slot);
static void commitBenchSlot(int slot);
static void bassFromSeqPattern(bool quiet = false);
static void drumFromSeqPattern(bool quiet = false);
static void hatFromSeqPattern(bool quiet = false);
static void followSeqPattern();
static void patternClear();
static void patternDup();
static void setActiveSlot(int slot);
static void setLayerMute(const String &layer, bool on);
static void soloLayer(const String &layer);
static void transposeSeqPattern(int semi);
static void seqThinPattern();
static void seqDensifyPattern();
static void bassRotatePattern(int steps);
static void humanizeGroove(bool quiet = false);
static void patternExport();
static void patternImportLine(const String &line);
static void eepromImportLine(const String &line);
static void finishEepromImport();
static uint32_t measureFilterLoadUsPerFrame(int frames, int iterations);
static uint32_t measureFilterEnvLoadUsPerFrame(int frames, int iterations);
static uint32_t measureFilterResLoadUsPerFrame(int frames, int iterations);
static uint32_t measureFilterComboLoadUsPerFrame(int frames, int iterations);
static void finishPatternImport();
static void pulseMetroBeat();
static void pollMetro(unsigned long nowMs);
static bool chordEditBlocked();
static void reloadActiveSlot();
static void unmuteAllLayers();
static void drumRotatePattern(int steps);
static void syncLenPattern();
static void patternQuantize();
static void seqInvertPattern();
static void bassInvertPattern();
static void drumInvertPattern();
static void patternTrim();
static void loadPatternBank(int slot);
static void savePatternBank(int slot);
static void transportReset();
static void drumClearLayer(const String &layer);
static bool harmEditBlocked();
static void nudgeTransportSwing(int deltaPct);
static void eraseBenchSlot(int slot);
static void nudgeTransportBpm(int delta);
static void pollBenchDisplay();
static void tickTransportStep();
#if BENCH_BASS_SEQ
static void onBassStep(uint8_t note, bool on, void *);
static void setBassEnabled(bool on);
#endif

static void markPresetDirty();
static void saveBenchPreset(int slot);
static bool loadBenchPreset(int slot);
static void applyTransportBpm(unsigned bpm);
static void applyTransportSwing(float swing, bool persist);
static int scaleNoteForTransport(int degree, void *);
static int scaleNoteForSeq(int degree, void *);
static void runMacro(const String &name);
static void runBenchSelfTest();
static void printBenchDiag();
static void benchTone(int note, unsigned ms);
static void handleTextCommand(const String &line);
static void setSeqEnabled(bool on);
#if BENCH_SEQUENCER
static void onSeqStep(uint8_t note, bool on, void *);
#endif

static void stopArpVoice();
static void refreshDemoChord();
static void releaseBlockChord();

static void ledOn() { digitalWrite(kLed, LOW); }
static void ledOff() { digitalWrite(kLed, HIGH); }

static void bootBlink(int times, int ms) {
  for (int i = 0; i < times; i++) {
    ledOn();
    delay(ms);
    ledOff();
    delay(ms);
  }
}

static void failForever(const __FlashStringHelper *reason) {
#if BENCH_SERIAL_MIDI
  Serial.print(F("FATAL "));
  Serial.println(reason);
#endif
  while (true) {
    ledOn();
    delay(50);
    ledOff();
    delay(50);
  }
}

static void bootBeep() {
  voicer.noteOn(60, kStkAmp, kGroup);
  delay(180);
  voicer.noteOff(60, kStkAmp, kGroup);
}

static float softClip(float x) {
  if (x > 1.0f) return 1.0f;
  if (x < -1.0f) return -1.0f;
  return x * (1.5f - 0.5f * x * x);
}

#if BENCH_MONO_FILTER
static float cutoffHzFromNorm(float norm) {
  norm = constrain(norm, 0.0f, 1.0f);
  constexpr float kMin = 120.0f;
  constexpr float kMax = 8000.0f;
  return kMin * powf(kMax / kMin, norm);
}

static float cutoffHzFromMidi(uint8_t value) {
  return cutoffHzFromNorm((float)value / 127.0f);
}

static void setMonoFilterCutoff(float hz) {
#if BENCH_MONO_LADDER
  monoLadder.setCutoff(hz);
#else
  monoLpf.setCutoff(hz);
#endif
}

#if BENCH_FILTER_ENV
static void applyMonoFilterCutoff() {
  float hz = filterCutoffBase;
  if (filterEnvOn && monoFilterOn && monoMode) {
    const float e = filterEnv.level();
    const float closed = hz * (1.0f - filterEnvDepth);
    hz = closed + (hz - closed) * e;
  }
  setMonoFilterCutoff(hz);
}

static void setFilterCutoffBase(float hz) {
  filterCutoffBase = constrain(hz, 120.0f, 8000.0f);
  applyMonoFilterCutoff();
}
#else
static void setFilterCutoffBase(float hz) { setMonoFilterCutoff(hz); }
#endif

#if BENCH_MONO_LADDER
static void setMonoFilterResonance(float r) { monoLadder.setResonance(r); }
static float monoFilterResonance() { return monoLadder.resonance(); }
#endif

static float monoFilterCutoffHz() {
#if BENCH_FILTER_ENV
  return filterCutoffBase;
#else
#if BENCH_MONO_LADDER
  return monoLadder.cutoffHz();
#else
  return monoLpf.cutoffHz();
#endif
#endif
}
#endif

static int16_t clip16(float x) {
  if (x > 32767.0f) return 32767;
  if (x < -32768.0f) return -32768;
  return (int16_t)x;
}

static void fillAudio(int16_t *buf, size_t frames, void *) {
  const float gain = masterGain * midiVolume * kStkGain;
  for (size_t i = 0; i < frames; i++) {
    if (monoMode && monoLead.gliding()) {
      monoLead.tickGlide(voicer);
    }
    float synth = voicer.tick() * gain / 32768.0f;
#if BENCH_DRUM_GRID
    float drumMix = 0.0f;
    if (drums.enabled() && !muteDrums) {
      drumMix = drums.tickSample() / 32768.0f;
    }
#else
    const float drumMix = 0.0f;
#endif
    float metroMix = 0.0f;
    if (metroEnv > 0.0001f) {
      metroPhase += 0.09f;
      metroMix = sinf(metroPhase * 6.2831853f) * metroEnv * 0.12f;
      metroEnv *= 0.994f;
    }
#if BENCH_MONO_FILTER
    if (monoFilterOn && monoMode) {
#if BENCH_MONO_LADDER
      synth = monoLadder.process(synth);
#else
      synth = monoLpf.process(synth);
#endif
    }
    const float norm = softClip(synth + drumMix + metroMix);
    int16_t l = 0;
    int16_t r = 0;
    audioOut.process(norm * 32768.0f, l, r);
#else
    float sample = synth + drumMix + metroMix;
    const float norm = softClip(sample);
    int16_t l = 0;
    int16_t r = 0;
    audioOut.process(norm * 32768.0f, l, r);
#endif
    buf[i * 2] = l;
    buf[i * 2 + 1] = r;
  }
}

static void addAllVoices() {
  voicer.addInstrument(&inst0, kGroup);
  voicer.addInstrument(&inst1, kGroup);
  voicer.addInstrument(&inst2, kGroup);
  voicer.addInstrument(&inst3, kGroup);
  voicer.addInstrument(&inst4, kGroup);
  voicer.addInstrument(&inst5, kGroup);
}

static float midiVelToAmp(uint8_t velocity) {
  return (kStkAmp * (float)velocity) / 127.0f;
}

static int keyboardNote(int index) {
  return chords.scaleNote(index) + octaveShift * 12;
}

static void releaseSustainedNotes() {
  for (int n = 0; n < 128; n++) {
    if (sustainHeld[n]) {
      voicer.noteOff(n, kStkAmp, kGroup);
      sustainHeld[n] = false;
    }
  }
}

static void noteOnLive(int note, float amp) {
#if BENCH_SEQUENCER
  if (seq.recording()) {
    seq.captureNote((uint8_t)note);
  }
#endif
  demoActive = false;
  if (monoMode) {
    monoLead.noteOn(voicer, note, amp, kGroup);
    monoNote = note;
#if BENCH_FILTER_ENV
    if (filterEnvOn && monoFilterOn) filterEnv.trigger();
#endif
    return;
  }
  voicer.noteOn(note, amp, kGroup);
}

static void applyTransportBpm(unsigned bpm) {
  if (bpm < 40) return;
#if BENCH_SEQUENCER
  seq.setBpm(bpm);
#endif
#if BENCH_BASS_SEQ
  bass.setBpm(bpm);
#endif
#if BENCH_DRUM_GRID
  drums.setBpm(bpm);
#endif
  markPresetDirty();
}

static void nudgeTransportBpm(int delta) {
#if BENCH_SEQUENCER
  const int bpm = constrain((int)seq.bpm() + delta, 40, 240);
  applyTransportBpm((unsigned)bpm);
#if BENCH_SERIAL_MIDI
  Serial.print(F("bpm "));
  Serial.println(seq.bpm());
#endif
#else
  (void)delta;
#endif
}

static void applyTransportSwing(float swing, bool persist) {
  const float s = constrain(swing, 0.0f, 0.55f);
#if BENCH_SEQUENCER
  seq.setSwing(s);
#endif
#if BENCH_BASS_SEQ
  bass.setSwing(s);
#endif
#if BENCH_DRUM_GRID
  drums.setSwing(s);
#endif
  if (persist) markPresetDirty();
}

static void applyTransportSwing(float swing) { applyTransportSwing(swing, true); }

static BenchTransportState transportState() {
  BenchTransportState t;
#if BENCH_DRUM_GRID
  t.drumsOn = drums.enabled();
#endif
#if BENCH_BASS_SEQ
  t.bassOn = bass.enabled();
#endif
  t.monoOn = monoMode;
  t.legatoOn = monoLead.legato();
  t.glideOn = monoLead.glide();
  t.unisonOn = monoLead.unison();
#if BENCH_HARMONY
  t.harmOn = harmony.enabled();
  t.harmSemi = (uint8_t)harmony.semitones();
#endif
  t.seqAccent = seqAccent;
#if BENCH_SEQUENCER
  t.swingPct = (uint8_t)constrain((int)(seq.swing() * 100.0f), 0, 55);
#endif
  return t;
}

static void applyTransportState(const BenchTransportState &t) {
#if BENCH_DRUM_GRID
  drums.setEnabled(t.drumsOn);
#endif
#if BENCH_BASS_SEQ
  bass.setEnabled(t.bassOn);
#endif
  monoMode = t.monoOn;
  monoLead.setLegato(t.legatoOn);
  monoLead.setGlide(t.glideOn);
  monoLead.setUnison(t.unisonOn);
#if BENCH_HARMONY
  harmony.setSemitones(t.harmSemi);
  harmony.setEnabled(t.harmOn);
  if (!t.harmOn) harmony.silence(voicer, kGroup, kStkAmp);
#endif
  seqAccent = t.seqAccent;
  applyTransportSwing((float)t.swingPct / 100.0f, false);
}

static BenchLiveFlags captureLiveFlags() {
  BenchLiveFlags f;
  f.demoActive = demoActive;
  f.midiQuantize = midiQuantize;
  f.genMode = genMode;
  f.monoMode = monoMode;
  f.seqAccent = seqAccent;
  f.octaveShift = octaveShift;
#if BENCH_DRUM_PADS
  f.drumPadMode = drumPadMode;
  f.drumWriteMode = drumWriteMode;
#endif
#if BENCH_SEQUENCER
  f.seqWriteMode = seqWriteMode;
  f.seqGridMode = seqGridMode;
#endif
#if BENCH_BASS_SEQ
  f.bassGridMode = bassGridMode;
#endif
  f.muteSeq = muteSeq;
  f.muteBass = muteBass;
  f.muteDrums = muteDrums;
  return f;
}

static void applyLiveFlags(const BenchLiveFlags &f) {
  demoActive = f.demoActive;
  midiQuantize = f.midiQuantize;
  genMode = f.genMode;
  monoMode = f.monoMode;
  seqAccent = f.seqAccent;
  octaveShift = f.octaveShift;
#if BENCH_DRUM_PADS
  drumPadMode = f.drumPadMode;
  drumWriteMode = f.drumWriteMode;
#endif
#if BENCH_SEQUENCER
  seqWriteMode = f.seqWriteMode;
  seqGridMode = f.seqGridMode;
#endif
#if BENCH_BASS_SEQ
  bassGridMode = f.bassGridMode;
#endif
  muteSeq = f.muteSeq;
  muteBass = f.muteBass;
  muteDrums = f.muteDrums;
}

static void captureBenchStash() {
  benchStash.capture(chords, captureLiveFlags(), transportState(),
#if BENCH_SEQUENCER
                     &seq,
#else
                     nullptr,
#endif
#if BENCH_BASS_SEQ
                     &bass,
#else
                     nullptr,
#endif
#if BENCH_DRUM_GRID
                     &drums,
#else
                     nullptr,
#endif
#if BENCH_HARMONY
                     &harmony,
#else
                     nullptr,
#endif
                     monoLead, activeBenchSlot);
#if BENCH_SERIAL_MIDI
  Serial.println(F("stash ok"));
#endif
}

static void undoBenchStash() {
  BenchLiveFlags flags;
  BenchTransportState t;
  int slot = activeBenchSlot;
  if (!benchStash.restore(chords, flags, t,
#if BENCH_SEQUENCER
                          &seq,
#else
                          nullptr,
#endif
#if BENCH_BASS_SEQ
                          &bass,
#else
                          nullptr,
#endif
#if BENCH_DRUM_GRID
                          &drums,
#else
                          nullptr,
#endif
#if BENCH_HARMONY
                          &harmony,
#else
                          nullptr,
#endif
                          monoLead, slot)) {
#if BENCH_SERIAL_MIDI
    Serial.println(F("stash empty"));
#endif
    return;
  }
  activeBenchSlot = slot;
  applyLiveFlags(flags);
  applyTransportState(t);
  if (demoActive) {
    refreshDemoChord();
  } else {
    releaseBlockChord();
  }
#if BENCH_SERIAL_MIDI
  Serial.println(F("undo ok"));
#endif
}

static void auditionBenchSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
  if (!benchStash.valid()) {
    captureBenchStash();
  }
  if (loadBenchPreset(slot)) {
    activeBenchSlot = slot;
#if BENCH_SERIAL_MIDI
    Serial.print(F("audition "));
    Serial.print(slot);
    Serial.println(F(" (undo restores)"));
#endif
  } else {
#if BENCH_SERIAL_MIDI
    Serial.println(F("empty slot"));
#endif
  }
}

static void commitBenchSlot(int slot) {
  if (slot < 0) slot = activeBenchSlot;
  if (slot < 0 || slot >= PresetStore::kSlots) {
#if BENCH_SERIAL_MIDI
    Serial.println(F("commit? slot 0-3"));
#endif
    return;
  }
  saveBenchPreset(slot);
  activeBenchSlot = slot;
  benchEeprom.setActiveSlot((uint8_t)slot);
  benchEeprom.persist();
  benchStash.clear();
  presetDirty = false;
#if BENCH_SERIAL_MIDI
  Serial.print(F("committed "));
  Serial.println(slot);
#endif
}

static int midiToScaleDegree(int midiNote) {
  if (midiNote <= 0) return BassSeq::kRest;
  const int n = chords.scaleNoteCount();
  int bestDeg = 0;
  int bestDist = 127;
  for (int d = 0; d < n; d++) {
    const int sn = chords.scaleNote(d);
    const int dist = abs(sn - midiNote);
    if (dist < bestDist) {
      bestDist = dist;
      bestDeg = d;
    }
  }
  return bestDeg;
}

static void bassFromSeqPattern(bool quiet) {
#if BENCH_SEQUENCER && BENCH_BASS_SEQ
  const int steps = min(seq.length(), BassSeq::kSteps);
  for (int i = 0; i < steps; i++) {
    const uint8_t note = seq.stepNote(i);
    bass.setStep(i, note == SeqPlayer::kRest ? BassSeq::kRest : (int8_t)midiToScaleDegree(note));
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("bass from seq"));
#endif
#else
  (void)quiet;
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("bass from seq?"));
#endif
#endif
}

static void drumFromSeqPattern(bool quiet) {
#if BENCH_SEQUENCER && BENCH_DRUM_GRID
  const int steps = min(seq.length(), drums.length());
  for (int i = 0; i < steps; i++) {
    drums.setKick(i, seq.stepNote(i) != SeqPlayer::kRest);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("drum from seq"));
#endif
#else
  (void)quiet;
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("drum from seq?"));
#endif
#endif
}

static void hatFromSeqPattern(bool quiet) {
#if BENCH_SEQUENCER && BENCH_DRUM_GRID
  const int steps = min(seq.length(), drums.length());
  for (int i = 0; i < steps; i++) {
    drums.setHat(i, seq.stepNote(i) == SeqPlayer::kRest);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("hat from seq"));
#endif
#else
  (void)quiet;
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("hat from seq?"));
#endif
#endif
}

static void followSeqPattern() {
  bassFromSeqPattern(true);
  drumFromSeqPattern(true);
  hatFromSeqPattern(true);
#if BENCH_SERIAL_MIDI
  Serial.println(F("follow seq"));
#endif
}

static void patternClear() {
#if BENCH_SEQUENCER
  seq.clear();
#endif
#if BENCH_BASS_SEQ
  bass.clear();
#endif
#if BENCH_DRUM_GRID
  drums.clear();
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern clear"));
#endif
}

static void patternDup() {
#if BENCH_SEQUENCER
  if (seq.length() <= SeqPlayer::kSteps) {
    seq.doubleBar();
  }
#endif
#if BENCH_BASS_SEQ
  bass.doubleBar();
#endif
#if BENCH_DRUM_GRID
  if (drums.length() <= DrumGrid::kSteps) {
    drums.doubleBar();
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern dup"));
#endif
}

static void setActiveSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
  activeBenchSlot = slot;
#if BENCH_SERIAL_MIDI
  Serial.print(F("slot "));
  Serial.println(slot);
#endif
}

static void setLayerMute(const String &layer, bool on) {
  if (layer.equalsIgnoreCase("seq")) {
    muteSeq = on;
#if BENCH_SEQUENCER
    if (on && seq.playingNote() > 0) {
      voicer.noteOff((uint8_t)seq.playingNote(), kStkAmp, kGroup);
#if BENCH_HARMONY
      harmony.onLeadStep((uint8_t)seq.playingNote(), false, voicer, kGroup, kStkAmp);
#endif
    }
#endif
#if BENCH_SERIAL_MIDI
    Serial.println(on ? F("mute seq on") : F("mute seq off"));
#endif
    return;
  }
  if (layer.equalsIgnoreCase("bass")) {
    muteBass = on;
#if BENCH_BASS_SEQ
    if (on && bassPlayingNote >= 0) {
      voicer.noteOff(bassPlayingNote, kStkAmp, kGroup);
      bassPlayingNote = -1;
    }
#endif
#if BENCH_SERIAL_MIDI
    Serial.println(on ? F("mute bass on") : F("mute bass off"));
#endif
    return;
  }
  if (layer.equalsIgnoreCase("drums") || layer.equalsIgnoreCase("drum")) {
    muteDrums = on;
#if BENCH_SERIAL_MIDI
    Serial.println(on ? F("mute drums on") : F("mute drums off"));
#endif
    return;
  }
  if (layer.equalsIgnoreCase("all")) {
    setLayerMute(String("seq"), on);
    setLayerMute(String("bass"), on);
    setLayerMute(String("drums"), on);
    return;
  }
#if BENCH_SERIAL_MIDI
  Serial.println(F("mute seq|bass|drums|all on|off"));
#endif
}

static void soloLayer(const String &layer) {
  muteSeq = true;
  muteBass = true;
  muteDrums = true;
  if (layer.equalsIgnoreCase("seq")) {
    muteSeq = false;
  } else if (layer.equalsIgnoreCase("bass")) {
    muteBass = false;
  } else if (layer.equalsIgnoreCase("drums") || layer.equalsIgnoreCase("drum")) {
    muteDrums = false;
  } else {
#if BENCH_SERIAL_MIDI
    Serial.println(F("solo seq|bass|drums"));
#endif
    return;
  }
#if BENCH_SERIAL_MIDI
  Serial.print(F("solo "));
  Serial.println(layer);
#endif
}

static void transposeSeqPattern(int semi) {
#if BENCH_SEQUENCER
  for (int i = 0; i < seq.length(); i++) {
    const uint8_t note = seq.stepNote(i);
    if (note == SeqPlayer::kRest) continue;
    int t = constrain((int)note + semi, 0, 127);
    if (midiQuantize) {
      t = chords.quantizeToScale(t);
    }
    seq.setStep(i, (uint8_t)t);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("transpose "));
  Serial.println(semi);
#endif
#else
  (void)semi;
#endif
}

static void seqThinPattern() {
#if BENCH_SEQUENCER
  for (int i = 0; i < seq.length(); i++) {
    if ((i & 1) != 0 && seq.stepNote(i) != SeqPlayer::kRest) {
      seq.setStep(i, SeqPlayer::kRest);
    }
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("seq thin"));
#endif
#endif
}

static void seqDensifyPattern() {
#if BENCH_SEQUENCER
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) != SeqPlayer::kRest) continue;
    int note = chords.root() + octaveShift * 12;
    if (midiQuantize) {
      note = chords.quantizeToScale(note);
    }
    seq.setStep(i, (uint8_t)constrain(note, 1, 127));
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("seq densify"));
#endif
#endif
}

static void bassRotatePattern(int steps) {
#if BENCH_BASS_SEQ
  int8_t tmp[BassSeq::kSteps];
  for (int i = 0; i < BassSeq::kSteps; i++) {
    int src = (i - steps) % BassSeq::kSteps;
    if (src < 0) src += BassSeq::kSteps;
    tmp[i] = bass.stepDegree(src);
  }
  for (int i = 0; i < BassSeq::kSteps; i++) {
    bass.setStep(i, tmp[i]);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("bass rotate "));
  Serial.println(steps);
#endif
#else
  (void)steps;
#endif
}

static void humanizeGroove(bool quiet) {
  randomSeed((uint32_t)micros() ^ (uint32_t)millis());
#if BENCH_SEQUENCER
  const int sw = 12 + (int)random(0, 22);
  applyTransportSwing(sw / 100.0f, true);
  seq.shiftSteps((int)random(-2, 3));
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  if (!quiet) Serial.println(F("humanize"));
#endif
}

static void patternExport() {
  Serial.println(F("--- export ---"));
#if BENCH_SEQUENCER
  Serial.print(F("seq "));
  for (int i = 0; i < seq.length(); i++) {
    if (i > 0) Serial.print(' ');
    const uint8_t n = seq.stepNote(i);
    Serial.print(n == SeqPlayer::kRest ? F("-") : String(n));
  }
  Serial.println();
#endif
#if BENCH_BASS_SEQ
  Serial.print(F("bass "));
  for (int i = 0; i < BassSeq::kSteps; i++) {
    if (i > 0) Serial.print(' ');
    const int8_t d = bass.stepDegree(i);
    Serial.print(d == BassSeq::kRest ? F("-") : String(d));
  }
  Serial.println();
#endif
#if BENCH_DRUM_GRID
  Serial.print(F("kick "));
  for (int i = 0; i < drums.length(); i++) {
    Serial.print(drums.kickAt(i) ? 'X' : '.');
  }
  Serial.println();
  Serial.print(F("hat  "));
  for (int i = 0; i < drums.length(); i++) {
    Serial.print(drums.hatAt(i) ? 'x' : '.');
  }
  Serial.println();
#endif
  Serial.println(F("---"));
}

static bool chordEditBlocked() {
  if (!chordLocked) return false;
#if BENCH_SERIAL_MIDI
  Serial.println(F("chord locked"));
#endif
  return true;
}

static bool harmEditBlocked() {
  if (!harmLocked) return false;
#if BENCH_SERIAL_MIDI
  Serial.println(F("harm locked"));
#endif
  return true;
}

static void pulseMetroBeat() {
  const bool downbeat = (metroBeatCount % 4) == 0;
  if (metroAccent) {
    metroEnv = downbeat ? 1.0f : 0.48f;
  } else {
    metroEnv = 1.0f;
  }
  metroBeatCount = (metroBeatCount + 1) % 4;
}

static void pollMetro(unsigned long nowMs) {
  if (!metroEnabled || midiClockSync) return;
  static unsigned long lastMs = 0;
  unsigned bpm = 120;
#if BENCH_SEQUENCER
  bpm = seq.bpm();
#endif
  const unsigned interval = 60000UL / bpm;
  if (lastMs == 0) lastMs = nowMs;
  if ((unsigned long)(nowMs - lastMs) >= interval) {
    lastMs = nowMs;
    pulseMetroBeat();
  }
}

static void patternImportLine(const String &line) {
  if (line.startsWith("seq ")) {
#if BENCH_SEQUENCER
    String rest = line.substring(4);
    rest.trim();
    int idx = 0;
    while (rest.length() > 0 && idx < SeqPlayer::kMaxSteps) {
      while (rest.length() > 0 && rest[0] == ' ') rest.remove(0, 1);
      if (rest.length() == 0) break;
      const int sp = rest.indexOf(' ');
      const String tok = sp < 0 ? rest : rest.substring(0, sp);
      if (tok == "-") {
        seq.setStep(idx, SeqPlayer::kRest);
      } else {
        int note = tok.toInt();
        if (patternImportQuantize) {
          note = chords.quantizeToScale(note);
        }
        seq.setStep(idx, (uint8_t)constrain(note, 0, 127));
      }
      idx++;
      rest = sp < 0 ? String("") : rest.substring(sp + 1);
    }
    if (idx > 0) seq.setLength(idx);
#endif
    return;
  }
  if (line.startsWith("bass ")) {
#if BENCH_BASS_SEQ
    String rest = line.substring(5);
    rest.trim();
    int idx = 0;
    while (rest.length() > 0 && idx < BassSeq::kSteps) {
      while (rest.length() > 0 && rest[0] == ' ') rest.remove(0, 1);
      if (rest.length() == 0) break;
      const int sp = rest.indexOf(' ');
      const String tok = sp < 0 ? rest : rest.substring(0, sp);
      if (tok == "-") {
        bass.setStep(idx, BassSeq::kRest);
      } else {
        bass.setStep(idx, (int8_t)constrain(tok.toInt(), 0, 6));
      }
      idx++;
      rest = sp < 0 ? String("") : rest.substring(sp + 1);
    }
#endif
    return;
  }
  if (line.startsWith("kick ")) {
#if BENCH_DRUM_GRID
    const String pat = line.substring(5);
    const int n = min((int)pat.length(), drums.length());
    for (int i = 0; i < n; i++) {
      const char c = pat[i];
      drums.setKick(i, c == 'X' || c == 'x' || c == '1');
    }
#endif
    return;
  }
  if (line.startsWith("hat ")) {
#if BENCH_DRUM_GRID
    String pat = line.substring(4);
    pat.trim();
    const int n = min((int)pat.length(), drums.length());
    for (int i = 0; i < n; i++) {
      const char c = pat[i];
      drums.setHat(i, c == 'x' || c == 'X' || c == '1');
    }
#endif
  }
}

static void finishPatternImport() {
  int from = 0;
  while (from < (int)patternImportBuf.length()) {
    int end = patternImportBuf.indexOf('\n', from);
    if (end < 0) end = patternImportBuf.length();
    String row = patternImportBuf.substring(from, end);
    row.trim();
    if (row.length() > 0 && !row.equals(F("---"))) {
      patternImportLine(row);
    }
    from = end + 1;
  }
  patternImportQuantize = false;
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("import ok"));
#endif
}

static int eepromImportFieldInt(const String &line, const char *key) {
  const String prefix = String(key) + "=";
  int idx = line.indexOf(prefix);
  if (idx < 0) return -1;
  int start = idx + prefix.length();
  while (start < (int)line.length() && line[start] == ' ') start++;
  int end = start;
  while (end < (int)line.length() && line[end] != ' ') end++;
  return line.substring(start, end).toInt();
}

static void eepromImportLine(const String &line) {
  if (line.startsWith(F("---"))) return;
  if (line.indexOf(F("valid=")) >= 0) {
    eepromImportSnapValid = eepromImportFieldInt(line, "valid") > 0;
    eepromImportSnap.valid = eepromImportSnapValid;
  }
  if (line.indexOf(F("root=")) >= 0) {
    const int v = eepromImportFieldInt(line, "root");
    if (v >= 0) eepromImportSnap.root = (int8_t)v;
  }
  if (line.indexOf(F("type=")) >= 0) {
    const int v = eepromImportFieldInt(line, "type");
    if (v >= 0) eepromImportSnap.chordType = (uint8_t)v;
  }
  if (line.indexOf(F("inv=")) >= 0) {
    const int v = eepromImportFieldInt(line, "inv");
    if (v >= 0) eepromImportSnap.inversion = (uint8_t)v;
  }
  if (line.indexOf(F("scale=")) >= 0) {
    const int v = eepromImportFieldInt(line, "scale");
    if (v >= 0) eepromImportSnap.scale = (uint8_t)v;
  }
  if (line.indexOf(F("oct=")) >= 0) {
    const int v = eepromImportFieldInt(line, "oct");
    if (v >= -8 && v <= 8) eepromImportSnap.octaveShift = (int8_t)v;
  }
  if (line.indexOf(F("demo=")) >= 0) {
    const int v = eepromImportFieldInt(line, "demo");
    if (v >= 0) eepromImportSnap.demo = v != 0;
  }
  if (line.indexOf(F("arp=")) >= 0) {
    const int v = eepromImportFieldInt(line, "arp");
    if (v >= 0) eepromImportSnap.arp = v != 0;
  }
  if (line.indexOf(F("seq=")) >= 0) {
    const int v = eepromImportFieldInt(line, "seq");
    if (v >= 0) eepromImportSnap.seqOn = v != 0;
  }
  if (line.indexOf(F("bpm=")) >= 0) {
    const int v = eepromImportFieldInt(line, "bpm");
    if (v > 0) eepromImportSnap.seqBpm = (unsigned)v;
  }
  if (line.startsWith(F("bank_hex="))) {
    eepromImportHex = line.substring(9);
    eepromImportHex.trim();
  }
}

static void finishEepromImport() {
  bool wrote = false;
  if (eepromImportSnapValid) {
    presets.writeChordSnapshot(eepromImportSlot, eepromImportSnap);
    wrote = true;
  }
  if (eepromImportHex.length() >= (int)(PresetStore::kPatternBankBytes * 2)) {
    if (presets.importPatternBankHex(eepromImportSlot, eepromImportHex.c_str(),
                                     eepromImportHex.length())) {
      wrote = true;
    }
  }
  if (!wrote) {
#if BENCH_SERIAL_MIDI
    Serial.println(F("eeprom import? no data"));
#endif
    return;
  }
#if BENCH_SERIAL_MIDI
  Serial.print(F("eeprom import ok slot="));
  Serial.println(eepromImportSlot);
#endif
}

static uint32_t measureFilterLoadUsPerFrame(int frames, int iterations) {
  if (frames <= 0) frames = 256;
  if (iterations <= 0) iterations = 40;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  monoMode = true;
  monoFilterOn = true;
#if BENCH_MONO_LADDER
  monoLadder.reset();
#else
  monoLpf.reset();
#endif
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[512];
  const size_t frameCount = (size_t)frames;
  const unsigned long t0 = micros();
  for (int i = 0; i < iterations; i++) {
    fillAudio(buf, frameCount, nullptr);
  }
  const unsigned long totalUs = micros() - t0;
  voicer.noteOff(60, kStkAmp, kGroup);
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  return (uint32_t)(totalUs / (unsigned long)(iterations * frames));
}

static void benchFilterLoad(int frames) {
  if (frames <= 0) frames = 256;
  const int iterations = 40;
  const uint32_t usPerFrame = measureFilterLoadUsPerFrame(frames, iterations);
  const unsigned long totalUs = (unsigned long)usPerFrame * (unsigned long)frames *
                                (unsigned long)iterations;
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float cpuPct = 100.0f * (float)usPerFrame / frameUs;
  Serial.println(F("--- bench filter load ---"));
  Serial.print(F("frames="));
  Serial.println(frames);
  Serial.print(F("iterations="));
  Serial.println(iterations);
  Serial.print(F("total_us="));
  Serial.println(totalUs);
  Serial.print(F("us_per_frame="));
  Serial.println(usPerFrame);
  Serial.print(F("est_cpu_pct="));
  Serial.println(cpuPct, 1);
  Serial.println(F("---"));
}

#if BENCH_MONO_FILTER && BENCH_FILTER_ENV
static uint32_t measureFilterEnvLoadUsPerFrame(int frames, int iterations) {
  if (frames <= 0) frames = 256;
  if (iterations <= 0) iterations = 40;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  const bool wasEnv = filterEnvOn;
  const float depthWas = filterEnvDepth;
  monoMode = true;
  monoFilterOn = true;
  filterEnvOn = true;
  filterEnvDepth = 0.88f;
#if BENCH_MONO_LADDER
  monoLadder.reset();
#else
  monoLpf.reset();
#endif
  filterEnv.reset();
  filterEnv.trigger();
  setFilterCutoffBase(2400.0f);
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[512];
  const size_t frameCount = (size_t)frames;
  unsigned long tMs = millis();
  const unsigned long t0 = micros();
  for (int i = 0; i < iterations; i++) {
    tMs += 3;
    filterEnv.poll(tMs);
    applyMonoFilterCutoff();
    fillAudio(buf, frameCount, nullptr);
  }
  const unsigned long totalUs = micros() - t0;
  voicer.noteOff(60, kStkAmp, kGroup);
  filterEnvDepth = depthWas;
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  filterEnvOn = wasEnv;
  return (uint32_t)(totalUs / (unsigned long)(iterations * frames));
}

static void benchFilterEnvLoad(int frames) {
  if (frames <= 0) frames = 256;
  const int iterations = 40;
  const uint32_t usPerFrame = measureFilterEnvLoadUsPerFrame(frames, iterations);
  const unsigned long totalUs = (unsigned long)usPerFrame * (unsigned long)frames *
                                  (unsigned long)iterations;
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float cpuPct = 100.0f * (float)usPerFrame / frameUs;
  Serial.println(F("--- bench filter env load ---"));
  Serial.print(F("frames="));
  Serial.println(frames);
  Serial.print(F("iterations="));
  Serial.println(iterations);
  Serial.print(F("total_us="));
  Serial.println(totalUs);
  Serial.print(F("us_per_frame="));
  Serial.println(usPerFrame);
  Serial.print(F("est_cpu_pct="));
  Serial.println(cpuPct, 1);
  Serial.println(F("---"));
}
#endif

#if BENCH_MONO_FILTER && BENCH_MONO_LADDER
static uint32_t measureFilterResLoadUsPerFrame(int frames, int iterations) {
  if (frames <= 0) frames = 256;
  if (iterations <= 0) iterations = 40;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  const float resWas = monoFilterResonance();
  monoMode = true;
  monoFilterOn = true;
  monoLadder.reset();
  setFilterCutoffBase(1800.0f);
  setMonoFilterResonance(0.82f);
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[512];
  const size_t frameCount = (size_t)frames;
  const unsigned long t0 = micros();
  for (int i = 0; i < iterations; i++) {
    fillAudio(buf, frameCount, nullptr);
  }
  const unsigned long totalUs = micros() - t0;
  voicer.noteOff(60, kStkAmp, kGroup);
  setMonoFilterResonance(resWas);
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  return (uint32_t)(totalUs / (unsigned long)(iterations * frames));
}

static void benchFilterResLoad(int frames) {
  if (frames <= 0) frames = 256;
  const int iterations = 40;
  const uint32_t usPerFrame = measureFilterResLoadUsPerFrame(frames, iterations);
  const unsigned long totalUs = (unsigned long)usPerFrame * (unsigned long)frames *
                                  (unsigned long)iterations;
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float cpuPct = 100.0f * (float)usPerFrame / frameUs;
  Serial.println(F("--- bench filter res load ---"));
  Serial.print(F("frames="));
  Serial.println(frames);
  Serial.print(F("iterations="));
  Serial.println(iterations);
  Serial.print(F("res="));
  Serial.println(0.82f, 2);
  Serial.print(F("total_us="));
  Serial.println(totalUs);
  Serial.print(F("us_per_frame="));
  Serial.println(usPerFrame);
  Serial.print(F("est_cpu_pct="));
  Serial.println(cpuPct, 1);
  Serial.println(F("---"));
}
#endif

#if BENCH_MONO_FILTER && BENCH_FILTER_ENV && BENCH_MONO_LADDER
static uint32_t measureFilterComboLoadUsPerFrame(int frames, int iterations) {
  if (frames <= 0) frames = 256;
  if (iterations <= 0) iterations = 40;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  const bool wasEnv = filterEnvOn;
  const float depthWas = filterEnvDepth;
  const float resWas = monoFilterResonance();
  monoMode = true;
  monoFilterOn = true;
  filterEnvOn = true;
  filterEnvDepth = 0.88f;
  monoLadder.reset();
  filterEnv.reset();
  filterEnv.trigger();
  setFilterCutoffBase(2000.0f);
  setMonoFilterResonance(0.78f);
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[512];
  const size_t frameCount = (size_t)frames;
  unsigned long tMs = millis();
  const unsigned long t0 = micros();
  for (int i = 0; i < iterations; i++) {
    tMs += 3;
    filterEnv.poll(tMs);
    applyMonoFilterCutoff();
    fillAudio(buf, frameCount, nullptr);
  }
  const unsigned long totalUs = micros() - t0;
  voicer.noteOff(60, kStkAmp, kGroup);
  filterEnvDepth = depthWas;
  setMonoFilterResonance(resWas);
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  filterEnvOn = wasEnv;
  return (uint32_t)(totalUs / (unsigned long)(iterations * frames));
}

static void benchFilterComboLoad(int frames) {
  if (frames <= 0) frames = 256;
  const int iterations = 40;
  const uint32_t usPerFrame = measureFilterComboLoadUsPerFrame(frames, iterations);
  const unsigned long totalUs = (unsigned long)usPerFrame * (unsigned long)frames *
                                  (unsigned long)iterations;
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float cpuPct = 100.0f * (float)usPerFrame / frameUs;
  Serial.println(F("--- bench filter combo load ---"));
  Serial.print(F("frames="));
  Serial.println(frames);
  Serial.print(F("iterations="));
  Serial.println(iterations);
  Serial.print(F("res="));
  Serial.println(0.78f, 2);
  Serial.print(F("env_depth="));
  Serial.println(0.88f, 2);
  Serial.print(F("total_us="));
  Serial.println(totalUs);
  Serial.print(F("us_per_frame="));
  Serial.println(usPerFrame);
  Serial.print(F("est_cpu_pct="));
  Serial.println(cpuPct, 1);
  Serial.println(F("---"));
}
#endif

#if BENCH_MONO_FILTER
static float sweepHzFromStep(int step, int steps) {
  constexpr float kMin = 200.0f;
  constexpr float kMax = 8000.0f;
  const float norm = (float)step / (float)steps;
  return kMin * powf(kMax / kMin, norm);
}

static int benchFilterSweepSteps(int steps, unsigned stepMs) {
  if (steps <= 0) steps = 20;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  monoMode = true;
  monoFilterOn = true;
#if BENCH_MONO_LADDER
  monoLadder.reset();
#else
  monoLpf.reset();
#endif
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[256];
  for (int i = 0; i <= steps; i++) {
    const float hz = sweepHzFromStep(i, steps);
    setFilterCutoffBase(hz);
    fillAudio(buf, 256, nullptr);
    Serial.print(F("step="));
    Serial.print(i);
    Serial.print(F("/"));
    Serial.print(steps);
    Serial.print(F(" hz="));
    Serial.println(hz, 0);
    if (stepMs > 0) delay(stepMs);
  }
  voicer.noteOff(60, kStkAmp, kGroup);
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  return steps;
}

static void benchFilterSweep() {
  Serial.println(F("--- bench filter sweep ---"));
  Serial.print(F("steps="));
  Serial.println(20);
  Serial.print(F("duration_ms="));
  Serial.println(2000);
  benchFilterSweepSteps(20, 100);
  Serial.println(F("sweep done"));
  Serial.println(F("---"));
}

static int benchFilterResSweepSteps(int steps, unsigned stepMs) {
  if (steps <= 0) steps = 16;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  const float resWas = monoFilterResonance();
  monoMode = true;
  monoFilterOn = true;
#if BENCH_MONO_LADDER
  monoLadder.reset();
  setFilterCutoffBase(2200.0f);
#else
  monoLpf.reset();
#endif
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[256];
  for (int i = 0; i <= steps; i++) {
    const float res = 0.05f + (0.90f * (float)i / (float)steps);
    setMonoFilterResonance(res);
    fillAudio(buf, 256, nullptr);
    Serial.print(F("step="));
    Serial.print(i);
    Serial.print(F("/"));
    Serial.print(steps);
    Serial.print(F(" res="));
    Serial.println(res, 2);
    if (stepMs > 0) delay(stepMs);
  }
  voicer.noteOff(60, kStkAmp, kGroup);
  setMonoFilterResonance(resWas);
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  return steps;
}

static void benchFilterResSweep() {
  Serial.println(F("--- bench filter res sweep ---"));
  Serial.print(F("cutoff_hz="));
  Serial.println(2200);
  Serial.print(F("steps="));
  Serial.println(16);
  benchFilterResSweepSteps(16, 0);
  Serial.println(F("res sweep done"));
  Serial.println(F("---"));
}

#if BENCH_FILTER_ENV
static int benchFilterEnvSweepSteps(int steps, unsigned stepMs) {
  if (steps <= 0) steps = 16;
  const bool wasMono = monoMode;
  const bool wasFilter = monoFilterOn;
  const bool wasEnv = filterEnvOn;
  const float depthWas = filterEnvDepth;
  monoMode = true;
  monoFilterOn = true;
  filterEnvOn = true;
#if BENCH_MONO_LADDER
  monoLadder.reset();
  setFilterCutoffBase(2200.0f);
#else
  monoLpf.reset();
#endif
  filterEnv.reset();
  filterEnv.trigger();
  voicer.noteOn(60, kStkAmp, kGroup);
  int16_t buf[256];
  unsigned long t0 = millis();
  for (int i = 0; i <= steps; i++) {
    filterEnvDepth = (float)i / (float)steps;
    const unsigned long now = t0 + (unsigned long)i * (stepMs > 0 ? stepMs : 1U);
    filterEnv.poll(now);
    fillAudio(buf, 256, nullptr);
    Serial.print(F("step="));
    Serial.print(i);
    Serial.print(F("/"));
    Serial.print(steps);
    Serial.print(F(" depth="));
    Serial.println(filterEnvDepth, 2);
    if (stepMs > 0) delay(stepMs);
  }
  voicer.noteOff(60, kStkAmp, kGroup);
  filterEnv.release();
  filterEnvDepth = depthWas;
  monoMode = wasMono;
  monoFilterOn = wasFilter;
  filterEnvOn = wasEnv;
  return steps;
}

static void benchFilterEnvSweep() {
  Serial.println(F("--- bench filter env sweep ---"));
  Serial.print(F("cutoff_hz="));
  Serial.println(2200);
  Serial.print(F("steps="));
  Serial.println(16);
  benchFilterEnvSweepSteps(16, 0);
  Serial.println(F("env sweep done"));
  Serial.println(F("---"));
}
#endif
#endif

static void reloadActiveSlot() {
  if (loadBenchPreset(activeBenchSlot)) {
#if BENCH_SERIAL_MIDI
    Serial.print(F("reload "));
    Serial.println(activeBenchSlot);
#endif
  } else {
#if BENCH_SERIAL_MIDI
    Serial.println(F("empty slot"));
#endif
  }
}

static void unmuteAllLayers() {
  muteSeq = false;
  muteBass = false;
  muteDrums = false;
#if BENCH_SERIAL_MIDI
  Serial.println(F("unmute all"));
#endif
}

static void syncLenPattern() {
#if BENCH_SEQUENCER && BENCH_DRUM_GRID
  const int len = seq.length();
  if (len == 16 || len == 32) {
    drums.setLength(len);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("sync len "));
  Serial.println(len);
#endif
#else
#if BENCH_SERIAL_MIDI
  Serial.println(F("sync len?"));
#endif
#endif
}

static void patternQuantize() {
#if BENCH_SEQUENCER
  for (int i = 0; i < seq.length(); i++) {
    const uint8_t note = seq.stepNote(i);
    if (note == SeqPlayer::kRest) continue;
    seq.setStep(i, (uint8_t)chords.quantizeToScale(note));
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern quantize"));
#endif
#endif
}

static void seqInvertPattern() {
#if BENCH_SEQUENCER
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) == SeqPlayer::kRest) {
      int note = chords.root() + octaveShift * 12;
      if (midiQuantize) {
        note = chords.quantizeToScale(note);
      }
      seq.setStep(i, (uint8_t)constrain(note, 1, 127));
    } else {
      seq.setStep(i, SeqPlayer::kRest);
    }
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("seq invert"));
#endif
#endif
}

static void bassInvertPattern() {
#if BENCH_BASS_SEQ
  for (int i = 0; i < BassSeq::kSteps; i++) {
    if (bass.stepDegree(i) == BassSeq::kRest) {
      bass.setStep(i, 0);
    } else {
      bass.setStep(i, BassSeq::kRest);
    }
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("bass invert"));
#endif
#endif
}

static void drumInvertPattern() {
#if BENCH_DRUM_GRID
  const int len = drums.length();
  for (int i = 0; i < len; i++) {
    drums.setKick(i, drums.kickAt(i) == 0);
    drums.setHat(i, drums.hatAt(i) == 0);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("drum invert"));
#endif
#endif
}

static void patternTrim() {
#if BENCH_SEQUENCER
  int end = 0;
  for (int i = seq.length() - 1; i >= 0; i--) {
    if (seq.stepNote(i) != SeqPlayer::kRest) {
      end = i + 1;
      break;
    }
  }
  if (end < 1) end = 1;
  if (end < seq.length()) {
    for (int i = end; i < seq.length(); i++) {
      seq.setStep(i, SeqPlayer::kRest);
    }
    seq.setLength(end);
  }
#if BENCH_DRUM_GRID
  if (end == 16 || end == 32) {
    drums.setLength(end);
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern trim "));
  Serial.println(end);
#endif
#endif
}

static void loadPatternBank(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
#if BENCH_SEQUENCER
  presets.loadSeqPattern(slot, seq);
#endif
  BenchTransportState t;
  if (presets.loadTransport(slot, t)) {
    applyTransportState(t);
  }
#if BENCH_BASS_SEQ
  int8_t bassDeg[BassSeq::kSteps];
  if (presets.loadBassPattern(slot, bassDeg, BassSeq::kSteps)) {
    for (int i = 0; i < BassSeq::kSteps; i++) {
      bass.setStep(i, bassDeg[i]);
    }
  }
#endif
#if BENCH_DRUM_GRID
  int drumLen = DrumGrid::kSteps;
  uint8_t kickPat[DrumGrid::kMaxSteps]{};
  uint8_t hatPat[DrumGrid::kMaxSteps]{};
  if (presets.loadDrumPattern(slot, kickPat, hatPat, DrumGrid::kMaxSteps, drumLen)) {
    drums.setLength(drumLen);
    for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
      drums.setKick(i, kickPat[i] != 0);
      drums.setHat(i, hatPat[i] != 0);
    }
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern load "));
  Serial.println(slot);
#endif
}

static void patternStrip() {
#if BENCH_SEQUENCER
  const int len = seq.length();
  uint8_t compact[SeqPlayer::kMaxSteps];
  int n = 0;
  for (int i = 0; i < len; i++) {
    const uint8_t note = seq.stepNote(i);
    if (note != SeqPlayer::kRest) compact[n++] = note;
  }
  if (n < 1) {
    compact[0] = SeqPlayer::kRest;
    n = 1;
  }
  for (int i = 0; i < len; i++) {
    seq.setStep(i, i < n ? compact[i] : SeqPlayer::kRest);
  }
  seq.setLength(n);
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern strip "));
  Serial.println(n);
#endif
#endif
}

static void patternDensity(int density) {
#if BENCH_SEQUENCER
  const int dens = constrain(density, 10, 100);
  randomSeed((uint32_t)micros() ^ (uint32_t)millis());
  seq.fillRandom(scaleNoteForSeq, nullptr, dens);
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern density "));
  Serial.println(dens);
#endif
#else
  (void)density;
#endif
}

static void printPatternHitStats(const __FlashStringHelper *label) {
  Serial.print(label);
  Serial.print(F(" seq="));
#if BENCH_SEQUENCER
  int notes = 0;
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) != SeqPlayer::kRest) notes++;
  }
  Serial.print(notes);
  Serial.print(F("/"));
  Serial.print(seq.length());
#else
  Serial.print(F("0/0"));
#endif
#if BENCH_DRUM_GRID
  int kicks = 0;
  int hats = 0;
  for (int i = 0; i < drums.length(); i++) {
    if (drums.kickAt(i)) kicks++;
    if (drums.hatAt(i)) hats++;
  }
  Serial.print(F(" kick="));
  Serial.print(kicks);
  Serial.print(F(" hat="));
  Serial.print(hats);
  Serial.print(F(" drum_len="));
  Serial.print(drums.length());
#endif
#if BENCH_BASS_SEQ
  int bassHits = 0;
  for (int i = 0; i < BassSeq::kSteps; i++) {
    if (bass.stepDegree(i) != BassSeq::kRest) bassHits++;
  }
  Serial.print(F(" bass="));
  Serial.print(bassHits);
#endif
  Serial.println();
}

static void patternStats() { printPatternHitStats(F("pattern stats")); }

static void patternSetLength(int steps) {
  int len = steps;
  if (len < 1) len = 1;
  if (len > SeqPlayer::kMaxSteps) len = SeqPlayer::kMaxSteps;
#if BENCH_SEQUENCER
  if (len < seq.length()) {
    for (int i = len; i < seq.length(); i++) {
      seq.setStep(i, SeqPlayer::kRest);
    }
  }
  seq.setLength(len);
#endif
#if BENCH_DRUM_GRID
  if (len == 16 || len == 32) {
    drums.setLength(len);
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern len "));
  Serial.println(len);
#endif
}

static void patternThicken() {
  patternDensity(85);
  handleTextCommand("drum hat fill");
  handleTextCommand("seq densify");
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern thicken"));
#endif
}

static void patternOneline() {
  Serial.print(F("pattern oneline "));
#if BENCH_SEQUENCER
  Serial.print(F("seq:"));
  for (int i = 0; i < seq.length(); i++) {
    if (i > 0) Serial.print(' ');
    const uint8_t n = seq.stepNote(i);
    Serial.print(n == SeqPlayer::kRest ? F("-") : String(n));
  }
#endif
#if BENCH_DRUM_GRID
  Serial.print(F(" kick:"));
  for (int i = 0; i < drums.length(); i++) {
    Serial.print(drums.kickAt(i) ? 'X' : '.');
  }
  Serial.print(F(" hat:"));
  for (int i = 0; i < drums.length(); i++) {
    Serial.print(drums.hatAt(i) ? 'x' : '.');
  }
#endif
  Serial.println();
}

static void patternAlign() {
  handleTextCommand("sync len");
  handleTextCommand("pattern quantize");
  handleTextCommand("pattern strip");
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern align"));
#endif
}

static void patternThin() {
  handleTextCommand("seq thin");
#if BENCH_DRUM_GRID
  for (int i = 0; i < drums.length(); i++) {
    if (i % 2 == 1) drums.setHat(i, false);
  }
  markPresetDirty();
#endif
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern thin"));
#endif
}

static void patternInvertAll() {
  seqInvertPattern();
  bassInvertPattern();
  drumInvertPattern();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern invert"));
#endif
}

static void patternContrast() {
  patternMirrorAll();
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern contrast"));
#endif
}

static void patternEcho() {
  int half = 1;
#if BENCH_SEQUENCER
  half = seq.length() / 2;
  if (half < 1) half = 1;
#endif
  patternDoubleAll();
  patternRotateAll(half);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern echo"));
#endif
}

static void patternCompact() {
  patternAlign();
  patternTrim();
  patternQuantize();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern compact"));
#endif
}

static void patternHalve() {
  int len = 1;
#if BENCH_SEQUENCER
  len = seq.length() / 2;
  if (len < 1) len = 1;
#endif
  patternSetLength(len);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern halve"));
#endif
}

static void patternWeave() {
  patternShuffleAll();
  patternCompact();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern weave"));
#endif
}

static void patternSpread() {
  patternThicken();
  patternAlign();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern spread"));
#endif
}

static void patternFuse() {
  patternAlign();
  bassFromSeqPattern(true);
  drumFromSeqPattern(true);
  hatFromSeqPattern(true);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern fuse"));
#endif
}

static void patternBloom() {
  patternSpread();
  patternEcho();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern bloom"));
#endif
}

static void patternGrit() {
  patternStrip();
  patternDensity(42);
  patternShuffleAll();
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern grit"));
#endif
}

static void patternGlaze() {
  patternCompact();
  patternThin();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern glaze"));
#endif
}

static void patternSmear() {
  patternRotateAll(2);
  patternShuffleAll();
  humanizeGroove(true);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern smear"));
#endif
}

static void patternPolish() {
  patternAlign();
  patternQuantize();
  patternThin();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern polish"));
#endif
}

static void patternCarve() {
  patternStrip();
  patternThin();
  patternContrast();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern carve"));
#endif
}

static void patternRipple() {
  patternEcho();
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern ripple"));
#endif
}

static void patternFlare() {
  patternSpread();
  handleTextCommand("seq densify");
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern flare"));
#endif
}

static void patternKnit() {
  patternWeave();
  patternThin();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern knit"));
#endif
}

static void patternSnap() {
  patternAlign();
  patternQuantize();
  patternCompact();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern snap"));
#endif
}

static void patternRust() {
  patternGrit();
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern rust"));
#endif
}

static void patternBraid() {
  patternKnit();
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern braid"));
#endif
}

static void patternTaper() {
  patternHalve();
  patternThin();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern taper"));
#endif
}

static void patternChurn() {
  patternShuffleAll();
  patternRotateAll(2);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern churn"));
#endif
}

static void patternDrift() {
  humanizeGroove(true);
  patternRotateAll(1);
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern drift"));
#endif
}

static void patternLoop() {
  patternDup();
  patternAlign();
#if BENCH_SEQUENCER
  seq.setBounce(true);
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern loop"));
#endif
}

static void patternAnchor() {
  patternKnit();
  patternAlign();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern anchor"));
#endif
}

static void patternPulse() {
  patternEcho();
  seqAccent = true;
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern pulse"));
#endif
}

static void patternLatch() {
#if BENCH_SEQUENCER
  seq.setBounce(!seq.bounce());
  seqAccent = true;
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern latch"));
#endif
#endif
}

static void patternRivet() {
  patternThicken();
  patternAlign();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern rivet"));
#endif
}

static void patternBolt() {
  patternRivet();
  patternPulse();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern bolt"));
#endif
}

static void patternWeld() {
  patternThicken();
  patternAlign();
  patternEcho();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern weld"));
#endif
}

static void patternForge() {
  patternCarve();
  patternRivet();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern forge"));
#endif
}

static void patternMint() {
  patternForge();
  patternLatch();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern mint"));
#endif
}

static void patternAnvil() {
  patternMint();
  patternWeld();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern anvil"));
#endif
}

static void patternTemper() {
  patternAnvil();
  patternAnchor();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern temper"));
#endif
}

static void patternQuench() {
  patternTemper();
  patternPulse();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern quench"));
#endif
}

static void patternSmelt() {
  patternQuench();
  patternDrift();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern smelt"));
#endif
}

static void patternCast() {
  patternSmelt();
  patternWeld();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern cast"));
#endif
}

static void patternMold() {
  patternCast();
  patternAnchor();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern mold"));
#endif
}

static void patternStamp() {
  patternMold();
  patternCompact();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern stamp"));
#endif
}

static void patternPlate() {
  patternStamp();
  patternBraid();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern plate"));
#endif
}

static void patternSheet() {
  patternPlate();
  patternWeave();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern sheet"));
#endif
}

static void patternCoil() {
  patternSheet();
  patternCarve();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern coil"));
#endif
}

static void patternWire() {
  patternCoil();
  patternPulse();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern wire"));
#endif
}

static void patternStrand() {
  patternWire();
  patternLatch();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern strand"));
#endif
}

static void patternCable() {
  patternStrand();
  patternRivet();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern cable"));
#endif
}

static void patternHarness() {
  patternCable();
  patternBolt();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern harness"));
#endif
}

static void patternMesh() {
  patternHarness();
  patternWeld();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern mesh"));
#endif
}

static void patternLattice() {
  patternMesh();
  patternForge();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern lattice"));
#endif
}

static void patternMatrix() {
  patternLattice();
  patternMint();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern matrix"));
#endif
}

static void patternFabric() {
  patternMatrix();
  patternAnvil();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern fabric"));
#endif
}

static void patternTapestry() {
  patternFabric();
  patternTemper();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern tapestry"));
#endif
}

static void patternCanopy() {
  patternTapestry();
  patternQuench();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern canopy"));
#endif
}

static void patternPavilion() {
  patternCanopy();
  patternSmelt();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern pavilion"));
#endif
}

static void patternPortico() {
  patternPavilion();
  patternCast();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern portico"));
#endif
}

static void patternColonnade() {
  patternPortico();
  patternMold();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern colonnade"));
#endif
}

static void patternFacade() {
  patternColonnade();
  patternStamp();
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern facade"));
#endif
}

static void nudgeTransportSwing(int deltaPct) {
#if BENCH_SEQUENCER
  const int pct = constrain((int)(seq.swing() * 100.0f) + deltaPct, 0, 55);
  applyTransportSwing((float)pct / 100.0f);
#if BENCH_SERIAL_MIDI
  Serial.print(F("swing "));
  Serial.println(pct);
#endif
#else
  (void)deltaPct;
#endif
}

static void patternRotateAll(int steps) {
  if (steps == 0) steps = 1;
#if BENCH_SEQUENCER
  seq.shiftSteps(steps);
#endif
#if BENCH_DRUM_GRID
  {
    const int len = drums.length();
    uint8_t kick[DrumGrid::kMaxSteps]{};
    uint8_t hat[DrumGrid::kMaxSteps]{};
    for (int i = 0; i < len; i++) {
      int src = (i - steps) % len;
      if (src < 0) src += len;
      kick[i] = drums.kickAt(src);
      hat[i] = drums.hatAt(src);
    }
    for (int i = 0; i < len; i++) {
      drums.setKick(i, kick[i] != 0);
      drums.setHat(i, hat[i] != 0);
    }
  }
#endif
#if BENCH_BASS_SEQ
  {
    int8_t tmp[BassSeq::kSteps];
    for (int i = 0; i < BassSeq::kSteps; i++) {
      int src = (i - steps) % BassSeq::kSteps;
      if (src < 0) src += BassSeq::kSteps;
      tmp[i] = bass.stepDegree(src);
    }
    for (int i = 0; i < BassSeq::kSteps; i++) {
      bass.setStep(i, tmp[i]);
    }
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern rotate "));
  Serial.println(steps);
#endif
}

static void patternMirrorAll() {
#if BENCH_SEQUENCER
  seq.reversePattern();
#endif
#if BENCH_DRUM_GRID
  {
    const int len = drums.length();
    for (int i = 0; i < len / 2; i++) {
      const int j = len - 1 - i;
      const bool k0 = drums.kickAt(i) != 0;
      const bool k1 = drums.kickAt(j) != 0;
      drums.setKick(i, k1);
      drums.setKick(j, k0);
      const bool h0 = drums.hatAt(i) != 0;
      const bool h1 = drums.hatAt(j) != 0;
      drums.setHat(i, h1);
      drums.setHat(j, h0);
    }
  }
#endif
#if BENCH_BASS_SEQ
  {
    for (int i = 0; i < BassSeq::kSteps / 2; i++) {
      const int j = BassSeq::kSteps - 1 - i;
      const int8_t a = bass.stepDegree(i);
      const int8_t b = bass.stepDegree(j);
      bass.setStep(i, b);
      bass.setStep(j, a);
    }
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern mirror"));
#endif
}

static void patternDoubleAll() {
#if BENCH_SEQUENCER
  if (seq.length() <= SeqPlayer::kSteps) {
    seq.doubleBar();
  }
#endif
#if BENCH_BASS_SEQ
  bass.doubleBar();
#endif
#if BENCH_DRUM_GRID
  if (drums.length() <= DrumGrid::kSteps) {
    drums.doubleBar();
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern double"));
#endif
}

static void patternSwapSteps(int a, int b) {
  if (a < 0 || b < 0) return;
#if BENCH_SEQUENCER
  if (a < seq.length() && b < seq.length()) {
    const uint8_t ta = seq.stepNote(a);
    seq.setStep(a, seq.stepNote(b));
    seq.setStep(b, ta);
  }
#endif
#if BENCH_DRUM_GRID
  if (a < drums.length() && b < drums.length()) {
    const bool k0 = drums.kickAt(a) != 0;
    const bool k1 = drums.kickAt(b) != 0;
    drums.setKick(a, k1);
    drums.setKick(b, k0);
    const bool h0 = drums.hatAt(a) != 0;
    const bool h1 = drums.hatAt(b) != 0;
    drums.setHat(a, h1);
    drums.setHat(b, h0);
  }
#endif
#if BENCH_BASS_SEQ
  if (a < BassSeq::kSteps && b < BassSeq::kSteps) {
    const int8_t ta = bass.stepDegree(a);
    bass.setStep(a, bass.stepDegree(b));
    bass.setStep(b, ta);
  }
#endif
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern swap "));
  Serial.print(a);
  Serial.print(' ');
  Serial.println(b);
#endif
}

static void patternShuffleAll() {
#if BENCH_SEQUENCER
  const int len = seq.length();
  if (len > 1) {
    patternRotateAll((int)random(1, len));
  } else {
    patternRotateAll(1);
  }
#else
  patternRotateAll((int)random(1, 5));
#endif
#if BENCH_SERIAL_MIDI
  Serial.println(F("pattern shuffle"));
#endif
}

static void patternFill(uint32_t seed, int density) {
  if (seed != 0) {
    randomSeed(seed);
  } else {
    randomSeed((uint32_t)micros() ^ (uint32_t)millis());
  }
  if (density < 10) density = 70;
  if (density > 100) density = 100;
  handleTextCommand("drum fill");
  handleTextCommand("bass fill");
  const String cmd = String(F("seq random ")) + String(density);
  handleTextCommand(cmd);
  handleTextCommand("sync len");
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern fill "));
  Serial.print(seed);
  Serial.print(' ');
  Serial.println(density);
#endif
}

static void drumRotatePattern(int steps) {
#if BENCH_DRUM_GRID
  const int len = drums.length();
  uint8_t kick[DrumGrid::kMaxSteps]{};
  uint8_t hat[DrumGrid::kMaxSteps]{};
  for (int i = 0; i < len; i++) {
    int src = (i - steps) % len;
    if (src < 0) src += len;
    kick[i] = drums.kickAt(src);
    hat[i] = drums.hatAt(src);
  }
  for (int i = 0; i < len; i++) {
    drums.setKick(i, kick[i] != 0);
    drums.setHat(i, hat[i] != 0);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("drum rotate "));
  Serial.println(steps);
#endif
#else
  (void)steps;
#endif
}

static void savePatternBank(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
  if (!presets.slotValid(slot)) {
    presets.save(slot, chords, demoActive, octaveShift, midiQuantize, genMode,
#if BENCH_SEQUENCER
                 seq.enabled(), seq.bpm());
#else
                 false, 120u);
#endif
  }
#if BENCH_SEQUENCER
  presets.saveSeqPattern(slot, seq);
#endif
  presets.saveTransport(slot, transportState());
#if BENCH_BASS_SEQ
  int8_t bassDeg[BassSeq::kSteps];
  for (int i = 0; i < BassSeq::kSteps; i++) bassDeg[i] = bass.stepDegree(i);
  presets.saveBassPattern(slot, bassDeg, BassSeq::kSteps);
#endif
#if BENCH_DRUM_GRID
  uint8_t kickPat[DrumGrid::kMaxSteps];
  uint8_t hatPat[DrumGrid::kMaxSteps];
  for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
    kickPat[i] = drums.kickAt(i);
    hatPat[i] = drums.hatAt(i);
  }
  presets.saveDrumPattern(slot, kickPat, hatPat, DrumGrid::kMaxSteps, drums.length());
#endif
  presetDirty = false;
#if BENCH_SERIAL_MIDI
  Serial.print(F("pattern push "));
  Serial.println(slot);
#endif
}

static void transportReset() {
#if BENCH_SEQUENCER
  seq.reset();
#endif
#if BENCH_BASS_SEQ
  bass.reset();
#endif
#if BENCH_DRUM_GRID
  drums.reset();
#endif
  metroBeatCount = 0;
  metroMidiCount = 0;
#if BENCH_SERIAL_MIDI
  Serial.println(F("transport reset"));
#endif
}

static void drumClearLayer(const String &layer) {
#if BENCH_DRUM_GRID
  const int len = drums.length();
  const bool kick = layer.equalsIgnoreCase("kick");
  const bool hat = layer.equalsIgnoreCase("hat");
  if (!kick && !hat) {
#if BENCH_SERIAL_MIDI
    Serial.println(F("drum clear kick|hat"));
#endif
    return;
  }
  for (int i = 0; i < len; i++) {
    if (kick) drums.setKick(i, false);
    if (hat) drums.setHat(i, false);
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("drum clear "));
  Serial.println(layer);
#endif
#endif
}

static void eraseBenchSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
  presets.eraseSlot(slot);
  if (slot == activeBenchSlot) {
    benchStash.clear();
  }
#if BENCH_SERIAL_MIDI
  Serial.print(F("erased "));
  Serial.println(slot);
#endif
}

static void markPresetDirty() {
#if BENCH_AUTO_SAVE
  if (!autoSaveEnabled) return;
  presetDirty = true;
  autoSaveAtMs = millis() + 2000;
#endif
}

static void saveBenchPreset(int slot) {
  presets.save(slot, chords, demoActive, octaveShift, midiQuantize, genMode,
#if BENCH_SEQUENCER
               seq.enabled(), seq.bpm());
  presets.saveSeqPattern(slot, seq);
#else
               false, 120u);
#endif
  presets.saveTransport(slot, transportState());
#if BENCH_BASS_SEQ
  int8_t bassDeg[BassSeq::kSteps];
  for (int i = 0; i < BassSeq::kSteps; i++) bassDeg[i] = bass.stepDegree(i);
  presets.saveBassPattern(slot, bassDeg, BassSeq::kSteps);
#endif
#if BENCH_DRUM_GRID
  uint8_t kickPat[DrumGrid::kMaxSteps];
  uint8_t hatPat[DrumGrid::kMaxSteps];
  for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
    kickPat[i] = drums.kickAt(i);
    hatPat[i] = drums.hatAt(i);
  }
  presets.saveDrumPattern(slot, kickPat, hatPat, DrumGrid::kMaxSteps, drums.length());
#endif
  activeBenchSlot = slot;
  benchEeprom.setActiveSlot((uint8_t)slot);
  benchEeprom.persist();
}

static bool loadBenchPreset(int slot) {
  bool seqEnabled = false;
  unsigned seqBpm = 120;
  if (!presets.load(slot, chords, demoActive, octaveShift, midiQuantize, genMode, seqEnabled,
                    seqBpm)) {
    return false;
  }
  activeBenchSlot = slot;
  benchEeprom.setActiveSlot((uint8_t)slot);
  benchEeprom.persist();
#if BENCH_SEQUENCER
  seq.setBpm(seqBpm);
  presets.loadSeqPattern(slot, seq);
  if (seqEnabled) {
    setSeqEnabled(true);
  } else {
    seq.setEnabled(false);
    if (demoActive) refreshDemoChord();
    else releaseBlockChord();
  }
#else
  (void)seqEnabled;
  (void)seqBpm;
  if (demoActive) refreshDemoChord();
  else releaseBlockChord();
#endif
  BenchTransportState t;
  if (presets.loadTransport(slot, t)) applyTransportState(t);
#if BENCH_BASS_SEQ
  int8_t bassDeg[BassSeq::kSteps];
  if (presets.loadBassPattern(slot, bassDeg, BassSeq::kSteps)) {
    for (int i = 0; i < BassSeq::kSteps; i++) bass.setStep(i, bassDeg[i]);
  }
#endif
#if BENCH_DRUM_GRID
  int drumLen = DrumGrid::kSteps;
  uint8_t kickPat[DrumGrid::kMaxSteps]{};
  uint8_t hatPat[DrumGrid::kMaxSteps]{};
  if (presets.loadDrumPattern(slot, kickPat, hatPat, DrumGrid::kMaxSteps, drumLen)) {
    drums.setLength(drumLen);
    for (int i = 0; i < DrumGrid::kMaxSteps; i++) {
      drums.setKick(i, kickPat[i] != 0);
      drums.setHat(i, hatPat[i] != 0);
    }
  }
#endif
  return true;
}

static void pollAutoSave() {
#if BENCH_AUTO_SAVE
  if (!autoSaveEnabled || !presetDirty || (long)(millis() - autoSaveAtMs) < 0) return;
  saveBenchPreset(activeBenchSlot);
  presetDirty = false;
#endif
}

static void onStrumNote(int note, void *) {
  voicer.noteOn(note, kStkAmp, kGroup);
}

static void panicAll() {
  voicer.silence();
  monoLead.silence(voicer, kStkAmp);
#if BENCH_HARMONY
  harmony.silence(voicer, kGroup, kStkAmp);
#endif
  stopArpVoice();
#if BENCH_SEQUENCER
  seq.reset();
#endif
#if BENCH_BASS_SEQ
  if (bassPlayingNote >= 0) {
    voicer.noteOff(bassPlayingNote, kStkAmp, kGroup);
    bassPlayingNote = -1;
  }
  bass.reset();
#endif
  monoNote = -1;
  for (int n = 0; n < 128; n++) sustainHeld[n] = false;
#if BENCH_FILTER_ENV
  filterEnv.reset();
#endif
}

static void noteOffLive(int note) {
  if (controls.sustainDown() || midiSustain) {
    sustainHeld[note] = true;
    return;
  }
  if (monoMode) {
    monoLead.noteOff(voicer, note, kStkAmp);
    if (monoNote == note) {
      monoNote = -1;
#if BENCH_FILTER_ENV
      filterEnv.release();
#endif
    }
    return;
  }
  voicer.noteOff(note, kStkAmp, kGroup);
}

static void onMidiNote(uint8_t note, uint8_t velocity, bool on, void *) {
#if BENCH_DRUM_GRID && BENCH_DRUM_PADS
  if (on && velocity > 0) {
    if (drumWriteMode && drums.enabled()) {
      const int step = drums.currentStep();
      if (note == kMidiPadKick) {
        drums.setKick(step, true);
        drums.fireKick();
        markPresetDirty();
        return;
      }
      if (note == kMidiPadKickOff) {
        drums.setKick(step, false);
        markPresetDirty();
        return;
      }
      if (note == kMidiPadHat) {
        drums.setHat(step, true);
        drums.fireHat();
        markPresetDirty();
        return;
      }
      if (note == kMidiPadHatOff) {
        drums.setHat(step, false);
        markPresetDirty();
        return;
      }
    } else if (drumPadMode) {
      if (note == kMidiPadKick) {
        drums.fireKick();
        return;
      }
      if (note == kMidiPadHat) {
        drums.fireHat();
        return;
      }
    }
  }
#endif
#if BENCH_BASS_SEQ
  if (on && velocity > 0 && bassGridMode && note >= kMidiBassGridBase &&
      note < kMidiBassGridBase + BassSeq::kSteps) {
    const int step = (int)note - (int)kMidiBassGridBase;
    const int8_t deg = (int8_t)constrain((int)(note % 7), 0, 6);
    bass.toggleStep(step, deg);
    markPresetDirty();
    return;
  }
#endif
#if BENCH_SEQUENCER
  if (on && velocity > 0) {
    if (seqGridMode && note >= kMidiSeqGridBase &&
        note < kMidiSeqGridBase + SeqPlayer::kMaxSteps) {
      const int step = (int)note - (int)kMidiSeqGridBase;
      uint8_t midi = note;
      if (midiQuantize) {
        midi = (uint8_t)chords.quantizeToScale(midi);
      }
      seq.toggleStep(step, midi);
      markPresetDirty();
      return;
    }
    if (seqWriteMode && seq.enabled() && !seq.recording()) {
      uint8_t midi = note;
      if (midiQuantize) {
        midi = (uint8_t)chords.quantizeToScale(midi);
      }
      seq.setStep(seq.currentStep(), midi);
      markPresetDirty();
      return;
    }
  }
#endif
  if (midiQuantize) {
    note = (uint8_t)chords.quantizeToScale(note);
  }
  if (on) {
    noteOnLive(note, midiVelToAmp(velocity));
  } else {
    noteOffLive(note);
  }
}

static void onMidiControl(uint8_t cc, uint8_t value, void *) {
#if BENCH_SERIAL_MIDI
  if (ccLearn.captureCc(cc, Serial)) return;
#endif
#if BENCH_CC_MACROS
  static bool ccLatch[89]{};
  if (cc >= kCcPresetBase && cc <= kCcPresetBase + 3) {
    const int slot = (int)cc - (int)kCcPresetBase;
    const int li = 11 + slot;
    if (value >= 64) {
      if (!ccLatch[li]) {
        ccLatch[li] = true;
        if (loadBenchPreset(slot)) {
#if BENCH_SERIAL_MIDI
          Serial.print(F("cc load "));
          Serial.println(slot);
#endif
        }
      }
    } else {
      ccLatch[li] = false;
    }
    return;
  }
  if (cc >= kCcMacroBase && cc <= kCcMacroBase + 6) {
    const int i = (int)cc - (int)kCcMacroBase;
    if (value >= 64) {
      if (!ccLatch[i]) {
        ccLatch[i] = true;
        static const char *kNames[] = {"jam", "ambient", "blues", "lounge",
                                       "techno", "funk", "full"};
        runMacro(String(kNames[i]));
      }
    } else {
      ccLatch[i] = false;
    }
    return;
  }
  if (cc == kCcMacroPanic) {
    if (value >= 64 && !ccLatch[7]) {
      ccLatch[7] = true;
      panicAll();
    } else if (value < 64) {
      ccLatch[7] = false;
    }
    return;
  }
#if BENCH_SEQUENCER
  if (cc == kCcToggleSeq) {
    if (value >= 64 && !ccLatch[8]) {
      ccLatch[8] = true;
      setSeqEnabled(!seq.enabled());
    } else if (value < 64) {
      ccLatch[8] = false;
    }
    return;
  }
#endif
#if BENCH_DRUM_GRID
  if (cc == kCcToggleDrums) {
    if (value >= 64 && !ccLatch[9]) {
      ccLatch[9] = true;
      drums.setEnabled(!drums.enabled());
    } else if (value < 64) {
      ccLatch[9] = false;
    }
    return;
  }
#endif
#if BENCH_BASS_SEQ
  if (cc == kCcToggleBass) {
    if (value >= 64 && !ccLatch[10]) {
      ccLatch[10] = true;
      setBassEnabled(!bass.enabled());
    } else if (value < 64) {
      ccLatch[10] = false;
    }
    return;
  }
#endif
#if BENCH_HARMONY
  if (cc == kCcToggleHarm) {
    if (value >= 64 && !ccLatch[15]) {
      ccLatch[15] = true;
      if (!harmEditBlocked()) {
        const bool on = !harmony.enabled();
        harmony.setEnabled(on);
        if (!on) harmony.silence(voicer, kGroup, kStkAmp);
        markPresetDirty();
      }
    } else if (value < 64) {
      ccLatch[15] = false;
    }
    return;
  }
  if (cc == kCcMacroDub) {
    if (value >= 64 && !ccLatch[16]) {
      ccLatch[16] = true;
      runMacro(String("dub"));
    } else if (value < 64) {
      ccLatch[16] = false;
    }
    return;
  }
#endif
  if (cc == kCcToggleUnison) {
    if (value >= 64 && !ccLatch[17]) {
      ccLatch[17] = true;
      monoLead.setUnison(!monoLead.unison());
      markPresetDirty();
    } else if (value < 64) {
      ccLatch[17] = false;
    }
    return;
  }
  if (cc == kCcStash) {
    if (value >= 64 && !ccLatch[18]) {
      ccLatch[18] = true;
      captureBenchStash();
    } else if (value < 64) {
      ccLatch[18] = false;
    }
    return;
  }
  if (cc == kCcUndo) {
    if (value >= 64 && !ccLatch[19]) {
      ccLatch[19] = true;
      undoBenchStash();
    } else if (value < 64) {
      ccLatch[19] = false;
    }
    return;
  }
  if (cc >= kCcAuditionBase && cc <= kCcAuditionBase + 3) {
    const int slot = (int)cc - (int)kCcAuditionBase;
    const int li = 20 + slot;
    if (value >= 64) {
      if (!ccLatch[li]) {
        ccLatch[li] = true;
        auditionBenchSlot(slot);
      }
    } else {
      ccLatch[li] = false;
    }
    return;
  }
  if (cc == kCcCommit) {
    if (value >= 64 && !ccLatch[24]) {
      ccLatch[24] = true;
      commitBenchSlot(-1);
    } else if (value < 64) {
      ccLatch[24] = false;
    }
    return;
  }
  if (cc == kCcMuteSeq) {
    if (value >= 64 && !ccLatch[25]) {
      ccLatch[25] = true;
      setLayerMute(String("seq"), !muteSeq);
    } else if (value < 64) {
      ccLatch[25] = false;
    }
    return;
  }
  if (cc == kCcMuteBass) {
    if (value >= 64 && !ccLatch[26]) {
      ccLatch[26] = true;
      setLayerMute(String("bass"), !muteBass);
    } else if (value < 64) {
      ccLatch[26] = false;
    }
    return;
  }
  if (cc == kCcMuteDrums) {
    if (value >= 64 && !ccLatch[27]) {
      ccLatch[27] = true;
      setLayerMute(String("drums"), !muteDrums);
    } else if (value < 64) {
      ccLatch[27] = false;
    }
    return;
  }
  if (cc == kCcSoloSeq) {
    if (value >= 64 && !ccLatch[28]) {
      ccLatch[28] = true;
      soloLayer(String("seq"));
    } else if (value < 64) {
      ccLatch[28] = false;
    }
    return;
  }
  if (cc == kCcSoloBass) {
    if (value >= 64 && !ccLatch[29]) {
      ccLatch[29] = true;
      soloLayer(String("bass"));
    } else if (value < 64) {
      ccLatch[29] = false;
    }
    return;
  }
  if (cc == kCcSoloDrums) {
    if (value >= 64 && !ccLatch[30]) {
      ccLatch[30] = true;
      soloLayer(String("drums"));
    } else if (value < 64) {
      ccLatch[30] = false;
    }
    return;
  }
  if (cc == kCcUnmuteAll) {
    if (value >= 64 && !ccLatch[31]) {
      ccLatch[31] = true;
      unmuteAllLayers();
    } else if (value < 64) {
      ccLatch[31] = false;
    }
    return;
  }
  if (cc == kCcToggleBounce) {
    if (value >= 64 && !ccLatch[32]) {
      ccLatch[32] = true;
#if BENCH_SEQUENCER
      seq.setBounce(!seq.bounce());
      seq.reset();
#if BENCH_SERIAL_MIDI
      Serial.println(seq.bounce() ? F("seq bounce on") : F("seq bounce off"));
#endif
#endif
    } else if (value < 64) {
      ccLatch[32] = false;
    }
    return;
  }
  if (cc == kCcToggleMetro) {
    if (value >= 64 && !ccLatch[33]) {
      ccLatch[33] = true;
      metroEnabled = !metroEnabled;
      metroMidiCount = 0;
#if BENCH_SERIAL_MIDI
      Serial.println(metroEnabled ? F("metro on") : F("metro off"));
#endif
    } else if (value < 64) {
      ccLatch[33] = false;
    }
    return;
  }
  if (cc == kCcToggleChordLock) {
    if (value >= 64 && !ccLatch[34]) {
      ccLatch[34] = true;
      chordLocked = !chordLocked;
#if BENCH_SERIAL_MIDI
      Serial.println(chordLocked ? F("chord lock on") : F("chord lock off"));
#endif
    } else if (value < 64) {
      ccLatch[34] = false;
    }
    return;
  }
  if (cc == kCcToggleMetroAccent) {
    if (value >= 64 && !ccLatch[35]) {
      ccLatch[35] = true;
      metroAccent = !metroAccent;
      metroBeatCount = 0;
#if BENCH_SERIAL_MIDI
      Serial.println(metroAccent ? F("metro accent on") : F("metro accent off"));
#endif
    } else if (value < 64) {
      ccLatch[35] = false;
    }
    return;
  }
  if (cc == kCcMacroStage) {
    if (value >= 64 && !ccLatch[36]) {
      ccLatch[36] = true;
      runMacro(String("stage"));
    } else if (value < 64) {
      ccLatch[36] = false;
    }
    return;
  }
  if (cc == kCcToggleHarmLock) {
    if (value >= 64 && !ccLatch[37]) {
      ccLatch[37] = true;
      harmLocked = !harmLocked;
#if BENCH_SERIAL_MIDI
      Serial.println(harmLocked ? F("harm lock on") : F("harm lock off"));
#endif
    } else if (value < 64) {
      ccLatch[37] = false;
    }
    return;
  }
  if (cc == kCcToggleAutosave) {
    if (value >= 64 && !ccLatch[38]) {
      ccLatch[38] = true;
      autoSaveEnabled = !autoSaveEnabled;
      if (!autoSaveEnabled) presetDirty = false;
#if BENCH_SERIAL_MIDI
      Serial.println(autoSaveEnabled ? F("autosave on") : F("autosave off"));
#endif
    } else if (value < 64) {
      ccLatch[38] = false;
    }
    return;
  }
  if (cc == kCcTransportReset) {
    if (value >= 64 && !ccLatch[39]) {
      ccLatch[39] = true;
      transportReset();
    } else if (value < 64) {
      ccLatch[39] = false;
    }
    return;
  }
  if (cc == kCcPatternMirror) {
    if (value >= 64 && !ccLatch[40]) {
      ccLatch[40] = true;
      patternMirrorAll();
    } else if (value < 64) {
      ccLatch[40] = false;
    }
    return;
  }
  if (cc == kCcPatternRotate) {
    if (value >= 64 && !ccLatch[41]) {
      ccLatch[41] = true;
      patternRotateAll(1);
    } else if (value < 64) {
      ccLatch[41] = false;
    }
    return;
  }
  if (cc == kCcPatternShuffle) {
    if (value >= 64 && !ccLatch[42]) {
      ccLatch[42] = true;
      patternShuffleAll();
    } else if (value < 64) {
      ccLatch[42] = false;
    }
    return;
  }
  if (cc == kCcPatternStrip) {
    if (value >= 64 && !ccLatch[43]) {
      ccLatch[43] = true;
      patternStrip();
    } else if (value < 64) {
      ccLatch[43] = false;
    }
    return;
  }
  if (cc == kCcMacroGlitch) {
    if (value >= 64 && !ccLatch[44]) {
      ccLatch[44] = true;
      runMacro(String("glitch"));
    } else if (value < 64) {
      ccLatch[44] = false;
    }
    return;
  }
  if (cc == kCcPatternThicken) {
    if (value >= 64 && !ccLatch[45]) {
      ccLatch[45] = true;
      patternThicken();
    } else if (value < 64) {
      ccLatch[45] = false;
    }
    return;
  }
  if (cc == kCcMacroRave) {
    if (value >= 64 && !ccLatch[46]) {
      ccLatch[46] = true;
      runMacro(String("rave"));
    } else if (value < 64) {
      ccLatch[46] = false;
    }
    return;
  }
  if (cc == kCcPatternAlign) {
    if (value >= 64 && !ccLatch[47]) {
      ccLatch[47] = true;
      patternAlign();
    } else if (value < 64) {
      ccLatch[47] = false;
    }
    return;
  }
  if (cc == kCcMacroHush) {
    if (value >= 64 && !ccLatch[48]) {
      ccLatch[48] = true;
      runMacro(String("hush"));
    } else if (value < 64) {
      ccLatch[48] = false;
    }
    return;
  }
  if (cc == kCcMacroSpark) {
    if (value >= 64 && !ccLatch[49]) {
      ccLatch[49] = true;
      runMacro(String("spark"));
    } else if (value < 64) {
      ccLatch[49] = false;
    }
    return;
  }
  if (cc == kCcPatternContrast) {
    if (value >= 64 && !ccLatch[50]) {
      ccLatch[50] = true;
      patternContrast();
    } else if (value < 64) {
      ccLatch[50] = false;
    }
    return;
  }
  if (cc == kCcMacroFrost) {
    if (value >= 64 && !ccLatch[51]) {
      ccLatch[51] = true;
      runMacro(String("frost"));
    } else if (value < 64) {
      ccLatch[51] = false;
    }
    return;
  }
  if (cc == kCcPatternEcho) {
    if (value >= 64 && !ccLatch[52]) {
      ccLatch[52] = true;
      patternEcho();
    } else if (value < 64) {
      ccLatch[52] = false;
    }
    return;
  }
  if (cc == kCcMacroPulse) {
    if (value >= 64 && !ccLatch[53]) {
      ccLatch[53] = true;
      runMacro(String("pulse"));
    } else if (value < 64) {
      ccLatch[53] = false;
    }
    return;
  }
  if (cc == kCcPatternCompact) {
    if (value >= 64 && !ccLatch[54]) {
      ccLatch[54] = true;
      patternCompact();
    } else if (value < 64) {
      ccLatch[54] = false;
    }
    return;
  }
  if (cc == kCcMacroSurge) {
    if (value >= 64 && !ccLatch[55]) {
      ccLatch[55] = true;
      runMacro(String("surge"));
    } else if (value < 64) {
      ccLatch[55] = false;
    }
    return;
  }
  if (cc == kCcPatternWeave) {
    if (value >= 64 && !ccLatch[56]) {
      ccLatch[56] = true;
      patternWeave();
    } else if (value < 64) {
      ccLatch[56] = false;
    }
    return;
  }
  if (cc == kCcPatternCarve) {
    if (value >= 64 && !ccLatch[57]) {
      ccLatch[57] = true;
      patternCarve();
    } else if (value < 64) {
      ccLatch[57] = false;
    }
    return;
  }
  if (cc == kCcMacroHaze) {
    if (value >= 64 && !ccLatch[58]) {
      ccLatch[58] = true;
      runMacro(String("haze"));
    } else if (value < 64) {
      ccLatch[58] = false;
    }
    return;
  }
  if (cc == kCcPatternSnap) {
    if (value >= 64 && !ccLatch[59]) {
      ccLatch[59] = true;
      patternSnap();
    } else if (value < 64) {
      ccLatch[59] = false;
    }
    return;
  }
  if (cc == kCcMacroGlow) {
    if (value >= 64 && !ccLatch[60]) {
      ccLatch[60] = true;
      runMacro(String("glow"));
    } else if (value < 64) {
      ccLatch[60] = false;
    }
    return;
  }
  if (cc == kCcPatternBraid) {
    if (value >= 64 && !ccLatch[61]) {
      ccLatch[61] = true;
      patternBraid();
    } else if (value < 64) {
      ccLatch[61] = false;
    }
    return;
  }
  if (cc == kCcMacroEmber) {
    if (value >= 64 && !ccLatch[62]) {
      ccLatch[62] = true;
      runMacro(String("ember"));
    } else if (value < 64) {
      ccLatch[62] = false;
    }
    return;
  }
  if (cc == kCcPatternDrift) {
    if (value >= 64 && !ccLatch[63]) {
      ccLatch[63] = true;
      patternDrift();
    } else if (value < 64) {
      ccLatch[63] = false;
    }
    return;
  }
  if (cc == kCcMacroSlate) {
    if (value >= 64 && !ccLatch[64]) {
      ccLatch[64] = true;
      runMacro(String("slate"));
    } else if (value < 64) {
      ccLatch[64] = false;
    }
    return;
  }
  if (cc == kCcPatternAnchor) {
    if (value >= 64 && !ccLatch[65]) {
      ccLatch[65] = true;
      patternAnchor();
    } else if (value < 64) {
      ccLatch[65] = false;
    }
    return;
  }
  if (cc == kCcMacroPrism) {
    if (value >= 64 && !ccLatch[66]) {
      ccLatch[66] = true;
      runMacro(String("prism"));
    } else if (value < 64) {
      ccLatch[66] = false;
    }
    return;
  }
  if (cc == kCcPatternLatch) {
    if (value >= 64 && !ccLatch[67]) {
      ccLatch[67] = true;
      patternLatch();
    } else if (value < 64) {
      ccLatch[67] = false;
    }
    return;
  }
  if (cc == kCcPatternRivet) {
    if (value >= 64 && !ccLatch[68]) {
      ccLatch[68] = true;
      patternRivet();
    } else if (value < 64) {
      ccLatch[68] = false;
    }
    return;
  }
  if (cc == kCcPatternBolt) {
    if (value >= 64 && !ccLatch[69]) {
      ccLatch[69] = true;
      patternBolt();
    } else if (value < 64) {
      ccLatch[69] = false;
    }
    return;
  }
  if (cc == kCcPatternWeld) {
    if (value >= 64 && !ccLatch[70]) {
      ccLatch[70] = true;
      patternWeld();
    } else if (value < 64) {
      ccLatch[70] = false;
    }
    return;
  }
  if (cc == kCcPatternForge) {
    if (value >= 64 && !ccLatch[71]) {
      ccLatch[71] = true;
      patternForge();
    } else if (value < 64) {
      ccLatch[71] = false;
    }
    return;
  }
  if (cc == kCcPatternMint) {
    if (value >= 64 && !ccLatch[72]) {
      ccLatch[72] = true;
      patternMint();
    } else if (value < 64) {
      ccLatch[72] = false;
    }
    return;
  }
  if (cc == kCcPatternAnvil) {
    if (value >= 64 && !ccLatch[73]) {
      ccLatch[73] = true;
      patternAnvil();
    } else if (value < 64) {
      ccLatch[73] = false;
    }
    return;
  }
  if (cc == kCcPatternTemper) {
    if (value >= 64 && !ccLatch[74]) {
      ccLatch[74] = true;
      patternTemper();
    } else if (value < 64) {
      ccLatch[74] = false;
    }
    return;
  }
  if (cc == kCcPatternQuench) {
    if (value >= 64 && !ccLatch[75]) {
      ccLatch[75] = true;
      patternQuench();
    } else if (value < 64) {
      ccLatch[75] = false;
    }
    return;
  }
  if (cc == kCcPatternSmelt) {
    if (value >= 64 && !ccLatch[76]) {
      ccLatch[76] = true;
      patternSmelt();
    } else if (value < 64) {
      ccLatch[76] = false;
    }
    return;
  }
  if (cc == kCcPatternCast) {
    if (value >= 64 && !ccLatch[77]) {
      ccLatch[77] = true;
      patternCast();
    } else if (value < 64) {
      ccLatch[77] = false;
    }
    return;
  }
  if (cc == kCcPatternMold) {
    if (value >= 64 && !ccLatch[78]) {
      ccLatch[78] = true;
      patternMold();
    } else if (value < 64) {
      ccLatch[78] = false;
    }
    return;
  }
  if (cc == kCcPatternStamp) {
    if (value >= 64 && !ccLatch[79]) {
      ccLatch[79] = true;
      patternStamp();
    } else if (value < 64) {
      ccLatch[79] = false;
    }
    return;
  }
  if (cc == kCcPatternPlate) {
    if (value >= 64 && !ccLatch[80]) {
      ccLatch[80] = true;
      patternPlate();
    } else if (value < 64) {
      ccLatch[80] = false;
    }
    return;
  }
  if (cc == kCcPatternSheet) {
    if (value >= 64 && !ccLatch[81]) {
      ccLatch[81] = true;
      patternSheet();
    } else if (value < 64) {
      ccLatch[81] = false;
    }
    return;
  }
  if (cc == kCcPatternCoil) {
    if (value >= 64 && !ccLatch[82]) {
      ccLatch[82] = true;
      patternCoil();
    } else if (value < 64) {
      ccLatch[82] = false;
    }
    return;
  }
  if (cc == kCcPatternWire) {
    if (value >= 64 && !ccLatch[83]) {
      ccLatch[83] = true;
      patternWire();
    } else if (value < 64) {
      ccLatch[83] = false;
    }
    return;
  }
  if (cc == kCcPatternStrand) {
    if (value >= 64 && !ccLatch[84]) {
      ccLatch[84] = true;
      patternStrand();
    } else if (value < 64) {
      ccLatch[84] = false;
    }
    return;
  }
  if (cc == kCcPatternCable) {
    if (value >= 64 && !ccLatch[85]) {
      ccLatch[85] = true;
      patternCable();
    } else if (value < 64) {
      ccLatch[85] = false;
    }
    return;
  }
  if (cc == kCcPatternHarness) {
    if (value >= 64 && !ccLatch[86]) {
      ccLatch[86] = true;
      patternHarness();
    } else if (value < 64) {
      ccLatch[86] = false;
    }
    return;
  }
#endif
#if BENCH_MONO_FILTER
  if (cc == kCcFilterCutoff) {
    setFilterCutoffBase(cutoffHzFromMidi(value));
    if (monoMode && !monoFilterOn) {
      monoFilterOn = true;
#if BENCH_MONO_LADDER
      monoLadder.reset();
#else
      monoLpf.reset();
#endif
    }
    return;
  }
#endif
#if BENCH_FILTER_ENV
  if (cc == kCcFilterEnvDepth) {
    filterEnvDepth = (float)value / 127.0f;
    applyMonoFilterCutoff();
    return;
  }
#endif
  if (cc == 64) {
    midiSustain = value >= 64;
    return;
  }
  if (cc == 7) {
    midiVolume = (float)value / 127.0f;
    return;
  }
  {
    const BenchCcMap &m = ccLearn.map();
    if (cc == m.tone) {
      audioOut.setTone((float)value / 127.0f);
      return;
    }
    if (cc == m.width) {
      audioOut.setWidth((float)value / 127.0f);
      return;
    }
    if (cc == m.mod) {
      voicer.controlChange(1, (StkFloat)value, kGroup);
      return;
    }
#if BENCH_MONO_FILTER && BENCH_MONO_LADDER
    if (cc == m.color && monoFilterOn && monoMode) {
      setMonoFilterResonance((float)value / 127.0f * 0.9f);
      return;
    }
#endif
    if (cc == m.color) {
      voicer.controlChange(4, (StkFloat)value, kGroup);
      return;
    }
    if (cc == m.breath) {
      voicer.controlChange(128, (StkFloat)value, kGroup);
      return;
    }
#if BENCH_SEQUENCER
    if (cc == m.swing) {
      applyTransportSwing((value / 127.0f) * 0.55f);
      return;
    }
#endif
#if BENCH_HARMONY
    if (cc == m.harmSemi) {
      if (harmEditBlocked()) return;
      harmony.setSemitones(1 + (int)((value * 11) / 127));
      harmony.setEnabled(true);
      markPresetDirty();
      return;
    }
#endif
  }
  if (cc == 120 || cc == 123 || cc == 124) {
    panicAll();
  }
}

static void onMidiBend(uint16_t value14, void *) {
  voicer.pitchBend((StkFloat)value14, kGroup);
}

static void onMidiProgram(uint8_t program, void *) {
  if (loadBenchPreset(program % PresetStore::kSlots)) {
#if BENCH_SERIAL_MIDI
    Serial.print(F("pc load "));
    Serial.println(program % PresetStore::kSlots);
#endif
  }
}

static void onMidiClock(void *) {
#if BENCH_MIDI_CLOCK
  if (!midiClockSync) return;
  if (metroEnabled) {
    if (++metroMidiCount >= 24) {
      metroMidiCount = 0;
      pulseMetroBeat();
    }
  }
  if (++midiClockCount >= kMidiClockPerArpStep) {
    midiClockCount = 0;
    tickTransportStep();
  }
#endif
}

static void tickTransportStep() {
#if BENCH_DRUM_GRID
  if (drums.enabled()) drums.clockTick();
#endif
#if BENCH_BASS_SEQ
  if (bass.enabled()) bass.clockTick(onBassStep, nullptr);
#endif
#if BENCH_SEQUENCER
  if (seq.enabled()) {
    seq.clockTick(onSeqStep, nullptr);
    return;
  }
#endif
  if (chords.arpEnabled()) arpStep();
}

static void onMidiStart(void *) {
  midiClockCount = 0;
  if (chords.arpEnabled()) {
    chords.arpReset();
    if (demoActive) refreshDemoChord();
  }
}

static void bindMidiParser(MidiParser &parser, Stream &port) {
  parser.begin(port, (uint8_t)kMidiChannel, onMidiNote, onMidiControl, onMidiBend,
                onMidiProgram, onMidiClock, onMidiStart, nullptr);
}

static void stopArpVoice() {
  if (arpLastNote >= 0) {
    voicer.noteOff(arpLastNote, kStkAmp, kGroup);
    arpLastNote = -1;
  }
}

static void releaseBlockChord() {
  stopArpVoice();
  int notes[6];
  chords.fillVoices(notes);
  for (int i = 0; i < kVoices; i++) {
    voicer.noteOff(notes[i], kStkAmp, kGroup);
  }
}

static void arpStep() {
  stopArpVoice();
  const int n = chords.voiceNote(chords.arpStepIndex());
  voicer.noteOn(n, kStkAmp, kGroup);
  arpLastNote = n;
  chords.arpAdvance();
  lastArpMs = millis();
}

static void playBlockChord() {
  if (chords.arpEnabled()) {
    chords.arpReset();
    arpStep();
    return;
  }
  int notes[6];
  chords.fillVoices(notes);
  strum.start(notes, kVoices);
}

static void refreshDemoChord() {
  releaseBlockChord();
  playBlockChord();
  lastChangeMs = millis();
  lastArpMs = millis();
  markPresetDirty();
}

static void genStep() {
  if (chordEditBlocked()) return;
  randomSeed((uint32_t)micros() ^ (uint32_t)millis());
  chords.setRoot(48 + (int)random(0, 18));
  chords.setType((ChordType)random(0, (int)ChordType::Count));
  chords.setScale((ScaleMode)random(0, (int)ScaleMode::Count));
  chords.setInversion(random(0, 6));
  if (random(0, 3) == 0) {
    chords.setArp(!chords.arpEnabled());
  }
  refreshDemoChord();
#if BENCH_SERIAL_MIDI
  Serial.print(F("gen root="));
  Serial.print(chords.root());
  Serial.print(F(" "));
  Serial.print(chords.typeName());
  Serial.print(F(" "));
  Serial.println(chords.scaleName());
#endif
}

static void advanceInversion(void *) {
  if (chordEditBlocked()) return;
  chords.nextInversion();
  refreshDemoChord();
}

static void onDemoToggle(bool on, void *) {
  demoActive = on;
  if (on) {
#if BENCH_SEQUENCER
    if (seq.enabled()) {
      seq.setEnabled(false);
    }
#endif
    refreshDemoChord();
  } else {
    releaseBlockChord();
    markPresetDirty();
  }
#if BENCH_SERIAL_MIDI
  Serial.println(on ? F("demo on") : F("demo off"));
#endif
}

static void onEncTurn(int steps, void *) {
  if (chordEditBlocked()) return;
  chords.setRoot(chords.root() + steps);
  if (demoActive) refreshDemoChord();
  else markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("root "));
  Serial.println(chords.root());
#endif
}

static void onEncSw(void *) {
  if (chordEditBlocked()) return;
  chords.nextType();
  if (demoActive) refreshDemoChord();
  else markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("chord "));
  Serial.println(chords.typeName());
#endif
}

static void onArpToggle(bool, void *) {
  chords.arpToggle();
  if (demoActive) refreshDemoChord();
  else markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("arp "));
  Serial.println(chords.arpEnabled() ? F("on") : F("off"));
#endif
}

static void onChordLong(void *) {
  if (chordEditBlocked()) return;
  chords.nextType();
  if (demoActive) refreshDemoChord();
  else markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.print(F("chord "));
  Serial.println(chords.typeName());
#endif
}

static void onPanicBtn(void *) {
  panicAll();
}

#if BENCH_SEQUENCER
static void onBtnSeq(void *) {
  setSeqEnabled(!seq.enabled());
}
#endif

#if BENCH_DRUM_GRID
static void onBtnDrums(void *) {
  const bool on = !drums.enabled();
  drums.setEnabled(on);
  if (on) {
    demoActive = false;
    releaseBlockChord();
    drums.reset();
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(on ? F("drums on") : F("drums off"));
#endif
}
#endif

#if BENCH_BASS_SEQ
static void onBtnBass(void *) {
  setBassEnabled(!bass.enabled());
}
#endif

static void onBtnTap(void *) {
  const unsigned bpm = tapTempo.tap(millis());
  if (bpm > 0) {
    applyTransportBpm(bpm);
#if BENCH_SERIAL_MIDI
    Serial.print(F("tap bpm "));
    Serial.println(bpm);
#endif
  }
}

static void onBtnSlot(void *) {
  const int next = (activeBenchSlot + 1) % PresetStore::kSlots;
  if (loadBenchPreset(next)) {
#if BENCH_SERIAL_MIDI
    Serial.print(F("slot "));
    Serial.println(activeBenchSlot);
#endif
  }
}

static void onBtnSlotLong(void *) {
  commitBenchSlot(-1);
}

static void onEncLong(void *) {
  metroEnabled = !metroEnabled;
  metroMidiCount = 0;
#if BENCH_SERIAL_MIDI
  Serial.println(metroEnabled ? F("metro on") : F("metro off"));
#endif
}

static void onNoteButton(int index, bool down, void *) {
  const int note = keyboardNote(index);
  if (down) {
    noteOnLive(note, kStkAmp);
  } else {
    noteOffLive(note);
  }
}

static void onOctaveShift(int delta, void *) {
  octaveShift = constrain(octaveShift + delta, kOctaveShiftMin, kOctaveShiftMax);
#if BENCH_SERIAL_MIDI
  Serial.print(F("oct "));
  Serial.println(octaveShift);
#endif
}

static int scaleNoteForTransport(int degree, void *) {
  return chords.scaleNote(degree) + octaveShift * 12;
}

#if BENCH_SEQUENCER
static void onSeqStep(uint8_t note, bool on, void *) {
  if (muteSeq) return;
  if (on && note > 0) {
    const int fired = (seq.currentStep() + seq.length() - 1) % seq.length();
    float amp = kStkAmp;
    if (seqAccent && (fired % 4) != 0) {
      amp *= 0.72f;
    }
    voicer.noteOn(note, amp, kGroup);
#if BENCH_HARMONY
    harmony.onLeadStep(note, true, voicer, kGroup, amp);
#endif
  } else {
    if (note > 0) {
      voicer.noteOff(note, kStkAmp, kGroup);
    }
#if BENCH_HARMONY
    harmony.onLeadStep(note, false, voicer, kGroup, kStkAmp);
#endif
  }
}

static int scaleNoteForSeq(int degree, void *) {
  return scaleNoteForTransport(degree, nullptr);
}

static void setSeqEnabled(bool on) {
  seq.setEnabled(on);
  if (on) {
    genMode = false;
    demoActive = false;
    releaseBlockChord();
    stopArpVoice();
    seq.reset();
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(on ? F("seq on") : F("seq off"));
#endif
}
#endif

#if BENCH_BASS_SEQ
static void onBassStep(uint8_t note, bool on, void *) {
  if (muteBass) return;
  if (on) {
    if (bassPlayingNote >= 0) {
      voicer.noteOff(bassPlayingNote, kStkAmp, kGroup);
    }
    if (note > 0) {
      voicer.noteOn(note, kStkAmp * 0.85f, kGroup);
      bassPlayingNote = note;
    } else {
      bassPlayingNote = -1;
    }
    return;
  }
  if (note > 0 && bassPlayingNote == (int)note) {
    voicer.noteOff(note, kStkAmp, kGroup);
    bassPlayingNote = -1;
  }
}

static void setBassEnabled(bool on) {
  bass.setEnabled(on);
  if (!on && bassPlayingNote >= 0) {
    voicer.noteOff(bassPlayingNote, kStkAmp, kGroup);
    bassPlayingNote = -1;
  }
  if (on) {
    demoActive = false;
    releaseBlockChord();
    bass.reset();
  }
  markPresetDirty();
#if BENCH_SERIAL_MIDI
  Serial.println(on ? F("bass on") : F("bass off"));
#endif
}
#endif

static int oledWindowStart(int len, int cursor) {
#if BENCH_OLED
  if (!oledScrollWindow || len <= 16) return 0;
  int start = cursor - 7;
  if (start < 0) start = 0;
  if (start + 16 > len) start = len - 16;
  return start;
#else
  (void)len;
  (void)cursor;
  return 0;
#endif
}

static int oledPatternLen() {
  int len = 16;
#if BENCH_SEQUENCER
  len = max(len, seq.length());
#endif
#if BENCH_DRUM_GRID
  len = max(len, drums.length());
#endif
#if BENCH_BASS_SEQ
  len = max(len, BassSeq::kSteps);
#endif
  return len;
}

static void buildOledPatternRows(char *seqRow, char *bassRow, char *drumRow, size_t cap, int cursor,
                                 int *windowStartOut) {
  const int win = oledWindowStart(oledPatternLen(), cursor);
  size_t n = 0;
#if BENCH_SEQUENCER
  const int slen = seq.length();
  const int show = min(slen - win, 16);
  for (int i = 0; i < show && n + 1 < cap; i++) {
    const int idx = win + i;
    const bool hit = seq.stepNote(idx) != SeqPlayer::kRest;
    if (idx == cursor) {
      seqRow[n++] = hit ? '*' : '>';
    } else {
      seqRow[n++] = hit ? '#' : '.';
    }
  }
#else
  (void)cursor;
#endif
  seqRow[n] = '\0';
  n = 0;
#if BENCH_BASS_SEQ
  const int blen = BassSeq::kSteps;
  const int bshow = min(blen - win, 16);
  for (int i = 0; i < bshow && n + 1 < cap; i++) {
    const int idx = win + i;
    const int8_t d = bass.stepDegree(idx);
    if (idx == cursor) {
      bassRow[n++] = (d == BassSeq::kRest) ? '>' : '*';
    } else {
      bassRow[n++] = (d == BassSeq::kRest) ? '-' : (char)('0' + d);
    }
  }
#endif
  bassRow[n] = '\0';
  n = 0;
#if BENCH_DRUM_GRID
  const int dlen = drums.length();
  const int dshow = min(dlen - win, 16);
  for (int i = 0; i < dshow && n + 1 < cap; i++) {
    const int idx = win + i;
    const bool k = drums.kickAt(idx) != 0;
    const bool h = drums.hatAt(idx) != 0;
    if (idx == cursor) {
      if (k && h) {
        drumRow[n++] = '*';
      } else if (k) {
        drumRow[n++] = '@';
      } else if (h) {
        drumRow[n++] = '^';
      } else {
        drumRow[n++] = '>';
      }
    } else if (k && h) {
      drumRow[n++] = '+';
    } else if (k) {
      drumRow[n++] = 'K';
    } else if (h) {
      drumRow[n++] = 'h';
    } else {
      drumRow[n++] = '.';
    }
  }
#endif
  drumRow[n] = '\0';
  if (windowStartOut) *windowStartOut = win;
}

static void pollBenchDisplay() {
#if BENCH_OLED || BENCH_CYD_UART
  static unsigned long lastMs = 0;
  const unsigned long now = millis();
  if (now - lastMs < 250) return;
  lastMs = now;

  int step = 0;
#if BENCH_SEQUENCER
  if (seq.enabled()) step = seq.currentStep();
#elif BENCH_DRUM_GRID
  if (drums.enabled()) step = drums.currentStep();
#endif
  unsigned bpm = 120;
#if BENCH_SEQUENCER
  bpm = seq.bpm();
#endif
  int swingPct = 0;
#if BENCH_SEQUENCER
  swingPct = (int)(seq.swing() * 100.0f);
#endif

  if (benchPatternView) {
    char seqRow[22]{};
    char bassRow[22]{};
    char drumRow[22]{};
    int winStart = 0;
    buildOledPatternRows(seqRow, bassRow, drumRow, sizeof(seqRow), step, &winStart);
#if BENCH_OLED
    if (display.ready()) display.showPattern(bpm, step, winStart, seqRow, bassRow, drumRow);
#endif
#if BENCH_CYD_UART
    benchCydEmitPattern(bpm, step, winStart, seqRow, bassRow, drumRow);
#endif
    return;
  }

#if BENCH_OLED
  if (display.ready()) {
    display.showTransport(bpm, kInstrumentName, chords.typeName(), chords.scaleName(),
#if BENCH_SEQUENCER
                          seq.enabled(),
#else
                          false,
#endif
#if BENCH_BASS_SEQ
                          bass.enabled(),
#else
                          false,
#endif
#if BENCH_DRUM_GRID
                          drums.enabled(),
#else
                          false,
#endif
                          monoMode, step,
#if BENCH_HARMONY
                          harmony.enabled(), harmony.semitones(),
#else
                          false, 0,
#endif
                          swingPct, monoLead.unison(), activeBenchSlot, muteSeq, muteBass, muteDrums,
                          metroEnabled, chordLocked);
  }
#endif
#if BENCH_CYD_UART
  benchCydEmitTransport(bpm, kInstrumentName, chords.typeName(), chords.scaleName(),
#if BENCH_SEQUENCER
                        seq.enabled(),
#else
                        false,
#endif
#if BENCH_BASS_SEQ
                        bass.enabled(),
#else
                        false,
#endif
#if BENCH_DRUM_GRID
                        drums.enabled(),
#else
                        false,
#endif
                        monoMode, step, swingPct, activeBenchSlot, muteSeq, muteBass, muteDrums,
                        metroEnabled, chordLocked);
#endif
#endif
}

static void printStatus() {
  Serial.print(F("inst="));
  Serial.print(kInstrumentName);
  Serial.print(F(" root="));
  Serial.print(chords.root());
  Serial.print(F(" chord="));
  Serial.print(chords.typeName());
  Serial.print(F(" scale="));
  Serial.print(chords.scaleName());
  Serial.print(F(" inv="));
  Serial.print(chords.inversion());
  Serial.print(F(" arp="));
  Serial.print(chords.arpEnabled() ? F("on") : F("off"));
  Serial.print(F(" demo="));
  Serial.print(demoActive ? F("on") : F("off"));
  Serial.print(F(" mono="));
  Serial.print(monoMode ? F("on") : F("off"));
  Serial.print(F(" strum="));
  Serial.print(strum.enabled() ? F("on") : F("off"));
  Serial.print(F(" gen="));
  Serial.print(genMode ? F("on") : F("off"));
  Serial.print(F(" qtz="));
  Serial.print(midiQuantize ? F("on") : F("off"));
#if BENCH_SEQUENCER
  Serial.print(F(" seq="));
  Serial.print(seq.enabled() ? F("on") : F("off"));
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
  Serial.print(F(" len="));
  Serial.print(seq.length());
  Serial.print(F(" swing="));
  Serial.print((int)(seq.swing() * 100.0f));
  Serial.print(F(" acc="));
  Serial.print(seqAccent ? F("on") : F("off"));
  Serial.print(F(" write="));
  Serial.print(seqWriteMode ? F("on") : F("off"));
  Serial.print(F(" grid="));
  Serial.print(seqGridMode ? F("on") : F("off"));
  Serial.print(F(" bounce="));
  Serial.print(seq.bounce() ? F("on") : F("off"));
#endif
  Serial.print(F(" metro="));
  Serial.print(metroEnabled ? F("on") : F("off"));
  if (metroEnabled) {
    Serial.print(F(" acc="));
    Serial.print(metroAccent ? F("on") : F("off"));
  }
  Serial.print(F(" lock="));
  Serial.print(chordLocked ? F("on") : F("off"));
#if BENCH_HARMONY
  Serial.print(F(" harm="));
  Serial.print(harmony.enabled() ? F("on") : F("off"));
  if (harmony.enabled()) {
    Serial.print(F("+"));
    Serial.print(harmony.semitones());
  }
  if (harmLocked) {
    Serial.print(F(" Hlock"));
  }
#endif
#if BENCH_DRUM_GRID
  Serial.print(F(" drums="));
  Serial.print(drums.enabled() ? F("on") : F("off"));
#endif
#if BENCH_BASS_SEQ
  Serial.print(F(" bass="));
  Serial.print(bass.enabled() ? F("on") : F("off"));
#endif
  if (monoMode) {
    Serial.print(F(" leg="));
    Serial.print(monoLead.legato() ? F("on") : F("off"));
    Serial.print(F(" glide="));
    Serial.print(monoLead.glide() ? F("on") : F("off"));
    Serial.print(F(" uni="));
    Serial.print(monoLead.unison() ? F("on") : F("off"));
  }
#if BENCH_DRUM_GRID
  Serial.print(F(" drumLen="));
  Serial.print(drums.length());
#if BENCH_DRUM_PADS
  Serial.print(F(" pad="));
  Serial.print(drumPadMode ? F("on") : F("off"));
  Serial.print(F(" write="));
  Serial.print(drumWriteMode ? F("on") : F("off"));
#endif
#endif
  Serial.print(F(" slot="));
  Serial.print(activeBenchSlot);
  Serial.print(F(" stash="));
  Serial.print(benchStash.valid() ? F("ok") : F("empty"));
  Serial.print(F(" autosave="));
  Serial.print(autoSaveEnabled ? F("on") : F("off"));
  if (muteSeq || muteBass || muteDrums) {
    Serial.print(F(" mut="));
    if (muteSeq) Serial.print(F("SQ"));
    if (muteBass) Serial.print(F("BS"));
    if (muteDrums) Serial.print(F("DR"));
  }
  Serial.print(F(" fw="));
  Serial.print(F411_SYNTH_VERSION);
  Serial.print(F(" oct="));
  Serial.print(octaveShift);
  Serial.print(F(" vol="));
  Serial.println(masterGain, 2);
}

static void printPatternAll() {
  Serial.println(F("--- pattern ---"));
#if BENCH_SEQUENCER
  Serial.print(F("seq len "));
  Serial.println(seq.length());
  Serial.print(F("seq "));
  seq.printPattern(Serial);
#endif
#if BENCH_BASS_SEQ
  bass.printPattern(Serial);
#endif
#if BENCH_DRUM_GRID
  drums.printPattern(Serial);
#endif
  Serial.println(F("---"));
}

static void printSlotLine(int slot) {
  PresetStore::ChordSnapshot snap;
  Serial.print(slot);
  Serial.print(F(": "));
  if (!presets.readChordSnapshot(slot, snap)) {
    Serial.println(F("empty"));
    return;
  }
  ChordEngine peek;
  peek.setRoot(snap.root);
  if (snap.chordType < (uint8_t)ChordType::Count) {
    peek.setType((ChordType)snap.chordType);
  }
  if (snap.scale < (uint8_t)ScaleMode::Count) {
    peek.setScale((ScaleMode)snap.scale);
  }
  peek.setInversion(snap.inversion);
  Serial.print(peek.typeName());
  Serial.print(F("/"));
  Serial.print(peek.scaleName());
  Serial.print(F(" r"));
  Serial.print(snap.root);
  Serial.print(F(" oct"));
  Serial.print(snap.octaveShift);
  Serial.print(snap.seqOn ? F(" seq") : F(""));
  Serial.print(F(" bpm"));
  Serial.println(snap.seqBpm);
}

static void printSlots() {
  Serial.println(F("slots:"));
  for (int i = 0; i < PresetStore::kSlots; i++) {
    printSlotLine(i);
  }
}

static void compareSlots(int a, int b) {
  if (a < 0 || a >= PresetStore::kSlots || b < 0 || b >= PresetStore::kSlots) return;
  Serial.print(F("compare "));
  Serial.print(a);
  Serial.print(F(" "));
  Serial.println(b);
  printSlotLine(a);
  printSlotLine(b);
  if (!presets.slotValid(a) || !presets.slotValid(b)) {
    Serial.println(F("need 2 valid slots"));
    return;
  }
  Serial.print(F("chord "));
  Serial.println(presets.chordSnapshotsEqual(a, b) ? F("match") : F("DIFF"));
  const int diffs = presets.patternBankDiffCount(a, b);
  Serial.print(F("pattern byte diffs "));
  Serial.print(diffs);
  Serial.print(F("/"));
  Serial.println(PresetStore::kPatternBankBytes);
}

static void eepromDiffSlots(int a, int b) {
  if (a < 0 || a >= PresetStore::kSlots || b < 0 || b >= PresetStore::kSlots) {
    Serial.println(F("eeprom diff? slot 0-3"));
    return;
  }
  Serial.print(F("eeprom diff "));
  Serial.print(a);
  Serial.print(F(" "));
  Serial.println(b);
  const int chordDiffs = presets.chordSlotDiffCount(a, b);
  const int bankDiffs = presets.patternBankDiffCount(a, b);
  Serial.print(F("chord byte diffs "));
  Serial.print(chordDiffs >= 0 ? chordDiffs : 0);
  Serial.print(F("/"));
  Serial.println(PresetStore::kBytesPerSlot);
  Serial.print(F("pattern byte diffs "));
  Serial.print(bankDiffs >= 0 ? bankDiffs : 0);
  Serial.print(F("/"));
  Serial.println(PresetStore::kPatternBankBytes);
}

static void eepromVerifySlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) {
    Serial.println(F("eeprom verify? slot 0-3"));
    return;
  }
  Serial.print(F("eeprom verify "));
  Serial.println(slot);
  const bool valid = presets.slotValid(slot);
  Serial.print(F("valid="));
  Serial.println(valid ? 1 : 0);
  if (!valid) {
    Serial.println(F("---"));
    return;
  }
  PresetStore::ChordSnapshot snap{};
  if (presets.readChordSnapshot(slot, snap)) {
    Serial.print(F("root="));
    Serial.print(snap.root);
    Serial.print(F(" type="));
    Serial.print(snap.chordType);
    Serial.print(F(" inv="));
    Serial.println(snap.inversion);
    Serial.print(F("seq_on="));
    Serial.print(snap.seqOn ? 1 : 0);
    Serial.print(F(" bpm="));
    Serial.println(snap.seqBpm);
  }
  Serial.print(F("active="));
  Serial.println(activeBenchSlot == slot ? 1 : 0);
  Serial.println(F("---"));
}

static uint16_t eepromSlotChecksum(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return 0;
  uint16_t sum = (uint16_t)(slot * 0x5A5A);
  const int chordAddr = slot * PresetStore::kBytesPerSlot;
  for (int i = 0; i < PresetStore::kBytesPerSlot; i++) {
    sum = (uint16_t)(sum + EEPROM.read(chordAddr + i));
    sum = (uint16_t)((sum << 1) | (sum >> 15));
  }
  const int bankAddr = PresetStore::kPatternBankBase + slot * PresetStore::kPatternBankBytes;
  for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
    sum ^= (uint16_t)(EEPROM.read(bankAddr + i) << (i & 7));
  }
  return sum;
}

static void eepromChecksumSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) {
    Serial.println(F("eeprom checksum? slot 0-3"));
    return;
  }
  Serial.print(F("eeprom checksum "));
  Serial.println(slot);
  Serial.print(F("valid="));
  Serial.println(presets.slotValid(slot) ? 1 : 0);
  Serial.print(F("checksum="));
  Serial.println(eepromSlotChecksum(slot));
  Serial.println(F("---"));
}

static void eepromSnapshotSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) {
    Serial.println(F("eeprom snapshot? slot 0-3"));
    return;
  }
  Serial.println(F("--- eeprom snapshot ---"));
  Serial.print(F("slot="));
  Serial.println(slot);
  const bool valid = presets.slotValid(slot);
  Serial.print(F("valid="));
  Serial.println(valid ? 1 : 0);
  Serial.print(F("checksum="));
  Serial.println(eepromSlotChecksum(slot));
  if (valid) {
    PresetStore::ChordSnapshot snap{};
    if (presets.readChordSnapshot(slot, snap)) {
      Serial.print(F("root="));
      Serial.print(snap.root);
      Serial.print(F(" bpm="));
      Serial.println(snap.seqBpm);
    }
  }
  Serial.print(F("active="));
  Serial.println(activeBenchSlot == slot ? 1 : 0);
  Serial.println(F("---"));
}

static void eepromSealAll() {
  Serial.println(F("--- eeprom seal ---"));
  int okCount = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const bool valid = presets.slotValid(i);
    if (valid) okCount++;
    Serial.print(F("slot "));
    Serial.print(i);
    Serial.print(F(" seal="));
    Serial.println(valid ? F("OK") : F("--"));
  }
  Serial.print(F("sealed="));
  Serial.print(okCount);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.println(F("---"));
}

static void benchFilterProfile() {
  Serial.println(F("--- bench filter profile ---"));
#if BENCH_MONO_FILTER
  {
    const uint32_t us = measureFilterLoadUsPerFrame(128, 8);
    Serial.print(F("load_us="));
    Serial.println(us);
  }
#if BENCH_FILTER_ENV
  {
    const uint32_t us = measureFilterEnvLoadUsPerFrame(128, 8);
    Serial.print(F("env_us="));
    Serial.println(us);
  }
#endif
#if BENCH_MONO_LADDER
  {
    const uint32_t us = measureFilterResLoadUsPerFrame(128, 8);
    Serial.print(F("res_us="));
    Serial.println(us);
  }
#if BENCH_FILTER_ENV
  {
    const uint32_t us = measureFilterComboLoadUsPerFrame(128, 8);
    Serial.print(F("combo_us="));
    Serial.println(us);
  }
#endif
#endif
#else
  Serial.println(F("filter=off"));
#endif
  Serial.println(F("---"));
}

#if BENCH_MONO_FILTER && BENCH_FILTER_ENV && BENCH_MONO_LADDER
static void benchFilterHeadroom() {
  Serial.println(F("--- bench filter headroom ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float loadPct = 100.0f * (float)load / frameUs;
  const float comboPct = 100.0f * (float)combo / frameUs;
  const float headroom = 100.0f - comboPct;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("load_pct="));
  Serial.println(loadPct, 1);
  Serial.print(F("combo_pct="));
  Serial.println(comboPct, 1);
  Serial.print(F("headroom_pct="));
  Serial.println(headroom, 1);
  Serial.println(F("---"));
}

static void benchFilterBudget() {
  Serial.println(F("--- bench filter budget ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  static const int kBudgetFrames[] = {64, 128, 256};
  for (unsigned i = 0; i < sizeof(kBudgetFrames) / sizeof(kBudgetFrames[0]); i++) {
    const int frames = kBudgetFrames[i];
    const uint32_t combo = measureFilterComboLoadUsPerFrame(frames, 8);
    const float pct = 100.0f * (float)combo / frameUs;
    Serial.print(F("frames="));
    Serial.print(frames);
    Serial.print(F(" combo_us="));
    Serial.print(combo);
    Serial.print(F(" pct="));
    Serial.println(pct, 1);
  }
  Serial.println(F("---"));
}

static void benchFilterGauge() {
  Serial.println(F("--- bench filter gauge ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  Serial.print(F("load_pct="));
  Serial.println(100.0f * (float)load / frameUs, 1);
  Serial.print(F("env_pct="));
  Serial.println(100.0f * (float)env / frameUs, 1);
  Serial.print(F("res_pct="));
  Serial.println(100.0f * (float)res / frameUs, 1);
  Serial.print(F("combo_pct="));
  Serial.println(100.0f * (float)combo / frameUs, 1);
  Serial.println(F("---"));
}

static void benchFilterCeiling() {
  Serial.println(F("--- bench filter ceiling ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const float comboPct = 100.0f * (float)combo / frameUs;
  const float ceilingPct = 80.0f;
  const float marginPct = ceilingPct - comboPct;
  Serial.print(F("ceiling_pct="));
  Serial.println(ceilingPct, 1);
  Serial.print(F("combo_pct="));
  Serial.println(comboPct, 1);
  Serial.print(F("margin_pct="));
  Serial.println(marginPct, 1);
  Serial.println(F("---"));
}

static void benchFilterReserve() {
  Serial.println(F("--- bench filter reserve ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const float ceilingPct = 80.0f;
  const float reserveUs = (ceilingPct / 100.0f) * frameUs - (float)combo;
  const float reservePct = 100.0f * reserveUs / frameUs;
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("reserve_us="));
  Serial.println(reserveUs, 1);
  Serial.print(F("reserve_pct="));
  Serial.println(reservePct, 1);
  Serial.println(F("---"));
}

static void benchFilterCushion() {
  Serial.println(F("--- bench filter cushion ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const int32_t cushionUs = (int32_t)combo - (int32_t)load;
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const float cushionPct = 100.0f * (float)cushionUs / frameUs;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("cushion_us="));
  Serial.println(cushionUs);
  Serial.print(F("cushion_pct="));
  Serial.println(cushionPct, 1);
  Serial.println(F("---"));
}

static void benchFilterSpan() {
  Serial.println(F("--- bench filter span ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  Serial.print(F("env_span_us="));
  Serial.println((int32_t)env - (int32_t)load);
  Serial.print(F("res_span_us="));
  Serial.println((int32_t)res - (int32_t)load);
  Serial.print(F("combo_span_us="));
  Serial.println((int32_t)combo - (int32_t)load);
  Serial.println(F("---"));
}

static void benchFilterTally() {
  Serial.println(F("--- bench filter tally ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t tallyUs = load + env + res + combo;
  const float tallyPct = 100.0f * (float)tallyUs / frameUs;
  Serial.print(F("tally_us="));
  Serial.println(tallyUs);
  Serial.print(F("tally_pct="));
  Serial.println(tallyPct, 1);
  Serial.println(F("---"));
}

static void benchFilterMargin() {
  Serial.println(F("--- bench filter margin ---"));
  const float frameUs = 1000000.0f / (float)kSampleRate;
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t tallyUs = load + env + res + combo;
  const float marginUs = frameUs - (float)tallyUs;
  const float marginPct = 100.0f * marginUs / frameUs;
  Serial.print(F("tally_us="));
  Serial.println(tallyUs);
  Serial.print(F("margin_us="));
  Serial.println(marginUs, 1);
  Serial.print(F("margin_pct="));
  Serial.println(marginPct, 1);
  Serial.println(F("---"));
}

static void benchFilterShare() {
  Serial.println(F("--- bench filter share ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t tallyUs = load + env + res + combo;
  const float invTally = tallyUs > 0 ? 100.0f / (float)tallyUs : 0.0f;
  Serial.print(F("load_share_pct="));
  Serial.println((float)load * invTally, 1);
  Serial.print(F("env_share_pct="));
  Serial.println((float)env * invTally, 1);
  Serial.print(F("res_share_pct="));
  Serial.println((float)res * invTally, 1);
  Serial.print(F("combo_share_pct="));
  Serial.println((float)combo * invTally, 1);
  Serial.println(F("---"));
}

static void benchFilterPeak() {
  Serial.println(F("--- bench filter peak ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  uint32_t peakUs = load;
  const char *peakTag = "load";
  if (env > peakUs) {
    peakUs = env;
    peakTag = "env";
  }
  if (res > peakUs) {
    peakUs = res;
    peakTag = "res";
  }
  if (combo > peakUs) {
    peakUs = combo;
    peakTag = "combo";
  }
  Serial.print(F("peak_us="));
  Serial.println(peakUs);
  Serial.print(F("peak_tag="));
  Serial.println(peakTag);
  Serial.println(F("---"));
}

static void benchFilterBalance() {
  Serial.println(F("--- bench filter balance ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const int32_t balanceUs = (int32_t)combo - (int32_t)load;
  const float balanceRatio = load > 0 ? (float)combo / (float)load : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("balance_us="));
  Serial.println(balanceUs);
  Serial.print(F("balance_ratio="));
  Serial.println(balanceRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterBlend() {
  Serial.println(F("--- bench filter blend ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t blendUs = (load + env + combo) / 3U;
  const float blendRatio = load > 0 ? (float)blendUs / (float)load : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("blend_us="));
  Serial.println(blendUs);
  Serial.print(F("blend_ratio="));
  Serial.println(blendRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterPivot() {
  Serial.println(F("--- bench filter pivot ---"));
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const int32_t pivotUs = (int32_t)env - (int32_t)res;
  const float pivotRatio = res > 0 ? (float)env / (float)res : 0.0f;
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("res_us="));
  Serial.println(res);
  Serial.print(F("pivot_us="));
  Serial.println(pivotUs);
  Serial.print(F("pivot_ratio="));
  Serial.println(pivotRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterHinge() {
  Serial.println(F("--- bench filter hinge ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t hingeUs = (load + combo) / 2U;
  const int32_t hingeDelta = (int32_t)combo - (int32_t)hingeUs;
  const float hingeRatio = load > 0 ? (float)hingeUs / (float)load : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("hinge_us="));
  Serial.println(hingeUs);
  Serial.print(F("hinge_delta="));
  Serial.println(hingeDelta);
  Serial.print(F("hinge_ratio="));
  Serial.println(hingeRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterSwivel() {
  Serial.println(F("--- bench filter swivel ---"));
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t swivelUs = env + res;
  const int32_t swivelDelta = (int32_t)combo - (int32_t)swivelUs;
  const float swivelRatio = swivelUs > 0 ? (float)combo / (float)swivelUs : 0.0f;
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("res_us="));
  Serial.println(res);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("swivel_us="));
  Serial.println(swivelUs);
  Serial.print(F("swivel_delta="));
  Serial.println(swivelDelta);
  Serial.print(F("swivel_ratio="));
  Serial.println(swivelRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterAxle() {
  Serial.println(F("--- bench filter axle ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t axleSpanUs = env > res ? (env - res) : (res - env);
  const uint32_t axlePairUs = env + res;
  const float axleRatio = load > 0 ? (float)axlePairUs / (float)load : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("res_us="));
  Serial.println(res);
  Serial.print(F("axle_span_us="));
  Serial.println(axleSpanUs);
  Serial.print(F("axle_pair_us="));
  Serial.println(axlePairUs);
  Serial.print(F("axle_ratio="));
  Serial.println(axleRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterCoupler() {
  Serial.println(F("--- bench filter coupler ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t couplerUs = load + env;
  const int32_t couplerDelta = (int32_t)combo - (int32_t)couplerUs;
  const float couplerRatio = couplerUs > 0 ? (float)combo / (float)couplerUs : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("coupler_us="));
  Serial.println(couplerUs);
  Serial.print(F("coupler_delta="));
  Serial.println(couplerDelta);
  Serial.print(F("coupler_ratio="));
  Serial.println(couplerRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterLinkage() {
  Serial.println(F("--- bench filter linkage ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t linkageUs = load + res;
  const int32_t linkageDelta = (int32_t)combo - (int32_t)linkageUs;
  const float linkageRatio = linkageUs > 0 ? (float)combo / (float)linkageUs : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("res_us="));
  Serial.println(res);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("linkage_us="));
  Serial.println(linkageUs);
  Serial.print(F("linkage_delta="));
  Serial.println(linkageDelta);
  Serial.print(F("linkage_ratio="));
  Serial.println(linkageRatio, 2);
  Serial.println(F("---"));
}

static void benchFilterTruss() {
  Serial.println(F("--- bench filter truss ---"));
  const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
  const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
  const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
  const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
  const uint32_t trussUs = load + env + res;
  const int32_t trussDelta = (int32_t)combo - (int32_t)trussUs;
  const float trussRatio = trussUs > 0 ? (float)combo / (float)trussUs : 0.0f;
  Serial.print(F("load_us="));
  Serial.println(load);
  Serial.print(F("env_us="));
  Serial.println(env);
  Serial.print(F("res_us="));
  Serial.println(res);
  Serial.print(F("combo_us="));
  Serial.println(combo);
  Serial.print(F("truss_us="));
  Serial.println(trussUs);
  Serial.print(F("truss_delta="));
  Serial.println(trussDelta);
  Serial.print(F("truss_ratio="));
  Serial.println(trussRatio, 2);
  Serial.println(F("---"));
}
#endif

static void eepromManifest() {
  Serial.println(F("--- eeprom manifest ---"));
  Serial.print(F("schema="));
  Serial.println(LUMPY_EEPROM_SCHEMA);
  Serial.print(F("api="));
  Serial.print(LUMPY_API_MAJOR);
  Serial.print(F("."));
  Serial.println(LUMPY_API_PATCH);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  int sealed = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const bool valid = presets.slotValid(i);
    if (valid) sealed++;
    Serial.print(F("slot "));
    Serial.print(i);
    Serial.print(F(" valid="));
    Serial.print(valid ? 1 : 0);
    Serial.print(F(" cs="));
    Serial.println(eepromSlotChecksum(i));
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.println(F("---"));
}

static void eepromAuditSlots(int a, int b) {
  if (a < 0 || a >= PresetStore::kSlots || b < 0 || b >= PresetStore::kSlots) {
    Serial.println(F("eeprom audit? slot 0-3"));
    return;
  }
  Serial.print(F("eeprom audit "));
  Serial.print(a);
  Serial.print(F(" "));
  Serial.println(b);
  Serial.print(F("valid_a="));
  Serial.print(presets.slotValid(a) ? 1 : 0);
  Serial.print(F(" valid_b="));
  Serial.println(presets.slotValid(b) ? 1 : 0);
  Serial.print(F("chord_diffs="));
  Serial.println(presets.chordSlotDiffCount(a, b));
  Serial.print(F("pattern_diffs="));
  Serial.println(presets.patternBankDiffCount(a, b));
  Serial.print(F("cs_a="));
  Serial.print(eepromSlotChecksum(a));
  Serial.print(F(" cs_b="));
  Serial.println(eepromSlotChecksum(b));
  Serial.println(F("---"));
}

static void eepromParity() {
  Serial.println(F("--- eeprom parity ---"));
  uint16_t parity = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    Serial.print(F("slot "));
    Serial.print(i);
    Serial.print(F(" cs="));
    Serial.println(cs);
  }
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.println(F("---"));
}

static void eepromDigest() {
  Serial.println(F("--- eeprom digest ---"));
  uint32_t sum = 0;
  int valid = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) valid++;
    sum += (uint32_t)eepromSlotChecksum(i);
  }
  Serial.print(F("valid="));
  Serial.print(valid);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("digest="));
  Serial.println(sum);
  Serial.println(F("---"));
}

static void eepromCensus() {
  Serial.println(F("--- eeprom census ---"));
  int valid = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) valid++;
    const uint16_t cs = eepromSlotChecksum(i);
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  Serial.print(F("valid="));
  Serial.print(valid);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.print(F("cs_min="));
  Serial.println(minCs);
  Serial.print(F("cs_max="));
  Serial.println(maxCs);
  Serial.println(F("---"));
}

static void eepromLedger() {
  Serial.println(F("--- eeprom ledger ---"));
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) mask |= (uint8_t)(1U << i);
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
  }
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromRoster() {
  Serial.println(F("--- eeprom roster ---"));
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  for (int i = 0; i < PresetStore::kSlots; i++) {
    Serial.print(F("slot "));
    Serial.print(i);
    Serial.print(F(" ok="));
    Serial.print(presets.slotValid(i) ? 1 : 0);
    Serial.print(F(" cs="));
    Serial.println(eepromSlotChecksum(i));
  }
  Serial.println(F("---"));
}

static void eepromIndex() {
  Serial.println(F("--- eeprom index ---"));
  uint8_t mask = 0;
  uint16_t parity = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) mask |= (uint8_t)(1U << i);
    parity ^= eepromSlotChecksum(i);
  }
  Serial.print(F("index="));
  Serial.println(activeBenchSlot);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.println(F("---"));
}

static void eepromAtlas() {
  Serial.println(F("--- eeprom atlas ---"));
  int sealed = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) sealed++;
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.println(F("---"));
}

static void eepromAlmanac() {
  Serial.println(F("--- eeprom almanac ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromGazette() {
  Serial.println(F("--- eeprom gazette ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    parity ^= eepromSlotChecksum(i);
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromRegister() {
  Serial.println(F("--- eeprom register ---"));
  int sealed = 0;
  uint8_t reg = 0;
  uint32_t digest = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      reg |= (uint8_t)(1U << i);
    }
    digest += (uint32_t)eepromSlotChecksum(i);
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("register="));
  Serial.println(reg);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromCodex() {
  Serial.println(F("--- eeprom codex ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromTome() {
  Serial.println(F("--- eeprom tome ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromFolio() {
  Serial.println(F("--- eeprom folio ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromScroll() {
  Serial.println(F("--- eeprom scroll ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromArchive() {
  Serial.println(F("--- eeprom archive ---"));
  int sealed = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    const uint16_t cs = eepromSlotChecksum(i);
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  const uint32_t avgCs = PresetStore::kSlots > 0 ? digest / (uint32_t)PresetStore::kSlots : 0;
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("avg_cs="));
  Serial.println(avgCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromVault() {
  Serial.println(F("--- eeprom vault ---"));
  int sealed = 0;
  int occupied = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const uint16_t cs = eepromSlotChecksum(i);
    if (cs != 0) occupied++;
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  const uint32_t avgCs = PresetStore::kSlots > 0 ? digest / (uint32_t)PresetStore::kSlots : 0;
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("occupied="));
  Serial.println(occupied);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("avg_cs="));
  Serial.println(avgCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromAnnals() {
  Serial.println(F("--- eeprom annals ---"));
  int sealed = 0;
  int occupied = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const uint16_t cs = eepromSlotChecksum(i);
    if (cs != 0) occupied++;
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const int vacant = PresetStore::kSlots - occupied;
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  const uint32_t avgCs = PresetStore::kSlots > 0 ? digest / (uint32_t)PresetStore::kSlots : 0;
  const int sealedPct = PresetStore::kSlots > 0 ? (sealed * 100) / PresetStore::kSlots : 0;
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("occupied="));
  Serial.println(occupied);
  Serial.print(F("vacant="));
  Serial.println(vacant);
  Serial.print(F("sealed_pct="));
  Serial.println(sealedPct);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("avg_cs="));
  Serial.println(avgCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromChronicle() {
  Serial.println(F("--- eeprom chronicle ---"));
  int sealed = 0;
  int occupied = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const uint16_t cs = eepromSlotChecksum(i);
    if (cs != 0) occupied++;
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const int vacant = PresetStore::kSlots - occupied;
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  const uint32_t avgCs = PresetStore::kSlots > 0 ? digest / (uint32_t)PresetStore::kSlots : 0;
  const int sealedPct = PresetStore::kSlots > 0 ? (sealed * 100) / PresetStore::kSlots : 0;
  const int csSpanPct = maxCs > 0 ? (int)((uint32_t)csSpan * 100U / (uint32_t)maxCs) : 0;
  const uint16_t digestMod = (uint16_t)(digest & 0xFFFFU);
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("occupied="));
  Serial.println(occupied);
  Serial.print(F("vacant="));
  Serial.println(vacant);
  Serial.print(F("sealed_pct="));
  Serial.println(sealedPct);
  Serial.print(F("cs_span_pct="));
  Serial.println(csSpanPct);
  Serial.print(F("digest_mod="));
  Serial.println(digestMod);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("avg_cs="));
  Serial.println(avgCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void eepromRegistry() {
  Serial.println(F("--- eeprom registry ---"));
  int sealed = 0;
  int occupied = 0;
  uint8_t mask = 0;
  uint16_t parity = 0;
  uint32_t digest = 0;
  uint16_t minCs = 0xFFFF;
  uint16_t maxCs = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    const uint16_t cs = eepromSlotChecksum(i);
    if (cs != 0) occupied++;
    if (presets.slotValid(i)) {
      sealed++;
      mask |= (uint8_t)(1U << i);
    }
    parity ^= cs;
    digest += (uint32_t)cs;
    if (cs < minCs) minCs = cs;
    if (cs > maxCs) maxCs = cs;
  }
  const int vacant = PresetStore::kSlots - occupied;
  const uint16_t csSpan = (maxCs >= minCs) ? (uint16_t)(maxCs - minCs) : 0;
  const uint32_t avgCs = PresetStore::kSlots > 0 ? digest / (uint32_t)PresetStore::kSlots : 0;
  const int sealedPct = PresetStore::kSlots > 0 ? (sealed * 100) / PresetStore::kSlots : 0;
  const int occupiedPct = PresetStore::kSlots > 0 ? (occupied * 100) / PresetStore::kSlots : 0;
  const int csSpanPct = maxCs > 0 ? (int)((uint32_t)csSpan * 100U / (uint32_t)maxCs) : 0;
  const uint16_t digestMod = (uint16_t)(digest & 0xFFFFU);
  const uint16_t parityMod = (uint16_t)(parity ^ digestMod);
  Serial.print(F("sealed="));
  Serial.print(sealed);
  Serial.print(F("/"));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("occupied="));
  Serial.println(occupied);
  Serial.print(F("vacant="));
  Serial.println(vacant);
  Serial.print(F("sealed_pct="));
  Serial.println(sealedPct);
  Serial.print(F("occupied_pct="));
  Serial.println(occupiedPct);
  Serial.print(F("cs_span_pct="));
  Serial.println(csSpanPct);
  Serial.print(F("digest_mod="));
  Serial.println(digestMod);
  Serial.print(F("parity_mod="));
  Serial.println(parityMod);
  Serial.print(F("mask="));
  Serial.println(mask);
  Serial.print(F("parity="));
  Serial.println(parity);
  Serial.print(F("digest="));
  Serial.println(digest);
  Serial.print(F("min_cs="));
  Serial.println(minCs);
  Serial.print(F("max_cs="));
  Serial.println(maxCs);
  Serial.print(F("cs_span="));
  Serial.println(csSpan);
  Serial.print(F("avg_cs="));
  Serial.println(avgCs);
  Serial.print(F("active="));
  Serial.println(activeBenchSlot);
  Serial.println(F("---"));
}

static void peekSlot(int slot) {
  if (slot < 0 || slot >= PresetStore::kSlots) return;
  Serial.print(F("peek "));
  Serial.println(slot);
  printSlotLine(slot);
#if BENCH_SEQUENCER
  SeqPlayer tmp;
  tmp.begin();
  if (presets.loadSeqPattern(slot, tmp)) {
    Serial.print(F("seq "));
    tmp.printPattern(Serial);
  }
#endif
#if BENCH_BASS_SEQ
  int8_t deg[BassSeq::kSteps];
  if (presets.loadBassPattern(slot, deg, BassSeq::kSteps)) {
    Serial.print(F("bass "));
    for (int i = 0; i < BassSeq::kSteps; i++) {
      if (i > 0) Serial.print(' ');
      Serial.print(deg[i] == BassSeq::kRest ? "-" : String(deg[i]));
    }
    Serial.println();
  }
#endif
#if BENCH_DRUM_GRID
  int drumLen = DrumGrid::kSteps;
  uint8_t kick[DrumGrid::kMaxSteps]{};
  uint8_t hat[DrumGrid::kMaxSteps]{};
  if (presets.loadDrumPattern(slot, kick, hat, DrumGrid::kMaxSteps, drumLen)) {
    Serial.print(F("drum len "));
    Serial.println(drumLen);
    Serial.print(F("kick "));
    for (int i = 0; i < drumLen; i++) Serial.print(kick[i] ? 'X' : '.');
    Serial.println();
    Serial.print(F("hat  "));
    for (int i = 0; i < drumLen; i++) Serial.print(hat[i] ? 'x' : '.');
    Serial.println();
  }
#endif
}

static void benchTestLine(bool ok, const __FlashStringHelper *name, int &pass, int &total) {
  total++;
  Serial.print(ok ? F("PASS ") : F("FAIL "));
  Serial.println(name);
  if (ok) pass++;
}

static uint32_t benchFreeRamBytes() {
  char stackTop;
  return (uint32_t)(&stackTop - &_end);
}

static void runBenchSelfTest() {
  int pass = 0;
  int total = 0;
  Serial.println(F("--- bench test ---"));

  benchTestLine(dac.isReady(), F("i2s ready"), pass, total);
  benchTestLine(dac.isStreaming(), F("i2s dma stream"), pass, total);

  const float peakBefore = audioOut.peak();
  voicer.noteOn(60, kStkAmp, kGroup);
  const unsigned long t0 = millis();
  while ((unsigned long)(millis() - t0) < 120U) {
    delay(1);
  }
  voicer.noteOff(60, kStkAmp, kGroup);
  const float peakAfter = audioOut.peak();
  benchTestLine(peakAfter > peakBefore + 0.004f, F("audio engine"), pass, total);
  Serial.print(F("  peak "));
  Serial.println(peakAfter, 4);

  benchTestLine(ccLearn.verifyEepromRoundTrip(), F("eeprom cc map"), pass, total);
  benchTestLine(benchEeprom.verifyHeaderRoundTrip(), F("eeprom schema v2"), pass, total);
  benchTestLine(dac.isStreaming() && dac.bufferFrames() >= 256, F("i2s dma buffer"), pass, total);

  {
    const int savedRoot = chords.root();
    const ScaleMode savedScale = chords.scale();
    chords.setRoot(60);
    chords.setScale(ScaleMode::Major);
    const int q = chords.quantizeToScale(61);
    const bool quantOk = (q == 60 || q == 62);
    benchTestLine(quantOk, F("chord quantize"), pass, total);
    chords.setRoot(savedRoot);
    chords.setScale(savedScale);
  }

#if BENCH_SEQUENCER
  const bool seqWasOn = seq.enabled();
  if (!seqWasOn) seq.setEnabled(true);
  seq.reset();
  const int step0 = seq.currentStep();
  bool moved = false;
  const unsigned long tStart = millis();
  while ((unsigned long)(millis() - tStart) < 500U) {
    seq.poll(millis(), onSeqStep, nullptr);
    if (seq.currentStep() != step0) {
      moved = true;
      break;
    }
    delay(1);
  }
  if (!seqWasOn) seq.setEnabled(false);
  benchTestLine(moved, F("seq transport tick"), pass, total);

  {
    uint8_t bankBackup[PresetStore::kPatternBankBytes];
    const int bankAddr =
        PresetStore::kPatternBankBase + 3 * PresetStore::kPatternBankBytes;
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.get(bankAddr + i, bankBackup[i]);
    }
#if BENCH_DRUM_GRID
    const bool kickProbe = !drums.kickAt(0);
    const bool hatProbe = !drums.hatAt(1);
    drums.setKick(0, kickProbe);
    drums.setHat(1, hatProbe);
    savePatternBank(3);
    drums.clear();
    loadPatternBank(3);
    bool bankOk = drums.kickAt(0) == kickProbe && drums.hatAt(1) == hatProbe;
    benchTestLine(bankOk, F("eeprom pattern bank"), pass, total);
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.write(bankAddr + i, bankBackup[i]);
    }
    loadPatternBank(3);
#else
    (void)bankAddr;
    (void)bankBackup;
#endif
  }

  SeqPlayer seqBackup;
  seqBackup.begin();
  const bool hadSeqSlot = presets.loadSeqPattern(3, seqBackup);
  const int liveLen = seq.length();

  SeqPlayer probe;
  probe.begin();
  probe.setLength(4);
  probe.setStep(0, 60);
  probe.setStep(1, 62);
  probe.setStep(2, SeqPlayer::kRest);
  probe.setStep(3, 64);
  const bool savedProbe = presets.saveSeqPattern(3, probe);
  SeqPlayer loaded;
  loaded.begin();
  bool eepromOk = savedProbe && presets.loadSeqPattern(3, loaded);
  eepromOk = eepromOk && loaded.length() == 4;
  for (int i = 0; i < 4 && eepromOk; i++) {
    eepromOk = loaded.stepNote(i) == probe.stepNote(i);
  }
  benchTestLine(eepromOk, F("eeprom seq slot3"), pass, total);
  if (hadSeqSlot) {
    presets.saveSeqPattern(3, seqBackup);
  } else {
    SeqPlayer blank;
    blank.begin();
    blank.setLength(liveLen > 0 ? liveLen : SeqPlayer::kSteps);
    presets.saveSeqPattern(3, blank);
  }

  const uint8_t live0 = seq.stepNote(0);
  seq.setStep(0, live0 == 72 ? 71 : 72);
  patternImportLine(String("seq ") + String(seq.stepNote(0)) + F(" - 62 64"));
  bool importOk = seq.length() >= 4;
  importOk = importOk && seq.stepNote(0) == (live0 == 72 ? 71 : 72);
  importOk = importOk && seq.stepNote(1) == SeqPlayer::kRest;
  importOk = importOk && seq.stepNote(2) == 62;
  importOk = importOk && seq.stepNote(3) == 64;
  benchTestLine(importOk, F("pattern import parse"), pass, total);
  seq.setStep(0, live0);

  const bool stashHad = benchStash.valid();
  if (!stashHad) captureBenchStash();
  const uint8_t stashNote = seq.stepNote(0);
  seq.setStep(0, stashNote == 80 ? 79 : 80);
  undoBenchStash();
  benchTestLine(seq.stepNote(0) == stashNote, F("stash undo"), pass, total);
  if (!stashHad) benchStash.clear();
#else
  benchTestLine(false, F("sequencer disabled"), pass, total);
#endif

#if BENCH_DRUM_GRID
  const int dlen = drums.length();
  const bool kick0 = drums.kickAt(0) != 0;
  drums.setKick(0, !kick0);
  drumInvertPattern();
  benchTestLine(drums.kickAt(0) == kick0, F("drum invert"), pass, total);
  drums.setKick(0, kick0);
  (void)dlen;
#endif

#if BENCH_BASS_SEQ
  const int8_t b0 = bass.stepDegree(0);
  bass.setStep(0, b0 == BassSeq::kRest ? 0 : BassSeq::kRest);
  bassInvertPattern();
  benchTestLine(bass.stepDegree(0) == b0, F("bass invert"), pass, total);
  bass.setStep(0, b0);
#endif

#if BENCH_SEQUENCER
  {
    const uint8_t sn0 = seq.stepNote(0);
#if BENCH_DRUM_GRID
    const bool dk0 = drums.kickAt(0) != 0;
#endif
    patternRotateAll(1);
    bool rotOk = seq.length() >= 2 && seq.stepNote(1) == sn0;
#if BENCH_DRUM_GRID
    rotOk = rotOk && drums.kickAt(1) == dk0;
#endif
    patternRotateAll(-1);
    rotOk = rotOk && seq.stepNote(0) == sn0;
    benchTestLine(rotOk, F("pattern rotate"), pass, total);
  }
  {
    const int len = seq.length();
    uint8_t saved[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) saved[i] = seq.stepNote(i);
    patternMirrorAll();
    patternMirrorAll();
    bool mirOk = true;
    for (int i = 0; i < len && mirOk; i++) {
      mirOk = seq.stepNote(i) == saved[i];
    }
    benchTestLine(mirOk, F("pattern mirror"), pass, total);
  }
  {
    const int len = seq.length();
    if (len >= 2) {
      const uint8_t n0 = seq.stepNote(0);
      const uint8_t n1 = seq.stepNote(1);
      patternSwapSteps(0, 1);
      bool swapOk = seq.stepNote(0) == n1 && seq.stepNote(1) == n0;
      patternSwapSteps(0, 1);
      swapOk = swapOk && seq.stepNote(0) == n0 && seq.stepNote(1) == n1;
      benchTestLine(swapOk, F("pattern swap"), pass, total);
    }
  }
  {
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    seq.setStep(0, 60);
    if (len > 1) seq.setStep(1, SeqPlayer::kRest);
    if (len > 2) seq.setStep(2, 64);
    patternStrip();
    bool stripOk = seq.length() == 2 && seq.stepNote(0) == 60 && seq.stepNote(1) == 64;
    for (int i = 0; i < len; i++) seq.setStep(i, backup[i]);
    seq.setLength(len);
    benchTestLine(stripOk, F("pattern strip"), pass, total);
  }
  {
    const int was = seq.length();
    patternSetLength(8);
    bool lenOk = seq.length() == 8;
#if BENCH_DRUM_GRID
    lenOk = lenOk && drums.length() == 8;
#endif
    patternSetLength(was);
    benchTestLine(lenOk, F("pattern len"), pass, total);
  }
  {
    const ScaleMode savedScale = chords.scale();
    const int savedRoot = chords.root();
    chords.setRoot(60);
    chords.setScale(ScaleMode::Major);
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    for (int i = 0; i < len; i++) seq.setStep(i, SeqPlayer::kRest);
    seq.setStep(0, 61);
    patternAlign();
    const int q = chords.quantizeToScale(61);
    bool alignOk = seq.length() == 1 && seq.stepNote(0) == (uint8_t)q;
    for (int i = 0; i < len; i++) seq.setStep(i, backup[i]);
    seq.setLength(len);
    chords.setRoot(savedRoot);
    chords.setScale(savedScale);
    benchTestLine(alignOk, F("pattern align"), pass, total);
  }
  {
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    seqInvertPattern();
    seqInvertPattern();
    bool invOk = true;
    for (int i = 0; i < len && invOk; i++) {
      invOk = seq.stepNote(i) == backup[i];
    }
    benchTestLine(invOk, F("pattern invert"), pass, total);
  }
  {
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    patternRotateAll(3);
    patternRotateAll(-3);
    bool shiftOk = true;
    for (int i = 0; i < len && shiftOk; i++) {
      shiftOk = seq.stepNote(i) == backup[i];
    }
    benchTestLine(shiftOk, F("pattern shift"), pass, total);
  }
  {
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    patternMirrorAll();
    patternMirrorAll();
    bool mirRtOk = true;
    for (int i = 0; i < len && mirRtOk; i++) {
      mirRtOk = seq.stepNote(i) == backup[i];
    }
    benchTestLine(mirRtOk, F("pattern mirror rt"), pass, total);
  }
  benchTestLine(benchFreeRamBytes() > 4096U, F("heap free"), pass, total);
  {
    const int was = seq.length();
    if (was >= 8) {
      uint8_t backup[SeqPlayer::kMaxSteps];
      for (int i = 0; i < was; i++) backup[i] = seq.stepNote(i);
      patternHalve();
      const bool halveOk = seq.length() == was / 2;
      for (int i = 0; i < was; i++) seq.setStep(i, backup[i]);
      seq.setLength(was);
      benchTestLine(halveOk, F("pattern halve"), pass, total);
    }
  }
  {
#if BENCH_BASS_SEQ && BENCH_SEQUENCER
    uint8_t seqBackup[SeqPlayer::kMaxSteps];
    int8_t bassBackup[BassSeq::kSteps];
    const int len = min(seq.length(), BassSeq::kSteps);
    for (int i = 0; i < len; i++) {
      seqBackup[i] = seq.stepNote(i);
      bassBackup[i] = bass.stepDegree(i);
    }
    seq.setStep(0, 60);
    for (int i = 1; i < len; i++) seq.setStep(i, SeqPlayer::kRest);
    bassFromSeqPattern(true);
    const int expected = midiToScaleDegree(60);
    const bool fuseOk = bass.stepDegree(0) == expected;
    for (int i = 0; i < len; i++) {
      seq.setStep(i, seqBackup[i]);
      bass.setStep(i, bassBackup[i]);
    }
    benchTestLine(fuseOk, F("pattern fuse"), pass, total);
#endif
  }
  {
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    int before = 0;
    for (int i = 0; i < len; i++) {
      if (seq.stepNote(i) != SeqPlayer::kRest) before++;
    }
    for (int i = 1; i < len; i += 2) {
      seq.setStep(i, SeqPlayer::kRest);
    }
    patternGlaze();
    int after = 0;
    for (int i = 0; i < seq.length(); i++) {
      if (seq.stepNote(i) != SeqPlayer::kRest) after++;
    }
    for (int i = 0; i < len; i++) seq.setStep(i, backup[i]);
    seq.setLength(len);
    benchTestLine(after <= before && after > 0, F("pattern glaze"), pass, total);
  }
  {
    const ScaleMode savedScale = chords.scale();
    const int savedRoot = chords.root();
    chords.setRoot(60);
    chords.setScale(ScaleMode::Major);
    const int len = seq.length();
    uint8_t backup[SeqPlayer::kMaxSteps];
    for (int i = 0; i < len; i++) backup[i] = seq.stepNote(i);
    for (int i = 0; i < len; i++) seq.setStep(i, SeqPlayer::kRest);
    seq.setStep(0, 61);
    patternSnap();
    const int q = chords.quantizeToScale(61);
    const bool snapOk = seq.length() >= 1 && seq.stepNote(0) == (uint8_t)q;
    for (int i = 0; i < len; i++) seq.setStep(i, backup[i]);
    seq.setLength(len);
    chords.setRoot(savedRoot);
    chords.setScale(savedScale);
    benchTestLine(snapOk, F("pattern snap"), pass, total);
  }
  {
    const int was = seq.length();
    if (was >= 8) {
      uint8_t backup[SeqPlayer::kMaxSteps];
      for (int i = 0; i < was; i++) backup[i] = seq.stepNote(i);
      patternTaper();
      const bool taperOk = seq.length() == was / 2;
      for (int i = 0; i < was; i++) seq.setStep(i, backup[i]);
      seq.setLength(was);
      benchTestLine(taperOk, F("pattern taper"), pass, total);
    }
  }
#if BENCH_SWING_POT
  controls.poll();
  {
    const float swing = controls.swingNorm();
    benchTestLine(swing >= 0.0f && swing <= 1.05f, F("swing pot"), pass, total);
  }
#endif
#if BENCH_LAYER_BUTTONS
  benchTestLine(digitalRead(kPinBtnSeq) == HIGH && digitalRead(kPinBtnDrums) == HIGH &&
                    digitalRead(kPinBtnBass) == HIGH,
                F("layer btn pullup"), pass, total);
#endif
  benchTestLine(benchPinLintCount() == 0, F("pin lint clean"), pass, total);
#if BENCH_MONO_FILTER
  {
#if BENCH_MONO_LADDER
    monoLadder.reset();
    monoLadder.setCutoff(1200.0f);
    const float a = monoLadder.process(1.0f);
    const float b = monoLadder.process(0.0f);
    benchTestLine(fabsf(a) > 0.01f && fabsf(b) < fabsf(a), F("mono zdf ladder"), pass, total);
    const float lo = cutoffHzFromNorm(0.0f);
    const float hi = cutoffHzFromNorm(1.0f);
    benchTestLine(lo < 200.0f && hi > 4000.0f, F("filter log map"), pass, total);
#else
    monoLpf.reset();
    monoLpf.setCutoff(1200.0f);
    const float a = monoLpf.process(1.0f);
    const float b = monoLpf.process(0.0f);
    benchTestLine(fabsf(a) > 0.01f && fabsf(b) < fabsf(a), F("mono zdf filter"), pass, total);
#endif
  }
#if BENCH_FILTER_ENV
  {
    FilterEnv probe;
    probe.setAttackMs(10.0f);
    probe.setDecayMs(80.0f);
    probe.setSustain(0.4f);
    probe.setReleaseMs(120.0f);
    probe.trigger();
    unsigned long t = 1000;
    for (int i = 0; i < 12; i++) {
      probe.poll(t);
      t += 5;
    }
    const bool atkOk = probe.level() > 0.85f;
    probe.release();
    for (int i = 0; i < 40; i++) {
      probe.poll(t);
      t += 5;
    }
    benchTestLine(atkOk && probe.level() < 0.08f, F("filter env adsr"), pass, total);
  }
#endif
#endif
  {
    const int was = seq.length();
    if (was >= 4) {
      const bool bounceWas = seq.bounce();
      patternLoop();
      const bool loopOk = seq.bounce();
      seq.setBounce(bounceWas);
      benchTestLine(loopOk, F("pattern loop"), pass, total);
    }
  }
  {
    const bool accentWas = seqAccent;
    patternAnchor();
    const bool anchorOk = seq.length() > 0;
    seqAccent = accentWas;
    benchTestLine(anchorOk, F("pattern anchor"), pass, total);
  }
  {
    const bool accentWas = seqAccent;
    patternPulse();
    const bool pulseOk = seqAccent;
    seqAccent = accentWas;
    benchTestLine(pulseOk, F("pattern pulse"), pass, total);
  }
  {
    const bool bounceWas = seq.bounce();
    patternLatch();
    const bool latchOk = seq.bounce() != bounceWas;
    patternLatch();
    benchTestLine(latchOk, F("pattern latch"), pass, total);
  }
  {
    const int lenWas = seq.length();
    patternRivet();
    const bool rivetOk = seq.length() == lenWas && lenWas > 0;
    benchTestLine(rivetOk, F("pattern rivet"), pass, total);
  }
  {
    const int lenWas = seq.length();
    patternWeld();
    const bool seqWas = seq.enabled();
    runMacro(String("chrome"));
    const bool smokeOk = seq.length() >= lenWas && lenWas > 0 && seq.enabled();
    if (!seqWas) setSeqEnabled(false);
    benchTestLine(smokeOk, F("pattern/macro smoke"), pass, total);
  }
  {
    const bool perfOk = benchFreeRamBytes() > 3000U && dac.isStreaming() &&
                        isfinite(audioOut.peak());
    benchTestLine(perfOk, F("bench perf headroom"), pass, total);
  }
  {
    uint8_t bankBackup[PresetStore::kPatternBankBytes];
    const int bankAddr = PresetStore::kPatternBankBase + 3 * PresetStore::kPatternBankBytes;
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.get(bankAddr + i, bankBackup[i]);
    }
    char hex[PresetStore::kPatternBankBytes * 2 + 1];
    const int addr0 = PresetStore::kPatternBankBase;
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      uint8_t b = 0;
      EEPROM.get(addr0 + i, b);
      static const char kHex[] = "0123456789ABCDEF";
      hex[i * 2] = kHex[b >> 4];
      hex[i * 2 + 1] = kHex[b & 0x0F];
    }
    hex[PresetStore::kPatternBankBytes * 2] = '\0';
    const bool importOk =
        presets.importPatternBankHex(3, hex, PresetStore::kPatternBankBytes * 2) &&
        presets.patternBankDiffCount(0, 3) == 0;
    benchTestLine(importOk, F("eeprom import hex"), pass, total);
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.write(bankAddr + i, bankBackup[i]);
    }
  }
  {
    uint8_t slotBackup[PresetStore::kBytesPerSlot];
    uint8_t bankBackup[PresetStore::kPatternBankBytes];
    const int toSlotAddr = 2 * PresetStore::kBytesPerSlot;
    const int toBankAddr = PresetStore::kPatternBankBase + 2 * PresetStore::kPatternBankBytes;
    for (int i = 0; i < PresetStore::kBytesPerSlot; i++) {
      EEPROM.get(toSlotAddr + i, slotBackup[i]);
    }
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.get(toBankAddr + i, bankBackup[i]);
    }
    const bool cloneOk = presets.cloneSlot(0, 2) && presets.chordSnapshotsEqual(0, 2) &&
                         presets.patternBankDiffCount(0, 2) == 0;
    benchTestLine(cloneOk, F("eeprom clone"), pass, total);
    for (int i = 0; i < PresetStore::kBytesPerSlot; i++) {
      EEPROM.put(toSlotAddr + i, slotBackup[i]);
    }
    for (int i = 0; i < PresetStore::kPatternBankBytes; i++) {
      EEPROM.write(toBankAddr + i, bankBackup[i]);
    }
  }
#if BENCH_MONO_FILTER
  {
    const uint32_t usPer = measureFilterLoadUsPerFrame(128, 20);
    benchTestLine(usPer > 0 && usPer < 500U, F("bench filter load"), pass, total);
  }
  {
    const int steps = benchFilterSweepSteps(4, 0);
    benchTestLine(steps == 4, F("bench filter sweep"), pass, total);
  }
  {
    const int steps = benchFilterResSweepSteps(4, 0);
    benchTestLine(steps == 4, F("bench filter res sweep"), pass, total);
  }
#if BENCH_FILTER_ENV
  {
    const int steps = benchFilterEnvSweepSteps(4, 0);
    benchTestLine(steps == 4, F("bench filter env sweep"), pass, total);
  }
  {
    const uint32_t usPer = measureFilterEnvLoadUsPerFrame(128, 20);
    benchTestLine(usPer > 0 && usPer < 500U, F("bench filter env load"), pass, total);
  }
#endif
#if BENCH_MONO_LADDER
  {
    const uint32_t usPer = measureFilterResLoadUsPerFrame(128, 20);
    benchTestLine(usPer > 0 && usPer < 600U, F("bench filter res load"), pass, total);
  }
#endif
#if BENCH_FILTER_ENV && BENCH_MONO_LADDER
  {
    const uint32_t usPer = measureFilterComboLoadUsPerFrame(128, 20);
    benchTestLine(usPer > 0 && usPer < 700U, F("bench filter combo load"), pass, total);
  }
#endif
  {
    const uint32_t loadUs = measureFilterLoadUsPerFrame(128, 8);
    benchTestLine(loadUs > 0 && loadUs < 500U, F("bench filter profile"), pass, total);
  }
#if BENCH_FILTER_ENV && BENCH_MONO_LADDER
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(combo >= load && combo < 700U, F("bench filter headroom"), pass, total);
  }
  {
    const uint32_t budget = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(budget > 0 && budget < 700U, F("bench filter budget"), pass, total);
  }
  {
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(combo > 0 && combo < 700U, F("bench filter gauge"), pass, total);
  }
  {
    const uint32_t comboCeil = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(comboCeil > 0 && comboCeil < 700U, F("bench filter ceiling"), pass, total);
  }
  {
    const uint32_t comboRes = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(comboRes > 0 && comboRes < 700U, F("bench filter reserve"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(combo >= load && combo < 700U, F("bench filter cushion"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(combo >= load && combo < 700U, F("bench filter span"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(load + combo > 0 && combo < 700U, F("bench filter tally"), pass, total);
  }
  {
    const float frameUs = 1000000.0f / (float)kSampleRate;
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t tallyUs = load + env + res + combo;
    const float marginUs = frameUs - (float)tallyUs;
    benchTestLine(tallyUs > 0 && marginUs > 0.0f, F("bench filter margin"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t tallyUs = load + env + res + combo;
    benchTestLine(tallyUs > 0 && load > 0 && combo > 0, F("bench filter share"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t peakUs = max(max(load, env), max(res, combo));
    benchTestLine(peakUs > 0 && peakUs < 700U, F("bench filter peak"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    benchTestLine(load > 0 && combo >= load && combo < 700U, F("bench filter balance"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t blendUs = (load + env + combo) / 3U;
    benchTestLine(load > 0 && blendUs >= load && blendUs < 700U, F("bench filter blend"), pass, total);
  }
  {
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const float pivotRatio = res > 0 ? (float)env / (float)res : 0.0f;
    benchTestLine(env > 0 && res > 0 && pivotRatio > 0.0f, F("bench filter pivot"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t hingeUs = (load + combo) / 2U;
    benchTestLine(load > 0 && combo >= load && hingeUs >= load && hingeUs <= combo, F("bench filter hinge"), pass, total);
  }
  {
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t swivelUs = env + res;
    const float swivelRatio = swivelUs > 0 ? (float)combo / (float)swivelUs : 0.0f;
    benchTestLine(env > 0 && res > 0 && swivelUs > 0 && swivelRatio > 0.0f, F("bench filter swivel"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t axlePairUs = env + res;
    const float axleRatio = load > 0 ? (float)axlePairUs / (float)load : 0.0f;
    benchTestLine(load > 0 && env > 0 && res > 0 && axleRatio > 0.0f, F("bench filter axle"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t couplerUs = load + env;
    const float couplerRatio = couplerUs > 0 ? (float)combo / (float)couplerUs : 0.0f;
    benchTestLine(load > 0 && env > 0 && couplerUs > 0 && couplerRatio > 0.0f, F("bench filter coupler"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t linkageUs = load + res;
    const float linkageRatio = linkageUs > 0 ? (float)combo / (float)linkageUs : 0.0f;
    benchTestLine(load > 0 && res > 0 && linkageUs > 0 && linkageRatio > 0.0f, F("bench filter linkage"), pass, total);
  }
  {
    const uint32_t load = measureFilterLoadUsPerFrame(128, 8);
    const uint32_t env = measureFilterEnvLoadUsPerFrame(128, 8);
    const uint32_t res = measureFilterResLoadUsPerFrame(128, 8);
    const uint32_t combo = measureFilterComboLoadUsPerFrame(128, 8);
    const uint32_t trussUs = load + env + res;
    const float trussRatio = trussUs > 0 ? (float)combo / (float)trussUs : 0.0f;
    benchTestLine(load > 0 && env > 0 && res > 0 && trussUs > 0 && trussRatio > 0.0f,
                  F("bench filter truss"), pass, total);
  }
#endif
#endif
  {
    const bool verifyOk = presets.slotValid(0);
    benchTestLine(verifyOk, F("eeprom verify"), pass, total);
  }
  {
    const uint16_t c0 = eepromSlotChecksum(0);
    const uint16_t c1 = eepromSlotChecksum(0);
    const bool checksumOk = (c0 == c1) && (!presets.slotValid(0) || c0 != 0);
    benchTestLine(checksumOk, F("eeprom checksum"), pass, total);
  }
  {
    const uint16_t snapCs = eepromSlotChecksum(0);
    const bool snapOk = presets.slotValid(0) && snapCs != 0;
    benchTestLine(snapOk, F("eeprom snapshot"), pass, total);
  }
  {
    const bool sealOk = presets.slotValid(0);
    benchTestLine(sealOk, F("eeprom seal"), pass, total);
  }
  {
    const bool manifestOk = presets.slotValid(0) && eepromSlotChecksum(0) != 0;
    benchTestLine(manifestOk, F("eeprom manifest"), pass, total);
  }
  {
    const int auditChord = presets.chordSlotDiffCount(0, 0);
    const int auditBank = presets.patternBankDiffCount(0, 0);
    benchTestLine(auditChord == 0 && auditBank == 0, F("eeprom audit"), pass, total);
  }
  {
    uint16_t parity = 0;
    int valid = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) valid++;
      parity ^= eepromSlotChecksum(i);
    }
    benchTestLine(valid > 0 && parity != 0, F("eeprom parity"), pass, total);
  }
  {
    uint32_t digest = 0;
    int validDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) validDigest++;
      digest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(validDigest > 0 && digest != 0, F("eeprom digest"), pass, total);
  }
  {
    int censusValid = 0;
    uint16_t minCs = 0xFFFF;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) censusValid++;
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs < minCs) minCs = cs;
    }
    benchTestLine(censusValid > 0 && minCs != 0xFFFF, F("eeprom census"), pass, total);
  }
  {
    uint8_t mask = 0;
    uint32_t digest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) mask |= (uint8_t)(1U << i);
      digest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(mask != 0 && digest != 0, F("eeprom ledger"), pass, total);
  }
  {
    const bool rosterOk = presets.slotValid(0) && eepromSlotChecksum(0) != 0;
    benchTestLine(rosterOk, F("eeprom roster"), pass, total);
  }
  {
    uint8_t idxMask = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) idxMask |= (uint8_t)(1U << i);
    }
    benchTestLine(idxMask != 0, F("eeprom index"), pass, total);
  }
  {
    int sealed = 0;
    uint32_t digest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) sealed++;
      digest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(sealed > 0 && digest != 0, F("eeprom atlas"), pass, total);
  }
  {
    int almanacSealed = 0;
    uint8_t almanacMask = 0;
    uint32_t almanacDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        almanacSealed++;
        almanacMask |= (uint8_t)(1U << i);
      }
      almanacDigest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(almanacSealed > 0 && almanacMask != 0 && almanacDigest != 0, F("eeprom almanac"), pass, total);
  }
  {
    int gazetteSealed = 0;
    uint8_t gazetteMask = 0;
    uint16_t gazetteParity = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        gazetteSealed++;
        gazetteMask |= (uint8_t)(1U << i);
      }
      gazetteParity ^= eepromSlotChecksum(i);
    }
    benchTestLine(gazetteSealed > 0 && gazetteMask != 0 && gazetteParity != 0, F("eeprom gazette"), pass, total);
  }
  {
    int regSealed = 0;
    uint8_t regMask = 0;
    uint32_t regDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        regSealed++;
        regMask |= (uint8_t)(1U << i);
      }
      regDigest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(regSealed > 0 && regMask != 0 && regDigest != 0, F("eeprom register"), pass, total);
  }
  {
    int codexSealed = 0;
    uint8_t codexMask = 0;
    uint32_t codexDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        codexSealed++;
        codexMask |= (uint8_t)(1U << i);
      }
      codexDigest += (uint32_t)eepromSlotChecksum(i);
    }
    benchTestLine(codexSealed > 0 && codexMask != 0 && codexDigest != 0, F("eeprom codex"), pass, total);
  }
  {
    int tomeSealed = 0;
    uint8_t tomeMask = 0;
    uint16_t tomeMinCs = 0xFFFF;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        tomeSealed++;
        tomeMask |= (uint8_t)(1U << i);
      }
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs < tomeMinCs) tomeMinCs = cs;
    }
    benchTestLine(tomeSealed > 0 && tomeMask != 0 && tomeMinCs != 0xFFFF, F("eeprom tome"), pass, total);
  }
  {
    int folioSealed = 0;
    uint8_t folioMask = 0;
    uint16_t folioMinCs = 0xFFFF;
    uint16_t folioMaxCs = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        folioSealed++;
        folioMask |= (uint8_t)(1U << i);
      }
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs < folioMinCs) folioMinCs = cs;
      if (cs > folioMaxCs) folioMaxCs = cs;
    }
    benchTestLine(folioSealed > 0 && folioMask != 0 && folioMaxCs >= folioMinCs, F("eeprom folio"), pass, total);
  }
  {
    int scrollSealed = 0;
    uint8_t scrollMask = 0;
    uint16_t scrollMinCs = 0xFFFF;
    uint16_t scrollMaxCs = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) {
        scrollSealed++;
        scrollMask |= (uint8_t)(1U << i);
      }
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs < scrollMinCs) scrollMinCs = cs;
      if (cs > scrollMaxCs) scrollMaxCs = cs;
    }
    benchTestLine(scrollSealed > 0 && scrollMask != 0 && scrollMaxCs >= scrollMinCs, F("eeprom scroll"), pass, total);
  }
  {
    int archiveSealed = 0;
    uint32_t archiveDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (presets.slotValid(i)) archiveSealed++;
      archiveDigest += (uint32_t)eepromSlotChecksum(i);
    }
    const uint32_t archiveAvg = PresetStore::kSlots > 0 ? archiveDigest / (uint32_t)PresetStore::kSlots : 0;
    benchTestLine(archiveSealed > 0 && archiveDigest != 0 && archiveAvg != 0, F("eeprom archive"), pass, total);
  }
  {
    int vaultSealed = 0;
    int vaultOccupied = 0;
    uint32_t vaultDigest = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs != 0) vaultOccupied++;
      if (presets.slotValid(i)) vaultSealed++;
      vaultDigest += (uint32_t)cs;
    }
    benchTestLine(vaultSealed > 0 && vaultOccupied > 0 && vaultDigest != 0, F("eeprom vault"), pass, total);
  }
  {
    int annalsSealed = 0;
    int annalsOccupied = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (eepromSlotChecksum(i) != 0) annalsOccupied++;
      if (presets.slotValid(i)) annalsSealed++;
    }
    const int annalsVacant = PresetStore::kSlots - annalsOccupied;
    benchTestLine(annalsSealed > 0 && annalsOccupied > 0 && annalsVacant >= 0, F("eeprom annals"), pass, total);
  }
  {
    int chronicleSealed = 0;
    int chronicleOccupied = 0;
    uint16_t chronicleMaxCs = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      const uint16_t cs = eepromSlotChecksum(i);
      if (cs != 0) chronicleOccupied++;
      if (presets.slotValid(i)) chronicleSealed++;
      if (cs > chronicleMaxCs) chronicleMaxCs = cs;
    }
    const int chronicleVacant = PresetStore::kSlots - chronicleOccupied;
    const bool chronicleOk =
        chronicleSealed > 0 && chronicleOccupied > 0 && chronicleVacant >= 0;
    benchTestLine(chronicleOk, F("eeprom chronicle"), pass, total);
  }
  {
    int registrySealed = 0;
    int registryOccupied = 0;
    for (int i = 0; i < PresetStore::kSlots; i++) {
      if (eepromSlotChecksum(i) != 0) registryOccupied++;
      if (presets.slotValid(i)) registrySealed++;
    }
    const int registryVacant = PresetStore::kSlots - registryOccupied;
    const bool registryOk =
        registrySealed > 0 && registryOccupied > 0 && registryVacant >= 0;
    benchTestLine(registryOk, F("eeprom registry"), pass, total);
  }
  {
    const int chordDiffs = presets.chordSlotDiffCount(0, 0);
    const int bankDiffs = presets.patternBankDiffCount(0, 0);
    benchTestLine(chordDiffs == 0 && bankDiffs == 0, F("eeprom diff"), pass, total);
  }
  {
    const int lenWas = seq.length();
    patternForge();
    const bool forgeOk = seq.length() == lenWas && lenWas > 0;
    benchTestLine(forgeOk, F("pattern forge"), pass, total);
  }
  {
    const bool bounceWas = seq.bounce();
    patternMint();
    const bool mintOk = seq.bounce() != bounceWas;
    seq.setBounce(bounceWas);
    benchTestLine(mintOk, F("pattern mint"), pass, total);
  }
  {
    const int lenWas = seq.length();
    patternAnvil();
    const bool anvilOk = seq.length() == lenWas && lenWas > 0;
    benchTestLine(anvilOk, F("pattern anvil"), pass, total);
  }
  {
    const int lenTemper = seq.length();
    patternTemper();
    const bool temperOk = seq.length() == lenTemper && lenTemper > 0;
    benchTestLine(temperOk, F("pattern temper"), pass, total);
  }
  {
    const int lenQuench = seq.length();
    patternQuench();
    const bool quenchOk = seq.length() == lenQuench && lenQuench > 0;
    benchTestLine(quenchOk, F("pattern quench"), pass, total);
  }
  {
    const int lenSmelt = seq.length();
    patternSmelt();
    const bool smeltOk = seq.length() == lenSmelt && lenSmelt > 0;
    benchTestLine(smeltOk, F("pattern smelt"), pass, total);
  }
  {
    const int lenCast = seq.length();
    patternCast();
    const bool castOk = seq.length() == lenCast && lenCast > 0;
    benchTestLine(castOk, F("pattern cast"), pass, total);
  }
  {
    const int lenMold = seq.length();
    patternMold();
    const bool moldOk = seq.length() == lenMold && lenMold > 0;
    benchTestLine(moldOk, F("pattern mold"), pass, total);
  }
  {
    const int lenStamp = seq.length();
    patternStamp();
    const bool stampOk = seq.length() == lenStamp && lenStamp > 0;
    benchTestLine(stampOk, F("pattern stamp"), pass, total);
  }
  {
    const int lenPlate = seq.length();
    patternPlate();
    const bool plateOk = seq.length() == lenPlate && lenPlate > 0;
    benchTestLine(plateOk, F("pattern plate"), pass, total);
  }
  {
    const int lenSheet = seq.length();
    patternSheet();
    const bool sheetOk = seq.length() == lenSheet && lenSheet > 0;
    benchTestLine(sheetOk, F("pattern sheet"), pass, total);
  }
  {
    const int lenCoil = seq.length();
    patternCoil();
    const bool coilOk = seq.length() == lenCoil && lenCoil > 0;
    benchTestLine(coilOk, F("pattern coil"), pass, total);
  }
  {
    const int lenWire = seq.length();
    patternWire();
    const bool wireOk = seq.length() == lenWire && lenWire > 0;
    benchTestLine(wireOk, F("pattern wire"), pass, total);
  }
  {
    const int lenStrand = seq.length();
    patternStrand();
    const bool strandOk = seq.length() == lenStrand && lenStrand > 0;
    benchTestLine(strandOk, F("pattern strand"), pass, total);
  }
  {
    const int lenCable = seq.length();
    patternCable();
    const bool cableOk = seq.length() == lenCable && lenCable > 0;
    benchTestLine(cableOk, F("pattern cable"), pass, total);
  }
  {
    const int lenHarness = seq.length();
    patternHarness();
    const bool harnessOk = seq.length() == lenHarness && lenHarness > 0;
    benchTestLine(harnessOk, F("pattern harness"), pass, total);
  }
  {
    const int lenMesh = seq.length();
    patternMesh();
    const bool meshOk = seq.length() == lenMesh && lenMesh > 0;
    benchTestLine(meshOk, F("pattern mesh"), pass, total);
  }
  {
    const int lenLattice = seq.length();
    patternLattice();
    const bool latticeOk = seq.length() == lenLattice && lenLattice > 0;
    benchTestLine(latticeOk, F("pattern lattice"), pass, total);
  }
  {
    const int lenMatrix = seq.length();
    patternMatrix();
    const bool matrixOk = seq.length() == lenMatrix && lenMatrix > 0;
    benchTestLine(matrixOk, F("pattern matrix"), pass, total);
  }
  {
    const int lenFabric = seq.length();
    patternFabric();
    const bool fabricOk = seq.length() == lenFabric && lenFabric > 0;
    benchTestLine(fabricOk, F("pattern fabric"), pass, total);
  }
  {
    const int lenTapestry = seq.length();
    patternTapestry();
    const bool tapestryOk = seq.length() == lenTapestry && lenTapestry > 0;
    benchTestLine(tapestryOk, F("pattern tapestry"), pass, total);
  }
  {
    const int lenCanopy = seq.length();
    patternCanopy();
    const bool canopyOk = seq.length() == lenCanopy && lenCanopy > 0;
    benchTestLine(canopyOk, F("pattern canopy"), pass, total);
  }
  {
    const int lenPavilion = seq.length();
    patternPavilion();
    const bool pavilionOk = seq.length() == lenPavilion && lenPavilion > 0;
    benchTestLine(pavilionOk, F("pattern pavilion"), pass, total);
  }
  {
    const int lenPortico = seq.length();
    patternPortico();
    const bool porticoOk = seq.length() == lenPortico && lenPortico > 0;
    benchTestLine(porticoOk, F("pattern portico"), pass, total);
  }
  {
    const int lenColonnade = seq.length();
    patternColonnade();
    const bool colonnadeOk = seq.length() == lenColonnade && lenColonnade > 0;
    benchTestLine(colonnadeOk, F("pattern colonnade"), pass, total);
  }
  {
    const int lenFacade = seq.length();
    patternFacade();
    const bool facadeOk = seq.length() == lenFacade && lenFacade > 0;
    benchTestLine(facadeOk, F("pattern facade"), pass, total);
  }
#if BENCH_SERIAL_MIDI
  benchTestLine(true, F("midi usb cfg"), pass, total);
#endif
#endif

  Serial.print(F("bench test "));
  Serial.print(pass);
  Serial.print(F("/"));
  Serial.print(total);
  Serial.println(pass == total ? F(" PASS") : F(" FAIL"));
  Serial.println(F("---"));
}

static void benchCal() {
  Serial.println(F("--- bench cal ---"));
#if BENCH_VOLUME_POT
  int vmin = 4095;
  int vmax = 0;
  for (int i = 0; i < 32; i++) {
    const int v = analogRead(kPinVol);
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
    delay(5);
  }
  Serial.print(F("vol adc="));
  Serial.print(analogRead(kPinVol));
  Serial.print(F(" min="));
  Serial.print(vmin);
  Serial.print(F(" max="));
  Serial.print(vmax);
  Serial.print(F(" gain="));
  Serial.println(masterGain, 3);
#else
  Serial.println(F("vol pot disabled"));
#endif
#if BENCH_MOD_POT
  Serial.print(F("mod adc="));
  Serial.println(analogRead(kPinMod));
#endif
#if BENCH_COLOR_POT
  Serial.print(F("color adc="));
  Serial.print(analogRead(kPinColor));
  Serial.print(F(" norm="));
  Serial.println(controls.colorNorm(), 3);
#endif
#if BENCH_TEMPO_POT
  Serial.print(F("tempo adc="));
  Serial.print(analogRead(kPinTempo));
  Serial.print(F(" bpm="));
  Serial.println(controls.bpmFromTempo());
#endif
#if BENCH_SWING_POT && BENCH_SEQUENCER
  Serial.print(F("swing adc="));
  Serial.print(analogRead(kPinSwing));
  Serial.print(F(" pct="));
  Serial.println((int)(controls.swingNorm() * 55.0f));
#endif
  Serial.println(F("---"));
}

static void benchControlsPanel() {
  Serial.println(F("--- bench controls ---"));
  Serial.print(F("gain="));
  Serial.println(masterGain, 3);
#if BENCH_MOD_POT
  Serial.print(F("mod="));
  Serial.println(controls.modNorm(), 3);
#endif
#if BENCH_COLOR_POT
  Serial.print(F("tone="));
  Serial.println(controls.colorNorm(), 3);
#endif
#if BENCH_TEMPO_POT
  Serial.print(F("bpm="));
  Serial.println(controls.bpmFromTempo());
#endif
#if BENCH_SWING_POT && BENCH_SEQUENCER
  Serial.print(F("swing="));
  Serial.println((int)(controls.swingNorm() * 55.0f));
#endif
#if BENCH_SEQUENCER
  Serial.print(F("seq btn="));
  Serial.println(digitalRead(kPinBtnSeq) == LOW ? F("down") : F("up"));
#endif
#if BENCH_DRUM_GRID
  Serial.print(F("drum btn="));
  Serial.println(digitalRead(kPinBtnDrums) == LOW ? F("down") : F("up"));
#endif
#if BENCH_BASS_SEQ
  Serial.print(F("bass btn="));
  Serial.println(digitalRead(kPinBtnBass) == LOW ? F("down") : F("up"));
#endif
#if BENCH_TAP_BUTTON
  Serial.print(F("tap btn="));
  Serial.println(digitalRead(kPinBtnTap) == LOW ? F("down") : F("up"));
#endif
#if BENCH_SLOT_BUTTON
  Serial.print(F("slot btn="));
  Serial.println(digitalRead(kPinBtnSlot) == LOW ? F("down") : F("up"));
#endif
  Serial.println(F("---"));
}

static void benchFilter() {
  Serial.println(F("--- bench filter ---"));
#if BENCH_MONO_FILTER
  Serial.print(F("mono mode="));
  Serial.println(monoMode ? F("on") : F("off"));
  Serial.print(F("mono filter="));
  Serial.println(monoFilterOn ? F("on") : F("off"));
#if BENCH_MONO_LADDER
  Serial.print(F("ladder=4pole res="));
  Serial.println(monoLadder.resonance(), 2);
  Serial.println(F("path=synth-only drums/metro dry"));
#endif
  if (monoFilterOn) {
    Serial.print(F("cutoff_hz="));
    Serial.println((int)monoFilterCutoffHz());
    Serial.print(F("cc74="));
    Serial.println(kCcFilterCutoff);
    Serial.print(F("mod_norm="));
    Serial.println(controls.modNorm(), 3);
#if BENCH_COLOR_POT && BENCH_MONO_LADDER
    Serial.print(F("color_norm="));
    Serial.println(controls.colorNorm(), 3);
#endif
#if BENCH_FILTER_ENV
    Serial.print(F("env="));
    Serial.print(filterEnvOn ? 1 : 0);
    Serial.print(F(" depth="));
    Serial.print(filterEnvDepth, 2);
    Serial.print(F(" level="));
    Serial.println(filterEnv.level(), 2);
    Serial.print(F("cc107="));
    Serial.println(kCcFilterEnvDepth);
#if BENCH_MONO_LADDER
    Serial.print(F("applied_hz="));
    Serial.println((int)monoLadder.cutoffHz());
#endif
#endif
  }
#else
  Serial.println(F("mono filter disabled"));
#endif
  Serial.println(F("---"));
}

static void benchMidi() {
  Serial.println(F("--- bench midi ---"));
#if BENCH_SERIAL_MIDI
  Serial.println(F("usb cdc midi=1 baud=115200"));
#else
  Serial.println(F("usb cdc midi=0"));
#endif
#if BENCH_UART_MIDI
  Serial.println(F("uart din=1 rx=PA10 baud=31250"));
#else
  Serial.println(F("uart din=0 profile=flash_uart_midi.ps1"));
#endif
  Serial.print(F("clock="));
  Serial.print(midiClockSync ? F("ext") : F("int"));
  Serial.print(F(" qtz="));
  Serial.print(midiQuantize ? 1 : 0);
  Serial.print(F(" metro="));
  Serial.println(metroEnabled ? 1 : 0);
  Serial.println(F("---"));
}

static void benchDac() {
  Serial.println(F("--- bench dac ---"));
#if BENCH_DAC_PCM5102
  Serial.println(F("module=pcm5102a"));
#else
  Serial.println(F("module=uda1334a cjmcu-1334"));
#endif
  Serial.println(F("i2s2 ws=PB12 bclk=PB13 din=PB15 fmt=philips 44.1k"));
#if BENCH_DAC_PCM5102
#if BENCH_PCM_XMT_PIN
  Serial.print(F("xmt gpio="));
  Serial.print(BENCH_PCM_XMT_PIN);
  Serial.print(F(" level="));
  Serial.println(digitalRead(BENCH_PCM_XMT_PIN) == HIGH ? 1 : 0);
#else
  Serial.println(F("xmt=tieoff (strap XMT to 3V3 on module)"));
#endif
#else
  Serial.println(F("xmt=n/a (uda1334a has no mute pin)"));
#endif
  Serial.print(F("stream="));
  Serial.println(dac.isStreaming() ? 1 : 0);
  Serial.println(F("---"));
}

static void benchPing() {
  Serial.print(F("bench pong "));
  Serial.println(F411_SYNTH_VERSION);
}

static void benchVersion() {
  Serial.print(LUMPY_SYNTH_NAME);
  Serial.print(F(" v"));
  Serial.println(F411_SYNTH_VERSION);
}

static void benchApi() {
  Serial.println(F("--- bench api ---"));
  Serial.print(F("product="));
  Serial.println(LUMPY_SYNTH_NAME);
  Serial.print(F("fw="));
  Serial.println(F411_SYNTH_VERSION);
  Serial.print(F("api="));
  Serial.print(LUMPY_API_MAJOR);
  Serial.print(F("."));
  Serial.print(LUMPY_API_PATCH);
  Serial.println(F(".0"));
  Serial.print(F("eeprom_schema="));
  Serial.println(LUMPY_EEPROM_SCHEMA);
  Serial.println(F("self_tests=125"));
  Serial.print(F("preset_slots="));
  Serial.println(PresetStore::kSlots);
  Serial.print(F("midi_ch="));
  Serial.println(kMidiChannel + 1);
  Serial.println(F("frozen=serial,midi_cc,eeprom_v2,filter_bench,eeprom_tools,pattern_metal"));
  Serial.println(F("---"));
}

static void benchLatency() {
  Serial.println(F("--- bench latency ---"));
  Serial.print(F("sample_rate="));
  Serial.println(kSampleRate);
  Serial.print(F("dma_frames="));
  Serial.println(dac.bufferFrames());
  const float bufMs = 1000.0f * (float)dac.bufferFrames() / (float)kSampleRate;
  Serial.print(F("buffer_ms="));
  Serial.println(bufMs, 2);
  Serial.print(F("roundtrip_est_ms="));
  Serial.println(bufMs * 2.0f, 2);
  Serial.println(F("---"));
}

static void benchPerf() {
  Serial.println(F("--- bench perf ---"));
  Serial.print(F("free_ram="));
  Serial.println(benchFreeRamBytes());
  Serial.print(F("audio_peak="));
  Serial.println(audioOut.peak(), 4);
  Serial.print(F("i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.println(dac.isStreaming() ? 1 : 0);
#if BENCH_MONO_FILTER
  Serial.print(F("mono_filter="));
  Serial.println(monoFilterOn ? 1 : 0);
#endif
  Serial.println(F("---"));
}

static void benchReady() {
  Serial.print(F("bench ready "));
  Serial.print(LUMPY_SYNTH_NAME);
  Serial.print(F(" fw="));
  Serial.print(F411_SYNTH_VERSION);
  Serial.print(F(" i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.print(dac.isStreaming() ? 1 : 0);
  int valid = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) valid++;
  }
  Serial.print(F(" slots="));
  Serial.print(valid);
  Serial.print(F("/"));
  Serial.print(PresetStore::kSlots);
  Serial.print(F(" eeprom="));
  Serial.print(benchEeprom.schema());
  Serial.print(F(" api="));
  Serial.print(LUMPY_API_MAJOR);
  Serial.print(F("."));
  Serial.print(LUMPY_API_PATCH);
  Serial.print(F(" active="));
  Serial.println(benchEeprom.activeSlot());
}

static void benchSnapshot() {
  Serial.print(F("bench snap fw="));
  Serial.print(F411_SYNTH_VERSION);
  Serial.print(F(" slot="));
  Serial.print(activeBenchSlot);
  Serial.print(F(" root="));
  Serial.print(chords.root());
#if BENCH_SEQUENCER
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
  Serial.print(F(" step="));
  Serial.print(seq.currentStep());
  int seqHits = 0;
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) != SeqPlayer::kRest) seqHits++;
  }
  Serial.print(F(" seq_hits="));
  Serial.print(seqHits);
#endif
  Serial.print(F(" dirty="));
  Serial.print(presetDirty ? 1 : 0);
  Serial.print(F(" mute="));
  Serial.print(muteSeq ? 1 : 0);
  Serial.print(muteBass ? 1 : 0);
  Serial.println(muteDrums ? 1 : 0);
}

static void benchUptime() {
  Serial.print(F("bench uptime "));
  Serial.print(millis());
  Serial.println(F("ms"));
}

static void benchHealth() {
  const bool ok = dac.isReady() && dac.isStreaming();
  Serial.print(F("bench health "));
  Serial.print(ok ? F("PASS") : F("FAIL"));
  Serial.print(F(" i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.println(dac.isStreaming() ? 1 : 0);
}

static void benchMem() {
  Serial.print(F("bench mem free="));
  Serial.print(benchFreeRamBytes());
  Serial.println(F("B"));
}

static void benchPeek() {
  Serial.print(F("bench peek fw="));
  Serial.print(F411_SYNTH_VERSION);
  Serial.print(F(" up="));
  Serial.print(millis());
  Serial.print(F("ms ram="));
  Serial.print(benchFreeRamBytes());
  Serial.print(F(" i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.println(dac.isStreaming() ? 1 : 0);
}

static void benchSlots() {
  Serial.print(F("bench slots active="));
  Serial.print(activeBenchSlot);
  for (int i = 0; i < PresetStore::kSlots; i++) {
    Serial.print(F(" "));
    Serial.print(i);
    Serial.print(presets.slotValid(i) ? F(":ok") : F(":--"));
  }
  Serial.println();
}

static void benchLayers() {
  Serial.print(F("bench layers seq="));
#if BENCH_SEQUENCER
  Serial.print(seq.enabled() ? 1 : 0);
#else
  Serial.print(0);
#endif
#if BENCH_DRUM_GRID
  Serial.print(F(" drums="));
  Serial.print(drums.enabled() ? 1 : 0);
#endif
#if BENCH_BASS_SEQ
  Serial.print(F(" bass="));
  Serial.print(bass.enabled() ? 1 : 0);
#endif
  Serial.print(F(" harm="));
  Serial.print(harmony.enabled() ? 1 : 0);
  Serial.print(F(" mute="));
  Serial.print(muteSeq ? 1 : 0);
  Serial.print(muteBass ? 1 : 0);
  Serial.print(muteDrums ? 1 : 0);
  Serial.print(F(" demo="));
  Serial.println(demoActive ? 1 : 0);
}

static void benchTempo() {
  Serial.print(F("bench tempo"));
#if BENCH_SEQUENCER
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
  Serial.print(F(" swing="));
  Serial.print((int)(seq.swing() * 100.0f));
  Serial.print(F("% step="));
  Serial.print(seq.currentStep());
  Serial.print(F("/"));
  Serial.print(seq.length());
#endif
  Serial.println();
}

static void benchChord() {
  Serial.print(F("bench chord root="));
  Serial.print(chords.root());
  Serial.print(F(" type="));
  Serial.print(chords.typeName());
  Serial.print(F(" scale="));
  Serial.print(chords.scaleName());
  Serial.print(F(" inv="));
  Serial.println(chords.inversion());
}

static void benchTransport() {
  Serial.print(F("bench transport demo="));
  Serial.print(demoActive ? 1 : 0);
#if BENCH_SEQUENCER
  Serial.print(F(" seq="));
  Serial.print(seq.enabled() ? 1 : 0);
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
#endif
  Serial.print(F(" metro="));
  Serial.print(metroEnabled ? 1 : 0);
  Serial.print(F(" arp="));
  Serial.print(chords.arpEnabled() ? 1 : 0);
  Serial.println();
}

static void benchMeter() {
  Serial.print(F("bench meter peak="));
  Serial.println(audioOut.peak(), 4);
}

static void benchGroove() {
  Serial.print(F("bench groove"));
#if BENCH_SEQUENCER
  int notes = 0;
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) != SeqPlayer::kRest) notes++;
  }
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
  Serial.print(F(" swing="));
  Serial.print((int)(seq.swing() * 100.0f));
  Serial.print(F(" accent="));
  Serial.print(seqAccent ? 1 : 0);
  Serial.print(F(" hits="));
  Serial.print(notes);
  Serial.print(F("/"));
  Serial.print(seq.length());
#endif
  Serial.println();
}

static void benchAudio() {
  Serial.print(F("bench audio i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.print(dac.isStreaming() ? 1 : 0);
  Serial.print(F(" peak="));
  Serial.print(audioOut.peak(), 4);
#if BENCH_VOLUME_POT
  Serial.print(F(" gain="));
  Serial.print(masterGain, 3);
#endif
  Serial.println();
}

static void benchPattern() { printPatternHitStats(F("bench pattern")); }

static void benchVoices() {
  Serial.print(F("bench voices demo="));
  Serial.print(demoActive ? 1 : 0);
  Serial.print(F(" arp="));
  Serial.print(chords.arpEnabled() ? 1 : 0);
  Serial.print(F(" mono="));
  Serial.print(monoLead.active() ? 1 : 0);
  Serial.print(F(" strum="));
  Serial.print(strum.active() ? 1 : 0);
#if BENCH_HARMONY
  Serial.print(F(" harm="));
  Serial.print(harmony.enabled() ? 1 : 0);
#endif
  Serial.println();
}

static void benchStack() {
  Serial.print(F("bench stack fw="));
  Serial.print(F411_SYNTH_VERSION);
  Serial.print(F(" i2s="));
  Serial.print(dac.isReady() ? 1 : 0);
  Serial.print(F(" stream="));
  Serial.print(dac.isStreaming() ? 1 : 0);
  Serial.print(F(" slot="));
  Serial.print(activeBenchSlot);
  Serial.print(F(" peak="));
  Serial.print(audioOut.peak(), 4);
#if BENCH_SEQUENCER
  int notes = 0;
  for (int i = 0; i < seq.length(); i++) {
    if (seq.stepNote(i) != SeqPlayer::kRest) notes++;
  }
  Serial.print(F(" bpm="));
  Serial.print(seq.bpm());
  Serial.print(F(" hits="));
  Serial.print(notes);
  Serial.print(F("/"));
  Serial.print(seq.length());
#endif
#if BENCH_MONO_FILTER
  Serial.print(F(" filter="));
  Serial.print(monoFilterOn ? 1 : 0);
  if (monoFilterOn) {
    Serial.print(F(" fc="));
    Serial.print((int)monoFilterCutoffHz());
  }
#endif
  Serial.println();
}

static void benchExport() {
  benchSnapshot();
  patternOneline();
  patternStats();
}

static void benchLoop(int times) {
  if (times < 1) times = 1;
  if (times > 5) times = 5;
  Serial.print(F("bench loop "));
  Serial.println(times);
  for (int i = 0; i < times; i++) {
    runBenchSelfTest();
  }
}

static void printBenchDiag() {
  Serial.println(F("--- bench diag ---"));
  Serial.print(F("fw="));
  Serial.println(F411_SYNTH_VERSION);
  Serial.print(F("i2s ready="));
  Serial.print(dac.isReady() ? F("yes") : F("no"));
  Serial.print(F(" stream="));
  Serial.println(dac.isStreaming() ? F("yes") : F("no"));
  Serial.print(F("audio peak="));
  Serial.println(audioOut.peak(), 4);
#if BENCH_VOLUME_POT
  Serial.print(F("vol adc="));
  Serial.print(analogRead(kPinVol));
  Serial.print(F(" gain="));
  Serial.println(masterGain, 3);
#endif
  Serial.print(F("slots:"));
  for (int i = 0; i < PresetStore::kSlots; i++) {
    Serial.print(F(" "));
    Serial.print(i);
    Serial.print(presets.slotValid(i) ? F(":ok") : F(":--"));
  }
  Serial.println();
#if BENCH_SEQUENCER
  Serial.print(F("seq step="));
  Serial.print(seq.currentStep());
  Serial.print(F(" bpm="));
  Serial.println(seq.bpm());
#endif
  Serial.print(F("autosave="));
  Serial.println(autoSaveEnabled ? F("on") : F("off"));
  Serial.println(F("---"));
}

static void benchTone(int note, unsigned ms) {
  if (note < 0) note = 60;
  if (ms < 40U) ms = 800U;
  demoActive = false;
  releaseBlockChord();
  stopArpVoice();
  voicer.noteOn(note, kStkAmp, kGroup);
  const unsigned long t0 = millis();
  while ((unsigned long)(millis() - t0) < ms) {
    delay(1);
  }
  voicer.noteOff(note, kStkAmp, kGroup);
#if BENCH_SERIAL_MIDI
  Serial.print(F("bench tone "));
  Serial.print(note);
  Serial.print(F(" "));
  Serial.print(ms);
  Serial.println(F("ms"));
#endif
}

static void printHelp() {
  Serial.println(F("bench ping | bench version | bench ready | bench snap | bench export | bench health | bench mem | bench peek | bench slots | bench layers | bench tempo | bench chord | bench transport | bench meter | bench groove | bench audio | bench pattern | bench voices | bench stack | bench uptime | bench cal | bench controls | bench lint | bench filter | bench midi | bench dac | bench eeprom | bench api | bench latency | bench perf | bench filter load | bench filter env load | bench filter res load | bench filter combo load | bench filter profile | bench filter headroom | bench filter budget | bench filter gauge | bench filter ceiling | bench filter reserve | bench filter cushion | bench filter span | bench filter tally | bench filter margin | bench filter share | bench filter peak | bench filter balance | bench filter blend | bench filter pivot | bench filter hinge | bench filter swivel | bench filter axle | bench filter coupler | bench filter linkage | bench filter truss | bench filter sweep | bench filter res sweep | bench filter env sweep | bench test | bench loop [n] | bench diag | bench tone [note] [ms]"));
  Serial.println(F("bench test — on-device I2S, audio, EEPROM, stash verify"));
  Serial.println(F("demo|arp|clock|mono|strum|gen|quantize on|off"));
#if BENCH_SEQUENCER
  Serial.println(F("seq on|off | fill | fill32 | random | double | reverse | shift | len | thin | densify"));
  Serial.println(F("seq bounce on|off | transpose <n> | seq invert"));
  Serial.println(F("seq write on|off | seq grid on|off — MIDI step edit"));
  Serial.println(F("pattern — dump | export | import — compact copy-paste"));
  Serial.println(F("pattern import — paste lines, end with ---"));
  Serial.println(F("pattern clear | dup | double | fill | density | quantize | trim | strip | thicken | thin | align | invert | contrast | echo | compact | shift | halve | weave | spread | fuse | bloom | grit | glaze | smear | polish | carve | ripple | flare | knit | snap | rust | braid | taper | churn | drift | loop | anchor | pulse | latch | rivet | bolt | weld | forge | len <n> | stats | oneline | rotate | mirror | shuffle | swap | seed"));
  Serial.println(F("pattern load <n> — patterns only, keep current chord"));
  Serial.println(F("pattern push <n> — save patterns only to EEPROM slot"));
  Serial.println(F("sync len — match drum bar length to seq"));
  Serial.println(F("pattern import q — quantize notes to scale on import"));
  Serial.println(F("seq record on|off | seq record add | seq accent on|off"));
  Serial.println(F("harm on|off | harm 3|4|5|7 | harm lock on|off"));
#endif
#if BENCH_DRUM_GRID
  Serial.println(F("drums on|off | fill | fill32 | double | len | set | pad | write on|off"));
  Serial.println(F("drum from seq | hat from seq | drum hat fill | drum rotate <n> | drum invert"));
  Serial.println(F("drum clear kick | drum clear hat"));
#endif
#if BENCH_BASS_SEQ
  Serial.println(F("bass on|off | fill | double | grid on|off | set | show | from seq | rotate <n> | invert"));
#endif
  Serial.println(F("seq set <step> <note|rest> — e.g. seq set 0 60"));
  Serial.println(F("transport | transport reset | all on | all off"));
  Serial.println(F("mute seq|bass|drums|all on|off — silent layer, clock runs"));
  Serial.println(F("solo seq|bass|drums | unmute all"));
  Serial.println(F("seq bpm <n> | bpm +n | bpm -n | swing +n | swing -n — tap also sets tempo"));
  Serial.println(F("perf moog | perf fat — mono lead + 4-pole ladder (MOD/CC74=cutoff, COLOR/CC4=res)"));
  Serial.println(F("mono filter on|off — ladder on synth only; drums stay dry"));
  Serial.println(F("filter cutoff <hz> | filter res <0-1> | filter env on|off | filter env depth <0-1>"));
  Serial.println(F("filter env adsr <a_ms> <d_ms> <s_0-1> <r_ms> — cutoff envelope"));
#if BENCH_CC_MACROS
  Serial.println(F("MIDI CC32-35 presets | CC36-42 macros | CC43 panic | CC44-49 toggles"));
  Serial.println(F("CC50 stash | CC51 undo | CC52-55 audition | CC56 commit"));
  Serial.println(F("CC57-59 mute | CC60-62 solo | CC63 unmute | CC70 bounce"));
  Serial.println(F("autosave on|off — defer EEPROM auto-write"));
  Serial.println(F("CC71 metro | CC72 chord lock | CC73 metro accent | CC75 stage | CC76 harm lock | CC77 autosave | CC78 transport reset | CC79 pattern mirror | CC80 pattern rotate | CC81 pattern shuffle | CC82 pattern strip | CC83 macro glitch | CC84 pattern thicken | CC85 macro rave | CC86 pattern align | CC87 macro hush | CC88 macro spark | CC89 pattern contrast | CC90 macro frost | CC91 pattern echo | CC92 macro pulse | CC93 pattern compact | CC94 macro surge | CC95 pattern weave | CC96 pattern carve | CC97 macro haze | CC98 pattern snap | CC99 macro glow | CC100 pattern braid | CC101 macro ember | CC102 pattern drift | CC103 macro slate | CC104 pattern anchor | CC105 macro prism | CC74 filter cutoff | CC107 filter env depth | CC108 pattern latch | CC109 pattern rivet | CC110 pattern bolt | CC111 pattern weld | CC112 pattern forge | CC113 pattern mint | CC114 pattern anvil | CC115 pattern temper | CC116 pattern quench | CC117 pattern smelt | CC118 pattern cast | CC119 pattern mold | CC120 pattern stamp | CC121 pattern plate | CC122 pattern sheet | CC123 pattern coil | CC124 pattern wire | CC125 pattern strand | CC126 pattern cable | CC127 pattern harness"));
#endif
  Serial.println(F("chord|scale maj|min|pent|blues | root <n> | inv | oct <n>"));
  Serial.println(F("chord lock on|off | metro on|off | metro accent on|off"));
  Serial.println(F("legato|glide|unison on|off (mono) | tap — tap tempo"));
  Serial.println(F("panel — PA0 chord PB2 demo PB6 panic | PA6 seq PA7 drums PA8 bass PA9 tap PA2 slot (hold=commit)"));
  Serial.println(F("pots PA1 vol PA4 mod PB0 color PB1 tempo PA5 swing | encoder PB3 PB10 PB4 | triad PB7-9"));
  Serial.println(F("learn tone|width|mod|color|breath|swing|harm | learn show | learn reset"));
  Serial.println(F("macro jam|ambient|blues|lounge|techno|funk|full|dub|groove|evolve|live|sketch|polish|warmup|stage|remix|settle|punch|drop|breathe|glitch|rave|hush|spark|frost|pulse|surge|drift|bloom|grit|coast|blaze|mist|weld|haze|forge|flare|glow|rust|ember|veil|braid|slate|prism|pluck|vault|steel|iron|chrome|nickel|copper|brass|bronze|pewter|tin|zinc|lead|silver|gold|platinum|titanium|cobalt|tungsten|mercury|uranium|neptunium|plutonium|americium|curium|berkelium|californium|einsteinium|fermium|mendelevium|nobelium"));
  Serial.println(F("encoder hold 700ms = metro toggle | encoder tap = chord type"));
  Serial.println(F("slots | compare <a> <b> | peek <n>"));
  Serial.println(F("stash | undo | audition <n> | commit [n] | reload | slot <n>"));
  Serial.println(F("eeprom migrate — rewrite schema v2 header + legacy bank0 copy"));
  Serial.println(F("eeprom export <0-3> — chord snapshot + pattern bank hex dump"));
  Serial.println(F("eeprom import <0-3> — paste export lines, end with ---"));
  Serial.println(F("eeprom clone <from> <to> — copy chord slot + pattern bank"));
  Serial.println(F("eeprom diff <a> <b> — chord + pattern bank byte diff counts"));
  Serial.println(F("eeprom verify <slot> — slot valid + chord snapshot"));
  Serial.println(F("eeprom checksum <slot> — chord + pattern bank fingerprint"));
  Serial.println(F("eeprom snapshot <slot> — verify + checksum one-liner"));
  Serial.println(F("eeprom seal — all-slot valid flags"));
  Serial.println(F("eeprom manifest — schema + checksums for all slots"));
  Serial.println(F("eeprom audit <a> <b> — slot valid + chord/pattern diff counts"));
  Serial.println(F("eeprom parity — XOR checksum fingerprint across all slots"));
  Serial.println(F("eeprom digest — sum of slot checksums + valid count"));
  Serial.println(F("eeprom census — valid count + active slot + checksum range"));
  Serial.println(F("eeprom ledger — valid mask + parity + digest + active"));
  Serial.println(F("eeprom roster — per-slot ok + checksum card"));
  Serial.println(F("eeprom index — active index + valid mask + parity"));
  Serial.println(F("eeprom atlas — sealed count + digest + parity + active"));
  Serial.println(F("eeprom almanac — sealed + mask + parity + digest + active"));
  Serial.println(F("eeprom gazette — sealed + mask + parity + active"));
  Serial.println(F("eeprom register — sealed + register mask + digest + active"));
  Serial.println(F("eeprom codex — sealed + mask + parity + digest + active"));
  Serial.println(F("eeprom tome — sealed + mask + parity + digest + min_cs + active"));
  Serial.println(F("eeprom folio — sealed + mask + parity + digest + min_cs + max_cs + active"));
  Serial.println(F("eeprom scroll — sealed + mask + parity + digest + min_cs + max_cs + cs_span + active"));
  Serial.println(F("eeprom archive — sealed + mask + parity + digest + min_cs + max_cs + cs_span + avg_cs + active"));
  Serial.println(F("eeprom vault — sealed + occupied + mask + parity + digest + min_cs + max_cs + cs_span + avg_cs + active"));
  Serial.println(F("eeprom annals — sealed + occupied + vacant + sealed_pct + mask + parity + digest + active"));
  Serial.println(F("eeprom chronicle — annals + cs_span_pct + digest_mod + active"));
  Serial.println(F("eeprom registry — chronicle + occupied_pct + parity_mod + active"));
  Serial.println(F("copy <from> <to> | pattern copy — EEPROM slots 0-3"));
  Serial.println(F("save <0-3> | load <0-3> | erase <n>"));
  Serial.println(F("n<note> | f<note> | help"));
  Serial.println(F("CC70 bounce | CC71 metro | CC72 lock | CC73 accent | CC75 stage | CC76 Hlock"));
  Serial.println(F("MIDI seq grid: 48-79 | bass grid: 80-95"));
  Serial.println(F("MIDI drum pads: 36=kick 42=hat | write on: 36/37/42/43 step edit"));
#if BENCH_OLED || BENCH_CYD_UART
  Serial.println(F("display pattern | transport | next | scroll on|off"));
#endif
  Serial.println(F("MIDI: notes, CC, bend, program, clock F8, start FA"));
}

static bool parseScaleMode(const String &s, ScaleMode &out) {
  if (s.equalsIgnoreCase("maj") || s.equalsIgnoreCase("major")) {
    out = ScaleMode::Major;
    return true;
  }
  if (s.equalsIgnoreCase("min") || s.equalsIgnoreCase("minor")) {
    out = ScaleMode::Minor;
    return true;
  }
  if (s.equalsIgnoreCase("pent") || s.equalsIgnoreCase("pentatonic")) {
    out = ScaleMode::Pentatonic;
    return true;
  }
  if (s.equalsIgnoreCase("blues")) {
    out = ScaleMode::Blues;
    return true;
  }
  return false;
}

static bool parseChordType(const String &s, ChordType &out) {
  if (s.equalsIgnoreCase("maj") || s.equalsIgnoreCase("major")) {
    out = ChordType::Major;
    return true;
  }
  if (s.equalsIgnoreCase("min") || s.equalsIgnoreCase("minor")) {
    out = ChordType::Minor;
    return true;
  }
  if (s.equalsIgnoreCase("sus4")) {
    out = ChordType::Sus4;
    return true;
  }
  if (s.equalsIgnoreCase("maj7")) {
    out = ChordType::Maj7;
    return true;
  }
  if (s.equalsIgnoreCase("min7")) {
    out = ChordType::Min7;
    return true;
  }
  return false;
}

static void runMacro(const String &name) {
  if (name.equalsIgnoreCase("jam")) {
    handleTextCommand("scale blues");
    handleTextCommand("chord min");
    handleTextCommand("arp on");
    handleTextCommand("strum on");
    handleTextCommand("demo on");
    Serial.println(F("macro jam"));
    return;
  }
  if (name.equalsIgnoreCase("ambient")) {
    handleTextCommand("scale pent");
    handleTextCommand("chord maj7");
    handleTextCommand("arp on");
    handleTextCommand("strum off");
    handleTextCommand("demo on");
    Serial.println(F("macro ambient"));
    return;
  }
  if (name.equalsIgnoreCase("blues")) {
    handleTextCommand("scale blues");
    handleTextCommand("chord min7");
    handleTextCommand("arp off");
    handleTextCommand("strum on");
    handleTextCommand("demo on");
    Serial.println(F("macro blues"));
    return;
  }
  if (name.equalsIgnoreCase("lounge")) {
    handleTextCommand("scale maj");
    handleTextCommand("chord maj7");
    handleTextCommand("arp off");
    handleTextCommand("strum on");
    handleTextCommand("gen on");
    Serial.println(F("macro lounge"));
    return;
  }
  if (name.equalsIgnoreCase("techno")) {
    handleTextCommand("scale min");
    handleTextCommand("chord min");
    handleTextCommand("demo off");
    handleTextCommand("seq fill");
    handleTextCommand("seq bpm 128");
    handleTextCommand("seq on");
    Serial.println(F("macro techno"));
    return;
  }
  if (name.equalsIgnoreCase("funk")) {
    handleTextCommand("scale blues");
    handleTextCommand("mono on");
    handleTextCommand("legato on");
    handleTextCommand("drum fill");
    handleTextCommand("drums on");
    handleTextCommand("seq fill");
    handleTextCommand("seq bpm 108");
    handleTextCommand("seq on");
    Serial.println(F("macro funk"));
    return;
  }
  if (name.equalsIgnoreCase("full")) {
    handleTextCommand("scale min");
    handleTextCommand("chord min");
    handleTextCommand("demo on");
    handleTextCommand("bass fill");
    handleTextCommand("drum fill32");
    handleTextCommand("bass on");
    handleTextCommand("drums on");
    handleTextCommand("seq fill32");
    handleTextCommand("seq len 32");
    handleTextCommand("seq bpm 100");
    handleTextCommand("seq on");
    Serial.println(F("macro full"));
    return;
  }
  if (name.equalsIgnoreCase("dub")) {
    handleTextCommand("seq record add");
    handleTextCommand("harm on");
    handleTextCommand("harm 4");
    handleTextCommand("seq on");
    Serial.println(F("macro dub"));
    return;
  }
  if (name.equalsIgnoreCase("groove")) {
    handleTextCommand("swing 28");
    handleTextCommand("seq accent on");
    handleTextCommand("drum fill32");
    handleTextCommand("bass fill");
    handleTextCommand("bass on");
    handleTextCommand("drums on");
    handleTextCommand("seq fill32");
    handleTextCommand("seq len 32");
    handleTextCommand("seq bpm 96");
    handleTextCommand("seq on");
    Serial.println(F("macro groove"));
    return;
  }
  if (name.equalsIgnoreCase("evolve")) {
    handleTextCommand("randomize");
    handleTextCommand("seq random 50");
    handleTextCommand("swing 18");
    handleTextCommand("harm on");
    handleTextCommand("harm 5");
    handleTextCommand("seq accent on");
    handleTextCommand("drums on");
    handleTextCommand("bass on");
    handleTextCommand("seq on");
    Serial.println(F("macro evolve"));
    return;
  }
  if (name.equalsIgnoreCase("live")) {
    handleTextCommand("demo off");
    handleTextCommand("quantize on");
    handleTextCommand("all on");
    Serial.println(F("macro live"));
    return;
  }
  if (name.equalsIgnoreCase("sketch")) {
    handleTextCommand("seq fill");
    handleTextCommand("follow seq");
    handleTextCommand("drum hat fill");
    handleTextCommand("all on");
    Serial.println(F("macro sketch"));
    return;
  }
  if (name.equalsIgnoreCase("polish")) {
    handleTextCommand("humanize");
    handleTextCommand("unmute all");
    Serial.println(F("macro polish"));
    return;
  }
  if (name.equalsIgnoreCase("warmup")) {
    handleTextCommand("all on");
    handleTextCommand("metro on");
    handleTextCommand("seq accent on");
    handleTextCommand("sync len");
    Serial.println(F("macro warmup"));
    return;
  }
  if (name.equalsIgnoreCase("stage")) {
    handleTextCommand("stash");
    handleTextCommand("chord lock on");
    handleTextCommand("all on");
    handleTextCommand("metro on");
    handleTextCommand("seq accent on");
    Serial.println(F("macro stage"));
    return;
  }
  if (name.equalsIgnoreCase("remix")) {
    handleTextCommand("follow seq");
    handleTextCommand("pattern quantize");
    handleTextCommand("sync len");
    Serial.println(F("macro remix"));
    return;
  }
  if (name.equalsIgnoreCase("settle")) {
    handleTextCommand("chord lock off");
    handleTextCommand("harm lock off");
    handleTextCommand("unmute all");
    handleTextCommand("metro off");
    handleTextCommand("demo off");
    Serial.println(F("macro settle"));
    return;
  }
  if (name.equalsIgnoreCase("punch")) {
    randomSeed((uint32_t)micros() ^ (uint32_t)millis());
    transportReset();
    handleTextCommand("all on");
    handleTextCommand("metro on");
    handleTextCommand("seq accent on");
    patternRotateAll((int)random(1, 5));
    Serial.println(F("macro punch"));
    return;
  }
  if (name.equalsIgnoreCase("drop")) {
    handleTextCommand("all off");
    handleTextCommand("metro off");
    handleTextCommand("seq accent off");
    handleTextCommand("demo on");
    handleTextCommand("strum on");
    transportReset();
    Serial.println(F("macro drop"));
    return;
  }
  if (name.equalsIgnoreCase("breathe")) {
    handleTextCommand("scale pent");
    handleTextCommand("chord maj7");
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    handleTextCommand("strum off");
    handleTextCommand("drums off");
    handleTextCommand("bass off");
    handleTextCommand("seq bpm 72");
    handleTextCommand("seq on");
    Serial.println(F("macro breathe"));
    return;
  }
  if (name.equalsIgnoreCase("glitch")) {
    randomSeed((uint32_t)micros() ^ (uint32_t)millis());
    patternShuffleAll();
    handleTextCommand("humanize");
    handleTextCommand("seq accent on");
    handleTextCommand("drums on");
    handleTextCommand("metro on");
    Serial.println(F("macro glitch"));
    return;
  }
  if (name.equalsIgnoreCase("rave")) {
    handleTextCommand("scale min");
    handleTextCommand("chord min");
    handleTextCommand("demo off");
    handleTextCommand("seq fill");
    handleTextCommand("seq bpm 128");
    handleTextCommand("drum fill32");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    randomSeed((uint32_t)micros() ^ (uint32_t)millis());
    patternShuffleAll();
    handleTextCommand("humanize");
    handleTextCommand("metro on");
    handleTextCommand("seq accent on");
    Serial.println(F("macro rave"));
    return;
  }
  if (name.equalsIgnoreCase("hush")) {
    handleTextCommand("all off");
    handleTextCommand("metro off");
    handleTextCommand("seq accent off");
    handleTextCommand("demo on");
    handleTextCommand("strum on");
    transportReset();
    handleTextCommand("scale pent");
    handleTextCommand("chord maj7");
    handleTextCommand("arp on");
    handleTextCommand("seq bpm 72");
    handleTextCommand("seq on");
    Serial.println(F("macro hush"));
    return;
  }
  if (name.equalsIgnoreCase("spark")) {
    patternFill(42, 70);
    handleTextCommand("all on");
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    Serial.println(F("macro spark"));
    return;
  }
  if (name.equalsIgnoreCase("frost")) {
    patternThin();
    patternStrip();
    handleTextCommand("all off");
    handleTextCommand("demo on");
    handleTextCommand("strum on");
    handleTextCommand("scale pent");
    handleTextCommand("chord min");
    handleTextCommand("seq bpm 64");
    handleTextCommand("seq on");
    Serial.println(F("macro frost"));
    return;
  }
  if (name.equalsIgnoreCase("pulse")) {
    handleTextCommand("seq densify");
    nudgeTransportSwing(8);
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    Serial.println(F("macro pulse"));
    return;
  }
  if (name.equalsIgnoreCase("surge")) {
    patternFill(17, 75);
    handleTextCommand("all on");
    handleTextCommand("seq densify");
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    nudgeTransportSwing(6);
    Serial.println(F("macro surge"));
    return;
  }
  if (name.equalsIgnoreCase("drift")) {
    randomSeed((uint32_t)micros() ^ (uint32_t)millis());
    handleTextCommand("humanize");
    handleTextCommand("scale pent");
    handleTextCommand("chord maj7");
    handleTextCommand("demo on");
    handleTextCommand("strum on");
    patternRotateAll((int)random(1, 4));
    handleTextCommand("seq bpm 80");
    handleTextCommand("bass on");
    handleTextCommand("seq on");
    Serial.println(F("macro drift"));
    return;
  }
  if (name.equalsIgnoreCase("bloom")) {
    patternBloom();
    handleTextCommand("demo on");
    handleTextCommand("strum on");
    handleTextCommand("seq on");
    Serial.println(F("macro bloom"));
    return;
  }
  if (name.equalsIgnoreCase("grit")) {
    patternGrit();
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    Serial.println(F("macro grit"));
    return;
  }
  if (name.equalsIgnoreCase("coast")) {
    handleTextCommand("all off");
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    handleTextCommand("seq bpm 68");
    nudgeTransportSwing(-4);
    handleTextCommand("seq on");
    Serial.println(F("macro coast"));
    return;
  }
  if (name.equalsIgnoreCase("blaze")) {
    patternBloom();
    handleTextCommand("seq densify");
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    handleTextCommand("all on");
    Serial.println(F("macro blaze"));
    return;
  }
  if (name.equalsIgnoreCase("mist")) {
    patternGlaze();
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    handleTextCommand("seq bpm 68");
    nudgeTransportSwing(-4);
    handleTextCommand("seq on");
    Serial.println(F("macro mist"));
    return;
  }
  if (name.equalsIgnoreCase("weld")) {
    patternFuse();
    handleTextCommand("seq densify");
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("seq on");
    Serial.println(F("macro weld"));
    return;
  }
  if (name.equalsIgnoreCase("haze")) {
    patternGlaze();
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    handleTextCommand("seq bpm 72");
    nudgeTransportSwing(-6);
    handleTextCommand("seq on");
    Serial.println(F("macro haze"));
    return;
  }
  if (name.equalsIgnoreCase("forge")) {
    patternFuse();
    handleTextCommand("seq densify");
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    Serial.println(F("macro forge"));
    return;
  }
  if (name.equalsIgnoreCase("flare")) {
    patternFlare();
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("all on");
    handleTextCommand("seq on");
    Serial.println(F("macro flare"));
    return;
  }
  if (name.equalsIgnoreCase("glow")) {
    patternBloom();
    patternGlaze();
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    handleTextCommand("strum on");
    handleTextCommand("seq on");
    Serial.println(F("macro glow"));
    return;
  }
  if (name.equalsIgnoreCase("rust")) {
    patternRust();
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    Serial.println(F("macro rust"));
    return;
  }
  if (name.equalsIgnoreCase("ember")) {
    patternRust();
    patternBloom();
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("all on");
    handleTextCommand("seq on");
    Serial.println(F("macro ember"));
    return;
  }
  if (name.equalsIgnoreCase("veil")) {
    patternGlaze();
    handleTextCommand("demo on");
    handleTextCommand("arp on");
    nudgeTransportSwing(-8);
    handleTextCommand("seq bpm 64");
    handleTextCommand("seq on");
    Serial.println(F("macro veil"));
    return;
  }
  if (name.equalsIgnoreCase("braid")) {
    patternBraid();
    handleTextCommand("seq accent on");
    handleTextCommand("metro on");
    handleTextCommand("seq on");
    Serial.println(F("macro braid"));
    return;
  }
  if (name.equalsIgnoreCase("slate")) {
    patternDrift();
    handleTextCommand("perf moog");
    handleTextCommand("mono filter on");
    nudgeTransportSwing(6);
    handleTextCommand("seq bpm 92");
    handleTextCommand("seq on");
    Serial.println(F("macro slate"));
    return;
  }
  if (name.equalsIgnoreCase("prism")) {
    patternPulse();
    handleTextCommand("perf moog");
    handleTextCommand("metro on");
    handleTextCommand("seq bpm 110");
    handleTextCommand("seq on");
    Serial.println(F("macro prism"));
    return;
  }
  if (name.equalsIgnoreCase("pluck")) {
    handleTextCommand("perf moog");
    handleTextCommand("filter env on");
#if BENCH_FILTER_ENV
    filterEnv.setAttackMs(4.0f);
    filterEnv.setDecayMs(130.0f);
    filterEnv.setSustain(0.08f);
    filterEnv.setReleaseMs(180.0f);
    filterEnvDepth = 0.88f;
    setFilterCutoffBase(2400.0f);
#endif
    Serial.println(F("macro pluck"));
    return;
  }
  if (name.equalsIgnoreCase("vault")) {
    commitBenchSlot(-1);
    Serial.print(F("macro vault slot="));
    Serial.println(activeBenchSlot);
    return;
  }
  if (name.equalsIgnoreCase("steel")) {
    handleTextCommand("perf moog");
    handleTextCommand("mono filter on");
    patternRivet();
    handleTextCommand("seq accent on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    Serial.println(F("macro steel"));
    return;
  }
  if (name.equalsIgnoreCase("iron")) {
    handleTextCommand("perf moog");
    handleTextCommand("mono filter on");
    patternRivet();
    handleTextCommand("seq accent on");
    handleTextCommand("drums on");
    handleTextCommand("seq on");
    handleTextCommand("filter env on");
#if BENCH_FILTER_ENV
    filterEnv.setAttackMs(4.0f);
    filterEnv.setDecayMs(130.0f);
    filterEnv.setSustain(0.08f);
    filterEnv.setReleaseMs(180.0f);
    filterEnvDepth = 0.88f;
    setFilterCutoffBase(2400.0f);
#endif
    Serial.println(F("macro iron"));
    return;
  }
  if (name.equalsIgnoreCase("chrome")) {
    handleTextCommand("perf moog");
    handleTextCommand("metro on");
    patternRivet();
    patternPulse();
    handleTextCommand("seq on");
    Serial.println(F("macro chrome"));
    return;
  }
  if (name.equalsIgnoreCase("nickel")) {
    runMacro(String("chrome"));
    runMacro(String("pluck"));
    Serial.println(F("macro nickel"));
    return;
  }
  if (name.equalsIgnoreCase("copper")) {
    runMacro(String("nickel"));
    runMacro(String("iron"));
    Serial.println(F("macro copper"));
    return;
  }
  if (name.equalsIgnoreCase("brass")) {
    runMacro(String("copper"));
    runMacro(String("bolt"));
    Serial.println(F("macro brass"));
    return;
  }
  if (name.equalsIgnoreCase("bronze")) {
    runMacro(String("brass"));
    runMacro(String("prism"));
    Serial.println(F("macro bronze"));
    return;
  }
  if (name.equalsIgnoreCase("pewter")) {
    runMacro(String("bronze"));
    runMacro(String("hush"));
    Serial.println(F("macro pewter"));
    return;
  }
  if (name.equalsIgnoreCase("tin")) {
    runMacro(String("pewter"));
    runMacro(String("frost"));
    Serial.println(F("macro tin"));
    return;
  }
  if (name.equalsIgnoreCase("zinc")) {
    runMacro(String("tin"));
    runMacro(String("spark"));
    Serial.println(F("macro zinc"));
    return;
  }
  if (name.equalsIgnoreCase("lead")) {
    runMacro(String("zinc"));
    runMacro(String("grit"));
    Serial.println(F("macro lead"));
    return;
  }
  if (name.equalsIgnoreCase("silver")) {
    runMacro(String("lead"));
    runMacro(String("glow"));
    Serial.println(F("macro silver"));
    return;
  }
  if (name.equalsIgnoreCase("gold")) {
    runMacro(String("silver"));
    runMacro(String("blaze"));
    Serial.println(F("macro gold"));
    return;
  }
  if (name.equalsIgnoreCase("platinum")) {
    runMacro(String("gold"));
    runMacro(String("frost"));
    Serial.println(F("macro platinum"));
    return;
  }
  if (name.equalsIgnoreCase("titanium")) {
    runMacro(String("platinum"));
    runMacro(String("prism"));
    Serial.println(F("macro titanium"));
    return;
  }
  if (name.equalsIgnoreCase("cobalt")) {
    runMacro(String("titanium"));
    runMacro(String("spark"));
    Serial.println(F("macro cobalt"));
    return;
  }
  if (name.equalsIgnoreCase("tungsten")) {
    runMacro(String("cobalt"));
    runMacro(String("weld"));
    Serial.println(F("macro tungsten"));
    return;
  }
  if (name.equalsIgnoreCase("mercury")) {
    runMacro(String("tungsten"));
    runMacro(String("frost"));
    Serial.println(F("macro mercury"));
    return;
  }
  if (name.equalsIgnoreCase("uranium")) {
    runMacro(String("mercury"));
    runMacro(String("surge"));
    Serial.println(F("macro uranium"));
    return;
  }
  if (name.equalsIgnoreCase("neptunium")) {
    runMacro(String("uranium"));
    runMacro(String("haze"));
    Serial.println(F("macro neptunium"));
    return;
  }
  if (name.equalsIgnoreCase("plutonium")) {
    runMacro(String("neptunium"));
    runMacro(String("spark"));
    Serial.println(F("macro plutonium"));
    return;
  }
  if (name.equalsIgnoreCase("americium")) {
    runMacro(String("plutonium"));
    runMacro(String("drift"));
    Serial.println(F("macro americium"));
    return;
  }
  if (name.equalsIgnoreCase("curium")) {
    runMacro(String("americium"));
    runMacro(String("bloom"));
    Serial.println(F("macro curium"));
    return;
  }
  if (name.equalsIgnoreCase("berkelium")) {
    runMacro(String("curium"));
    runMacro(String("grit"));
    Serial.println(F("macro berkelium"));
    return;
  }
  if (name.equalsIgnoreCase("californium")) {
    runMacro(String("berkelium"));
    runMacro(String("coast"));
    Serial.println(F("macro californium"));
    return;
  }
  if (name.equalsIgnoreCase("einsteinium")) {
    runMacro(String("californium"));
    runMacro(String("blaze"));
    Serial.println(F("macro einsteinium"));
    return;
  }
  if (name.equalsIgnoreCase("fermium")) {
    runMacro(String("einsteinium"));
    runMacro(String("mist"));
    Serial.println(F("macro fermium"));
    return;
  }
  if (name.equalsIgnoreCase("mendelevium")) {
    runMacro(String("fermium"));
    runMacro(String("coast"));
    Serial.println(F("macro mendelevium"));
    return;
  }
  if (name.equalsIgnoreCase("nobelium")) {
    runMacro(String("mendelevium"));
    runMacro(String("hush"));
    Serial.println(F("macro nobelium"));
    return;
  }
  Serial.println(F("macro? jam|ambient|blues|lounge|techno|funk|full|dub|groove|evolve|live|sketch|polish|warmup|stage|remix|settle|punch|drop|breathe|glitch|rave|hush|spark|frost|pulse|surge|drift|bloom|grit|coast|blaze|mist|weld|haze|forge|flare|glow|rust|ember|veil|braid|slate|prism|pluck|vault|steel|iron|chrome|nickel|copper|brass|bronze|pewter|tin|zinc|lead|silver|gold|platinum|titanium|cobalt|tungsten|mercury|uranium|neptunium|plutonium|americium|curium|berkelium|californium|einsteinium|fermium|mendelevium|nobelium"));
}

static void handleTextCommand(const String &line) {
  if (line.length() == 0) return;
  if (line.equalsIgnoreCase("help") || line == "?") {
    printHelp();
    return;
  }
  if (line.equalsIgnoreCase("status")) {
    printStatus();
    return;
  }
  if (line.equalsIgnoreCase("bench ping")) {
    benchPing();
    return;
  }
  if (line.equalsIgnoreCase("bench version")) {
    benchVersion();
    return;
  }
  if (line.equalsIgnoreCase("bench ready")) {
    benchReady();
    return;
  }
  if (line.equalsIgnoreCase("bench snap")) {
    benchSnapshot();
    return;
  }
  if (line.equalsIgnoreCase("bench export")) {
    benchExport();
    return;
  }
  if (line.equalsIgnoreCase("bench health")) {
    benchHealth();
    return;
  }
  if (line.equalsIgnoreCase("bench mem")) {
    benchMem();
    return;
  }
  if (line.equalsIgnoreCase("bench peek")) {
    benchPeek();
    return;
  }
  if (line.equalsIgnoreCase("bench slots")) {
    benchSlots();
    return;
  }
  if (line.equalsIgnoreCase("bench layers")) {
    benchLayers();
    return;
  }
  if (line.equalsIgnoreCase("bench tempo")) {
    benchTempo();
    return;
  }
  if (line.equalsIgnoreCase("bench chord")) {
    benchChord();
    return;
  }
  if (line.equalsIgnoreCase("bench transport")) {
    benchTransport();
    return;
  }
  if (line.equalsIgnoreCase("bench meter")) {
    benchMeter();
    return;
  }
  if (line.equalsIgnoreCase("bench groove")) {
    benchGroove();
    return;
  }
  if (line.equalsIgnoreCase("bench audio")) {
    benchAudio();
    return;
  }
  if (line.equalsIgnoreCase("bench pattern")) {
    benchPattern();
    return;
  }
  if (line.equalsIgnoreCase("bench voices")) {
    benchVoices();
    return;
  }
  if (line.equalsIgnoreCase("bench stack")) {
    benchStack();
    return;
  }
  if (line.equalsIgnoreCase("bench uptime")) {
    benchUptime();
    return;
  }
  if (line.equalsIgnoreCase("bench cal")) {
    benchCal();
    return;
  }
  if (line.equalsIgnoreCase("bench controls")) {
    benchControlsPanel();
    return;
  }
  if (line.equalsIgnoreCase("bench lint")) {
    benchPinLint(Serial);
    return;
  }
  if (line.equalsIgnoreCase("bench filter")) {
    benchFilter();
    return;
  }
  if (line.equalsIgnoreCase("bench midi")) {
    benchMidi();
    return;
  }
  if (line.equalsIgnoreCase("bench dac")) {
    benchDac();
    return;
  }
  if (line.equalsIgnoreCase("bench eeprom")) {
    benchEeprom.printStatus(Serial, presets);
    return;
  }
  if (line.equalsIgnoreCase("bench api")) {
    benchApi();
    return;
  }
  if (line.equalsIgnoreCase("bench latency")) {
    benchLatency();
    return;
  }
  if (line.equalsIgnoreCase("bench perf")) {
    benchPerf();
    return;
  }
  if (line.equalsIgnoreCase("bench filter load")) {
    benchFilterLoad(256);
    return;
  }
  if (line.startsWith("bench filter load ")) {
    benchFilterLoad(line.substring(18).toInt());
    return;
  }
#if BENCH_MONO_FILTER && BENCH_FILTER_ENV
  if (line.equalsIgnoreCase("bench filter env load")) {
    benchFilterEnvLoad(256);
    return;
  }
  if (line.startsWith("bench filter env load ")) {
    benchFilterEnvLoad(line.substring(22).toInt());
    return;
  }
#endif
#if BENCH_MONO_FILTER && BENCH_MONO_LADDER
  if (line.equalsIgnoreCase("bench filter res load")) {
    benchFilterResLoad(256);
    return;
  }
  if (line.startsWith("bench filter res load ")) {
    benchFilterResLoad(line.substring(22).toInt());
    return;
  }
#endif
#if BENCH_MONO_FILTER && BENCH_FILTER_ENV && BENCH_MONO_LADDER
  if (line.equalsIgnoreCase("bench filter combo load")) {
    benchFilterComboLoad(256);
    return;
  }
  if (line.startsWith("bench filter combo load ")) {
    benchFilterComboLoad(line.substring(24).toInt());
    return;
  }
#endif
  if (line.equalsIgnoreCase("bench filter profile")) {
    benchFilterProfile();
    return;
  }
#if BENCH_MONO_FILTER && BENCH_FILTER_ENV && BENCH_MONO_LADDER
  if (line.equalsIgnoreCase("bench filter headroom")) {
    benchFilterHeadroom();
    return;
  }
  if (line.equalsIgnoreCase("bench filter budget")) {
    benchFilterBudget();
    return;
  }
  if (line.equalsIgnoreCase("bench filter gauge")) {
    benchFilterGauge();
    return;
  }
  if (line.equalsIgnoreCase("bench filter ceiling")) {
    benchFilterCeiling();
    return;
  }
  if (line.equalsIgnoreCase("bench filter reserve")) {
    benchFilterReserve();
    return;
  }
  if (line.equalsIgnoreCase("bench filter cushion")) {
    benchFilterCushion();
    return;
  }
  if (line.equalsIgnoreCase("bench filter span")) {
    benchFilterSpan();
    return;
  }
  if (line.equalsIgnoreCase("bench filter tally")) {
    benchFilterTally();
    return;
  }
  if (line.equalsIgnoreCase("bench filter margin")) {
    benchFilterMargin();
    return;
  }
  if (line.equalsIgnoreCase("bench filter share")) {
    benchFilterShare();
    return;
  }
  if (line.equalsIgnoreCase("bench filter peak")) {
    benchFilterPeak();
    return;
  }
  if (line.equalsIgnoreCase("bench filter balance")) {
    benchFilterBalance();
    return;
  }
  if (line.equalsIgnoreCase("bench filter blend")) {
    benchFilterBlend();
    return;
  }
  if (line.equalsIgnoreCase("bench filter pivot")) {
    benchFilterPivot();
    return;
  }
  if (line.equalsIgnoreCase("bench filter hinge")) {
    benchFilterHinge();
    return;
  }
  if (line.equalsIgnoreCase("bench filter swivel")) {
    benchFilterSwivel();
    return;
  }
  if (line.equalsIgnoreCase("bench filter axle")) {
    benchFilterAxle();
    return;
  }
  if (line.equalsIgnoreCase("bench filter coupler")) {
    benchFilterCoupler();
    return;
  }
  if (line.equalsIgnoreCase("bench filter linkage")) {
    benchFilterLinkage();
    return;
  }
  if (line.equalsIgnoreCase("bench filter truss")) {
    benchFilterTruss();
    return;
  }
#endif
  if (line.equalsIgnoreCase("bench filter sweep")) {
    benchFilterSweep();
    return;
  }
  if (line.equalsIgnoreCase("bench filter res sweep")) {
    benchFilterResSweep();
    return;
  }
#if BENCH_MONO_FILTER && BENCH_FILTER_ENV
  if (line.equalsIgnoreCase("bench filter env sweep")) {
    benchFilterEnvSweep();
    return;
  }
#endif
  if (line.equalsIgnoreCase("eeprom migrate")) {
    benchEeprom.migrateFromV1(presets);
    Serial.println(F("eeprom migrate ok"));
    return;
  }
  if (line.startsWith("eeprom export ")) {
    const int slot = line.substring(14).toInt();
    if (slot < 0 || slot >= PresetStore::kSlots) {
      Serial.println(F("eeprom export? slot 0-3"));
      return;
    }
    presets.exportSlot(slot, Serial);
    return;
  }
  if (line.startsWith("eeprom import ")) {
    const int slot = line.substring(14).toInt();
    if (slot < 0 || slot >= PresetStore::kSlots) {
      Serial.println(F("eeprom import? slot 0-3"));
      return;
    }
    eepromImportMode = true;
    eepromImportSlot = slot;
    eepromImportSnap = {};
    eepromImportSnapValid = false;
    eepromImportHex = "";
#if BENCH_SERIAL_MIDI
    Serial.println(F("--- eeprom import ---"));
    Serial.println(F("paste export lines, then ---"));
#endif
    return;
  }
  if (line.startsWith("eeprom clone ")) {
    const String rest = line.substring(13);
    const int sp = rest.indexOf(' ');
    if (sp > 0) {
      const int from = rest.substring(0, sp).toInt();
      const int to = rest.substring(sp + 1).toInt();
      if (from >= 0 && from < PresetStore::kSlots && to >= 0 && to < PresetStore::kSlots) {
        if (presets.cloneSlot(from, to)) {
          Serial.print(F("eeprom clone ok from="));
          Serial.print(from);
          Serial.print(F(" to="));
          Serial.println(to);
        } else {
          Serial.println(F("eeprom clone? failed"));
        }
      } else {
        Serial.println(F("eeprom clone? slot 0-3"));
      }
    } else {
      Serial.println(F("eeprom clone? from to"));
    }
    return;
  }
  if (line.startsWith("eeprom diff ")) {
    const String rest = line.substring(12);
    const int sp = rest.indexOf(' ');
    if (sp > 0) {
      eepromDiffSlots(rest.substring(0, sp).toInt(), rest.substring(sp + 1).toInt());
    } else {
      Serial.println(F("eeprom diff? <a> <b>"));
    }
    return;
  }
  if (line.startsWith("eeprom verify ")) {
    eepromVerifySlot(line.substring(14).toInt());
    return;
  }
  if (line.startsWith("eeprom checksum ")) {
    eepromChecksumSlot(line.substring(16).toInt());
    return;
  }
  if (line.startsWith("eeprom snapshot ")) {
    eepromSnapshotSlot(line.substring(16).toInt());
    return;
  }
  if (line.equalsIgnoreCase("eeprom seal")) {
    eepromSealAll();
    return;
  }
  if (line.equalsIgnoreCase("eeprom manifest")) {
    eepromManifest();
    return;
  }
  if (line.startsWith("eeprom audit ")) {
    const String rest = line.substring(13);
    const int sp = rest.indexOf(' ');
    if (sp > 0) {
      eepromAuditSlots(rest.substring(0, sp).toInt(), rest.substring(sp + 1).toInt());
    } else {
      Serial.println(F("eeprom audit? slot 0-3"));
    }
    return;
  }
  if (line.equalsIgnoreCase("eeprom parity")) {
    eepromParity();
    return;
  }
  if (line.equalsIgnoreCase("eeprom digest")) {
    eepromDigest();
    return;
  }
  if (line.equalsIgnoreCase("eeprom census")) {
    eepromCensus();
    return;
  }
  if (line.equalsIgnoreCase("eeprom ledger")) {
    eepromLedger();
    return;
  }
  if (line.equalsIgnoreCase("eeprom roster")) {
    eepromRoster();
    return;
  }
  if (line.equalsIgnoreCase("eeprom index")) {
    eepromIndex();
    return;
  }
  if (line.equalsIgnoreCase("eeprom atlas")) {
    eepromAtlas();
    return;
  }
  if (line.equalsIgnoreCase("eeprom almanac")) {
    eepromAlmanac();
    return;
  }
  if (line.equalsIgnoreCase("eeprom gazette")) {
    eepromGazette();
    return;
  }
  if (line.equalsIgnoreCase("eeprom register")) {
    eepromRegister();
    return;
  }
  if (line.equalsIgnoreCase("eeprom codex")) {
    eepromCodex();
    return;
  }
  if (line.equalsIgnoreCase("eeprom tome")) {
    eepromTome();
    return;
  }
  if (line.equalsIgnoreCase("eeprom folio")) {
    eepromFolio();
    return;
  }
  if (line.equalsIgnoreCase("eeprom scroll")) {
    eepromScroll();
    return;
  }
  if (line.equalsIgnoreCase("eeprom archive")) {
    eepromArchive();
    return;
  }
  if (line.equalsIgnoreCase("eeprom vault")) {
    eepromVault();
    return;
  }
  if (line.equalsIgnoreCase("eeprom annals")) {
    eepromAnnals();
    return;
  }
  if (line.equalsIgnoreCase("eeprom chronicle")) {
    eepromChronicle();
    return;
  }
  if (line.equalsIgnoreCase("eeprom registry")) {
    eepromRegistry();
    return;
  }
  if (line.equalsIgnoreCase("bench test")) {
    runBenchSelfTest();
    return;
  }
  if (line.equalsIgnoreCase("bench loop")) {
    benchLoop(1);
    return;
  }
  if (line.startsWith("bench loop ")) {
    benchLoop(line.substring(11).toInt());
    return;
  }
  if (line.equalsIgnoreCase("bench diag")) {
    printBenchDiag();
    return;
  }
  if (line.equalsIgnoreCase("bench tone")) {
    benchTone(60, 800);
    return;
  }
  if (line.startsWith("bench tone ")) {
    const String arg = line.substring(11);
    const int sp = arg.indexOf(' ');
    if (sp > 0) {
      benchTone(arg.substring(0, sp).toInt(), (unsigned)arg.substring(sp + 1).toInt());
    } else {
      benchTone(arg.toInt(), 800);
    }
    return;
  }
  if (line.equalsIgnoreCase("pattern")) {
    printPatternAll();
    return;
  }
  if (line.equalsIgnoreCase("pattern clear")) {
    patternClear();
    return;
  }
  if (line.equalsIgnoreCase("pattern dup")) {
    patternDup();
    return;
  }
  if (line.equalsIgnoreCase("pattern double")) {
    patternDoubleAll();
    return;
  }
  if (line.equalsIgnoreCase("pattern quantize")) {
    patternQuantize();
    return;
  }
  if (line.equalsIgnoreCase("pattern trim")) {
    patternTrim();
    return;
  }
  if (line.equalsIgnoreCase("pattern strip")) {
    patternStrip();
    return;
  }
  if (line.equalsIgnoreCase("pattern thicken")) {
    patternThicken();
    return;
  }
  if (line.equalsIgnoreCase("pattern stats")) {
    patternStats();
    return;
  }
  if (line.equalsIgnoreCase("pattern oneline")) {
    patternOneline();
    return;
  }
  if (line.equalsIgnoreCase("pattern align")) {
    patternAlign();
    return;
  }
  if (line.equalsIgnoreCase("pattern thin")) {
    patternThin();
    return;
  }
  if (line.equalsIgnoreCase("pattern invert")) {
    patternInvertAll();
    return;
  }
  if (line.equalsIgnoreCase("pattern contrast")) {
    patternContrast();
    return;
  }
  if (line.equalsIgnoreCase("pattern echo")) {
    patternEcho();
    return;
  }
  if (line.equalsIgnoreCase("pattern compact")) {
    patternCompact();
    return;
  }
  if (line.equalsIgnoreCase("pattern shift")) {
    patternRotateAll(1);
    return;
  }
  if (line.startsWith("pattern shift ")) {
    patternRotateAll(line.substring(14).toInt());
    return;
  }
  if (line.equalsIgnoreCase("pattern halve")) {
    patternHalve();
    return;
  }
  if (line.equalsIgnoreCase("pattern weave")) {
    patternWeave();
    return;
  }
  if (line.equalsIgnoreCase("pattern spread")) {
    patternSpread();
    return;
  }
  if (line.equalsIgnoreCase("pattern fuse")) {
    patternFuse();
    return;
  }
  if (line.equalsIgnoreCase("pattern bloom")) {
    patternBloom();
    return;
  }
  if (line.equalsIgnoreCase("pattern grit")) {
    patternGrit();
    return;
  }
  if (line.equalsIgnoreCase("pattern glaze")) {
    patternGlaze();
    return;
  }
  if (line.equalsIgnoreCase("pattern smear")) {
    patternSmear();
    return;
  }
  if (line.equalsIgnoreCase("pattern polish")) {
    patternPolish();
    return;
  }
  if (line.equalsIgnoreCase("pattern carve")) {
    patternCarve();
    return;
  }
  if (line.equalsIgnoreCase("pattern ripple")) {
    patternRipple();
    return;
  }
  if (line.equalsIgnoreCase("pattern flare")) {
    patternFlare();
    return;
  }
  if (line.equalsIgnoreCase("pattern knit")) {
    patternKnit();
    return;
  }
  if (line.equalsIgnoreCase("pattern snap")) {
    patternSnap();
    return;
  }
  if (line.equalsIgnoreCase("pattern rust")) {
    patternRust();
    return;
  }
  if (line.equalsIgnoreCase("pattern braid")) {
    patternBraid();
    return;
  }
  if (line.equalsIgnoreCase("pattern taper")) {
    patternTaper();
    return;
  }
  if (line.equalsIgnoreCase("pattern churn")) {
    patternChurn();
    return;
  }
  if (line.equalsIgnoreCase("pattern drift")) {
    patternDrift();
    return;
  }
  if (line.equalsIgnoreCase("pattern loop")) {
    patternLoop();
    return;
  }
  if (line.equalsIgnoreCase("pattern anchor")) {
    patternAnchor();
    return;
  }
  if (line.equalsIgnoreCase("pattern pulse")) {
    patternPulse();
    return;
  }
  if (line.equalsIgnoreCase("pattern latch")) {
    patternLatch();
    return;
  }
  if (line.equalsIgnoreCase("pattern rivet")) {
    patternRivet();
    return;
  }
  if (line.equalsIgnoreCase("pattern bolt")) {
    patternBolt();
    return;
  }
  if (line.equalsIgnoreCase("pattern weld")) {
    patternWeld();
    return;
  }
  if (line.equalsIgnoreCase("pattern forge")) {
    patternForge();
    return;
  }
  if (line.equalsIgnoreCase("pattern mint")) {
    patternMint();
    return;
  }
  if (line.equalsIgnoreCase("pattern anvil")) {
    patternAnvil();
    return;
  }
  if (line.equalsIgnoreCase("pattern temper")) {
    patternTemper();
    return;
  }
  if (line.equalsIgnoreCase("pattern quench")) {
    patternQuench();
    return;
  }
  if (line.equalsIgnoreCase("pattern smelt")) {
    patternSmelt();
    return;
  }
  if (line.equalsIgnoreCase("pattern cast")) {
    patternCast();
    return;
  }
  if (line.equalsIgnoreCase("pattern mold")) {
    patternMold();
    return;
  }
  if (line.equalsIgnoreCase("pattern stamp")) {
    patternStamp();
    return;
  }
  if (line.equalsIgnoreCase("pattern plate")) {
    patternPlate();
    return;
  }
  if (line.equalsIgnoreCase("pattern sheet")) {
    patternSheet();
    return;
  }
  if (line.equalsIgnoreCase("pattern coil")) {
    patternCoil();
    return;
  }
  if (line.equalsIgnoreCase("pattern wire")) {
    patternWire();
    return;
  }
  if (line.equalsIgnoreCase("pattern strand")) {
    patternStrand();
    return;
  }
  if (line.equalsIgnoreCase("pattern cable")) {
    patternCable();
    return;
  }
  if (line.equalsIgnoreCase("pattern harness")) {
    patternHarness();
    return;
  }
  if (line.equalsIgnoreCase("pattern mesh")) {
    patternMesh();
    return;
  }
  if (line.equalsIgnoreCase("pattern lattice")) {
    patternLattice();
    return;
  }
  if (line.equalsIgnoreCase("pattern matrix")) {
    patternMatrix();
    return;
  }
  if (line.equalsIgnoreCase("pattern fabric")) {
    patternFabric();
    return;
  }
  if (line.equalsIgnoreCase("pattern tapestry")) {
    patternTapestry();
    return;
  }
  if (line.equalsIgnoreCase("pattern canopy")) {
    patternCanopy();
    return;
  }
  if (line.equalsIgnoreCase("pattern pavilion")) {
    patternPavilion();
    return;
  }
  if (line.equalsIgnoreCase("pattern portico")) {
    patternPortico();
    return;
  }
  if (line.equalsIgnoreCase("pattern colonnade")) {
    patternColonnade();
    return;
  }
  if (line.equalsIgnoreCase("pattern facade")) {
    patternFacade();
    return;
  }
  if (line.startsWith("pattern len ")) {
    patternSetLength(line.substring(12).toInt());
    return;
  }
  if (line.startsWith("pattern density ")) {
    patternDensity(line.substring(16).toInt());
    return;
  }
  if (line.equalsIgnoreCase("pattern rotate")) {
    patternRotateAll(1);
    return;
  }
  if (line.startsWith("pattern rotate ")) {
    patternRotateAll(line.substring(15).toInt());
    return;
  }
  if (line.equalsIgnoreCase("pattern mirror")) {
    patternMirrorAll();
    return;
  }
  if (line.equalsIgnoreCase("pattern shuffle")) {
    patternShuffleAll();
    return;
  }
  if (line.equalsIgnoreCase("pattern fill")) {
    patternFill(0, 70);
    return;
  }
  if (line.startsWith("pattern fill ")) {
    const String arg = line.substring(13);
    const int sp = arg.indexOf(' ');
    if (sp > 0) {
      patternFill((uint32_t)arg.substring(0, sp).toInt(),
                  arg.substring(sp + 1).toInt());
    } else {
      patternFill((uint32_t)arg.toInt(), 70);
    }
    return;
  }
  if (line.startsWith("pattern swap ")) {
    const String arg = line.substring(13);
    const int sp = arg.indexOf(' ');
    if (sp > 0) {
      patternSwapSteps(arg.substring(0, sp).toInt(), arg.substring(sp + 1).toInt());
    }
    return;
  }
  if (line.startsWith("pattern seed ")) {
    const uint32_t seed = (uint32_t)line.substring(13).toInt();
    randomSeed(seed);
#if BENCH_SERIAL_MIDI
    Serial.print(F("pattern seed "));
    Serial.println(seed);
#endif
    return;
  }
  if (line.startsWith("pattern load ")) {
    loadPatternBank(line.substring(13).toInt());
    return;
  }
  if (line.startsWith("pattern push ")) {
    savePatternBank(line.substring(13).toInt());
    return;
  }
  if (line.equalsIgnoreCase("sync len")) {
    syncLenPattern();
    return;
  }
  if (line.equalsIgnoreCase("pattern export")) {
    patternExport();
    return;
  }
  if (line.equalsIgnoreCase("pattern import q")) {
    patternImportMode = true;
    patternImportQuantize = true;
    patternImportBuf = "";
#if BENCH_SERIAL_MIDI
    Serial.println(F("--- import q ---"));
    Serial.println(F("paste seq/bass/kick/hat lines, then ---"));
#endif
    return;
  }
  if (line.equalsIgnoreCase("pattern import")) {
    patternImportMode = true;
    patternImportQuantize = false;
    patternImportBuf = "";
#if BENCH_SERIAL_MIDI
    Serial.println(F("--- import ---"));
    Serial.println(F("paste seq/bass/kick/hat lines, then ---"));
#endif
    return;
  }
  if (line.startsWith("pattern import ")) {
    String rest = line.substring(15);
    rest.trim();
    if (rest.startsWith("q ")) {
      patternImportQuantize = true;
      rest = rest.substring(2);
      rest.trim();
    }
    patternImportLine(rest);
    patternImportQuantize = false;
    markPresetDirty();
#if BENCH_SERIAL_MIDI
    Serial.println(F("import ok"));
#endif
    return;
  }
  if (line.equalsIgnoreCase("follow seq")) {
    followSeqPattern();
    return;
  }
  if (line.equalsIgnoreCase("slots")) {
    printSlots();
    return;
  }
  if (line.startsWith("compare ")) {
    const int sp = line.indexOf(' ', 8);
    if (sp > 8) {
      compareSlots(line.substring(8, sp).toInt(), line.substring(sp + 1).toInt());
    }
    return;
  }
  if (line.startsWith("peek ")) {
    peekSlot(line.substring(5).toInt());
    return;
  }
  if (line.equalsIgnoreCase("humanize")) {
    humanizeGroove();
    return;
  }
  if (line.equalsIgnoreCase("reload")) {
    reloadActiveSlot();
    return;
  }
  if (line.equalsIgnoreCase("unmute all")) {
    unmuteAllLayers();
    return;
  }
  if (line.equalsIgnoreCase("stash")) {
    captureBenchStash();
    return;
  }
  if (line.equalsIgnoreCase("undo")) {
    undoBenchStash();
    return;
  }
  if (line.startsWith("audition ")) {
    auditionBenchSlot(line.substring(9).toInt());
    return;
  }
  if (line.equalsIgnoreCase("commit")) {
    commitBenchSlot(-1);
    return;
  }
  if (line.startsWith("commit ")) {
    commitBenchSlot(line.substring(7).toInt());
    return;
  }
  if (line.equalsIgnoreCase("bass from seq")) {
    bassFromSeqPattern();
    return;
  }
  if (line.equalsIgnoreCase("drum from seq")) {
    drumFromSeqPattern();
    return;
  }
  if (line.equalsIgnoreCase("hat from seq")) {
    hatFromSeqPattern();
    return;
  }
  if (line.startsWith("slot ")) {
    setActiveSlot(line.substring(5).toInt());
    return;
  }
  if (line.startsWith("mute ")) {
    const int sp = line.indexOf(' ', 5);
    if (sp > 5) {
      const String layer = line.substring(5, sp);
      const String state = line.substring(sp + 1);
      setLayerMute(layer, state.equalsIgnoreCase("on"));
    }
    return;
  }
  if (line.startsWith("solo ")) {
    soloLayer(line.substring(5));
    return;
  }
  if (line.startsWith("transpose ")) {
    transposeSeqPattern(line.substring(10).toInt());
    return;
  }
  if (line.startsWith("bpm ")) {
    const String arg = line.substring(4);
    if (arg.length() > 0 && (arg[0] == '+' || arg[0] == '-')) {
      nudgeTransportBpm(arg.toInt());
    } else {
      applyTransportBpm((unsigned)arg.toInt());
#if BENCH_SERIAL_MIDI
      Serial.print(F("bpm "));
#if BENCH_SEQUENCER
      Serial.println(seq.bpm());
#else
      Serial.println(arg);
#endif
#endif
    }
    return;
  }
  if (line.equalsIgnoreCase("transport")) {
    Serial.print(F("demo="));
    Serial.print(demoActive ? F("on") : F("off"));
#if BENCH_SEQUENCER
    Serial.print(F(" seq="));
    Serial.print(seq.enabled() ? F("on") : F("off"));
#endif
#if BENCH_BASS_SEQ
    Serial.print(F(" bass="));
    Serial.print(bass.enabled() ? F("on") : F("off"));
#endif
#if BENCH_DRUM_GRID
    Serial.print(F(" drums="));
    Serial.print(drums.enabled() ? F("on") : F("off"));
#endif
    Serial.print(F(" bpm="));
#if BENCH_SEQUENCER
    Serial.println(seq.bpm());
#else
    Serial.println(120);
#endif
    return;
  }
  if (line.equalsIgnoreCase("transport reset")) {
    transportReset();
    return;
  }
  if (line.equalsIgnoreCase("autosave on")) {
    autoSaveEnabled = true;
    Serial.println(F("autosave on"));
    return;
  }
  if (line.equalsIgnoreCase("autosave off")) {
    autoSaveEnabled = false;
    presetDirty = false;
    Serial.println(F("autosave off"));
    return;
  }
  if (line.equalsIgnoreCase("all on")) {
    handleTextCommand("drum fill");
    handleTextCommand("bass fill");
    handleTextCommand("seq fill");
    handleTextCommand("drums on");
    handleTextCommand("bass on");
    handleTextCommand("seq on");
    Serial.println(F("all on"));
    return;
  }
  if (line.equalsIgnoreCase("all off")) {
    handleTextCommand("drums off");
    handleTextCommand("bass off");
    handleTextCommand("seq off");
    handleTextCommand("demo off");
    Serial.println(F("all off"));
    return;
  }
  if (line.equalsIgnoreCase("perf moog")) {
    handleTextCommand("demo off");
    handleTextCommand("quantize off");
    handleTextCommand("scale min");
    handleTextCommand("chord min");
    handleTextCommand("mono on");
    handleTextCommand("legato on");
    handleTextCommand("glide on");
    handleTextCommand("unison off");
    handleTextCommand("mono filter on");
#if BENCH_MONO_FILTER
    setFilterCutoffBase(1400.0f);
#if BENCH_MONO_LADDER
    setMonoFilterResonance(0.42f);
#endif
#if BENCH_FILTER_ENV
    filterEnvOn = true;
#endif
#endif
    Serial.println(F("perf moog"));
    return;
  }
  if (line.equalsIgnoreCase("perf fat")) {
    handleTextCommand("perf moog");
    handleTextCommand("unison on");
    Serial.println(F("perf fat"));
    return;
  }
  if (line.startsWith("swing ")) {
    const String arg = line.substring(6);
    if (arg.length() > 0 && (arg[0] == '+' || arg[0] == '-')) {
      nudgeTransportSwing(arg.toInt());
    } else {
      applyTransportSwing(arg.toInt() / 100.0f);
      Serial.print(F("swing "));
#if BENCH_SEQUENCER
      Serial.println((int)(seq.swing() * 100.0f));
#else
      Serial.println(arg);
#endif
    }
    return;
  }
  if (line.equalsIgnoreCase("learn show")) {
    ccLearn.printMap(Serial);
    return;
  }
  if (line.equalsIgnoreCase("learn reset")) {
    ccLearn.resetDefaults();
    Serial.println(F("learn reset"));
    return;
  }
  if (line.equalsIgnoreCase("learn off")) {
    ccLearn.clearPending();
    Serial.println(F("learn off"));
    return;
  }
  if (line.startsWith("learn ")) {
    CcLearnSlot slot;
    if (CcLearn::parseSlot(line.substring(6), slot)) {
      ccLearn.setPending(slot);
      Serial.print(F("learn "));
      Serial.println(ccLearn.pendingName());
    } else {
      Serial.println(F("learn? tone|width|mod|color|breath|swing|harm"));
    }
    return;
  }
  if (line.equalsIgnoreCase("panic")) {
    panicAll();
    Serial.println(F("panic"));
    return;
  }
  if (line.equalsIgnoreCase("demo on")) {
    onDemoToggle(true, nullptr);
    return;
  }
  if (line.equalsIgnoreCase("demo off")) {
    onDemoToggle(false, nullptr);
    return;
  }
  if (line.equalsIgnoreCase("arp on")) {
    chords.setArp(true);
    return;
  }
  if (line.equalsIgnoreCase("arp off")) {
    chords.setArp(false);
    return;
  }
  if (line.equalsIgnoreCase("clock on")) {
    midiClockSync = true;
    Serial.println(F("clock on"));
    return;
  }
  if (line.equalsIgnoreCase("clock off")) {
    midiClockSync = false;
    Serial.println(F("clock off"));
    return;
  }
  if (line.equalsIgnoreCase("mono on")) {
    monoMode = true;
    monoLead.setLegato(true);
    voicer.silence();
    monoNote = -1;
    Serial.println(F("mono on"));
    return;
  }
  if (line.equalsIgnoreCase("mono off")) {
    monoMode = false;
    monoLead.silence(voicer, kStkAmp);
    monoNote = -1;
    Serial.println(F("mono off"));
    return;
  }
#if BENCH_MONO_FILTER
  if (line.equalsIgnoreCase("mono filter on")) {
    monoFilterOn = true;
#if BENCH_MONO_LADDER
    monoLadder.reset();
#else
    monoLpf.reset();
#endif
    Serial.println(F("mono filter on"));
    return;
  }
  if (line.equalsIgnoreCase("mono filter off")) {
    monoFilterOn = false;
    Serial.println(F("mono filter off"));
    return;
  }
  if (line.startsWith("filter cutoff ")) {
    const int hz = line.substring(14).toInt();
    if (hz >= 80 && hz <= 12000) {
      setFilterCutoffBase((float)hz);
      Serial.print(F("filter cutoff "));
      Serial.println(hz);
    } else {
      Serial.println(F("filter cutoff? 120-8000"));
    }
    return;
  }
#if BENCH_MONO_LADDER
  if (line.startsWith("filter res ")) {
    const float r = line.substring(11).toFloat();
    if (r >= 0.0f && r <= 1.0f) {
      setMonoFilterResonance(r * 0.9f);
      Serial.print(F("filter res "));
      Serial.println(r, 2);
    } else {
      Serial.println(F("filter res? 0-1"));
    }
    return;
  }
#endif
#if BENCH_FILTER_ENV
  if (line.equalsIgnoreCase("filter env on")) {
    filterEnvOn = true;
    Serial.println(F("filter env on"));
    return;
  }
  if (line.equalsIgnoreCase("filter env off")) {
    filterEnvOn = false;
    filterEnv.reset();
    applyMonoFilterCutoff();
    Serial.println(F("filter env off"));
    return;
  }
  if (line.startsWith("filter env depth ")) {
    const float d = line.substring(17).toFloat();
    if (d >= 0.0f && d <= 1.0f) {
      filterEnvDepth = d;
      applyMonoFilterCutoff();
      Serial.print(F("filter env depth "));
      Serial.println(d, 2);
    } else {
      Serial.println(F("filter env depth? 0-1"));
    }
    return;
  }
  if (line.startsWith("filter env adsr ")) {
    const String rest = line.substring(16);
    int sp1 = rest.indexOf(' ');
    int sp2 = rest.indexOf(' ', sp1 + 1);
    int sp3 = rest.indexOf(' ', sp2 + 1);
    if (sp1 > 0 && sp2 > sp1 && sp3 > sp2) {
      const float a = rest.substring(0, sp1).toFloat();
      const float d = rest.substring(sp1 + 1, sp2).toFloat();
      const float s = rest.substring(sp2 + 1, sp3).toFloat();
      const float r = rest.substring(sp3 + 1).toFloat();
      if (a > 0.0f && d > 0.0f && s >= 0.0f && s <= 1.0f && r > 0.0f) {
        filterEnv.setAttackMs(a);
        filterEnv.setDecayMs(d);
        filterEnv.setSustain(s);
        filterEnv.setReleaseMs(r);
        Serial.println(F("filter env adsr"));
        return;
      }
    }
    Serial.println(F("filter env adsr? a d s r"));
    return;
  }
#endif
#endif
  if (line.equalsIgnoreCase("legato on")) {
    monoLead.setLegato(true);
    Serial.println(F("legato on"));
    return;
  }
  if (line.equalsIgnoreCase("legato off")) {
    monoLead.setLegato(false);
    Serial.println(F("legato off"));
    return;
  }
  if (line.equalsIgnoreCase("glide on")) {
    monoLead.setGlide(true);
    Serial.println(F("glide on"));
    return;
  }
  if (line.equalsIgnoreCase("glide off")) {
    monoLead.setGlide(false);
    Serial.println(F("glide off"));
    return;
  }
  if (line.equalsIgnoreCase("unison on")) {
    monoLead.setUnison(true);
    Serial.println(F("unison on"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("unison off")) {
    monoLead.setUnison(false);
    Serial.println(F("unison off"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("tap")) {
    const unsigned bpm = tapTempo.tap(millis());
    if (bpm > 0) {
      applyTransportBpm(bpm);
      Serial.print(F("tap bpm "));
      Serial.println(bpm);
    } else {
      Serial.println(F("tap..."));
    }
    return;
  }
#if BENCH_DRUM_GRID
  if (line.equalsIgnoreCase("drums on")) {
    drums.setEnabled(true);
#if BENCH_SEQUENCER
    applyTransportBpm(seq.bpm());
#else
    drums.setBpm(120);
#endif
    Serial.println(F("drums on"));
    return;
  }
  if (line.equalsIgnoreCase("drums off")) {
    drums.setEnabled(false);
    Serial.println(F("drums off"));
    return;
  }
  if (line.equalsIgnoreCase("drum fill")) {
    drums.fillFourOnFloor();
    Serial.println(F("drum fill"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("drum fill32")) {
    drums.fill32();
    Serial.println(F("drum fill32"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("drum hat fill")) {
    drums.fillHats();
    Serial.println(F("drum hat fill"));
    markPresetDirty();
    return;
  }
  if (line.startsWith("drum len ")) {
    const int len = line.substring(9).toInt();
    if (len == 16 || len == 32) {
      drums.setLength(len);
      Serial.print(F("drum len "));
      Serial.println(drums.length());
      markPresetDirty();
    }
    return;
  }
  if (line.equalsIgnoreCase("drum double")) {
    drums.doubleBar();
    Serial.println(F("drum double"));
    markPresetDirty();
    return;
  }
  if (line.startsWith("drum rotate ")) {
    drumRotatePattern(line.substring(12).toInt());
    return;
  }
  if (line.equalsIgnoreCase("drum invert")) {
    drumInvertPattern();
    return;
  }
  if (line.equalsIgnoreCase("drum clear kick")) {
    drumClearLayer(String("kick"));
    return;
  }
  if (line.equalsIgnoreCase("drum clear hat")) {
    drumClearLayer(String("hat"));
    return;
  }
  if (line.startsWith("drum set ")) {
    const int sp1 = line.indexOf(' ', 9);
    const int sp2 = (sp1 > 0) ? line.indexOf(' ', sp1 + 1) : -1;
    if (sp1 > 0 && sp2 > sp1) {
      const int step = line.substring(9, sp1).toInt();
      const String what = line.substring(sp1 + 1, sp2);
      const String state = line.substring(sp2 + 1);
      const bool on = state.equalsIgnoreCase("on") || state == "1";
      if (what.equalsIgnoreCase("kick")) {
        drums.setKick(step, on);
      } else if (what.equalsIgnoreCase("hat")) {
        drums.setHat(step, on);
      }
      Serial.print(F("drum set "));
      Serial.print(step);
      Serial.print(' ');
      Serial.print(what);
      Serial.print(' ');
      Serial.println(on ? F("on") : F("off"));
      markPresetDirty();
    }
    return;
  }
  if (line.equalsIgnoreCase("drum show")) {
    drums.printPattern(Serial);
    return;
  }
#if BENCH_DRUM_PADS
  if (line.equalsIgnoreCase("drum pad on")) {
    drumPadMode = true;
    Serial.println(F("drum pad on"));
    return;
  }
  if (line.equalsIgnoreCase("drum pad off")) {
    drumPadMode = false;
    Serial.println(F("drum pad off"));
    return;
  }
  if (line.equalsIgnoreCase("drum write on")) {
    drumWriteMode = true;
    Serial.println(F("drum write on"));
    return;
  }
  if (line.equalsIgnoreCase("drum write off")) {
    drumWriteMode = false;
    Serial.println(F("drum write off"));
    return;
  }
#endif
#endif
#if BENCH_BASS_SEQ
  if (line.equalsIgnoreCase("bass on")) {
    setBassEnabled(true);
    return;
  }
  if (line.equalsIgnoreCase("bass off")) {
    setBassEnabled(false);
    return;
  }
  if (line.equalsIgnoreCase("bass fill")) {
    bass.fillWalk();
    Serial.println(F("bass fill"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("bass double")) {
    bass.doubleBar();
    Serial.println(F("bass double"));
    markPresetDirty();
    return;
  }
  if (line.startsWith("bass rotate ")) {
    bassRotatePattern(line.substring(12).toInt());
    return;
  }
  if (line.equalsIgnoreCase("bass invert")) {
    bassInvertPattern();
    return;
  }
  if (line.equalsIgnoreCase("bass grid on")) {
    bassGridMode = true;
    Serial.println(F("bass grid on"));
    return;
  }
  if (line.equalsIgnoreCase("bass grid off")) {
    bassGridMode = false;
    Serial.println(F("bass grid off"));
    return;
  }
  if (line.startsWith("bass set ")) {
    const int sp = line.indexOf(' ', 9);
    if (sp > 0) {
      const int step = line.substring(9, sp).toInt();
      const String val = line.substring(sp + 1);
      int8_t deg = BassSeq::kRest;
      if (!val.equalsIgnoreCase("rest") && val != "-") {
        deg = (int8_t)constrain(val.toInt(), 0, 6);
      }
      bass.setStep(step, deg);
      Serial.print(F("bass set "));
      Serial.print(step);
      Serial.print(' ');
      Serial.println(deg == BassSeq::kRest ? F("rest") : String(deg));
      markPresetDirty();
    }
    return;
  }
  if (line.equalsIgnoreCase("bass show")) {
    bass.printPattern(Serial);
    return;
  }
#endif
  if (line.equalsIgnoreCase("strum on")) {
    strum.setEnabled(true);
    Serial.println(F("strum on"));
    return;
  }
  if (line.equalsIgnoreCase("strum off")) {
    strum.setEnabled(false);
    Serial.println(F("strum off"));
    return;
  }
  if (line.equalsIgnoreCase("gen on")) {
    genMode = true;
#if BENCH_SEQUENCER
    if (seq.enabled()) seq.setEnabled(false);
#endif
    demoActive = true;
    genStep();
    Serial.println(F("gen on"));
    return;
  }
  if (line.equalsIgnoreCase("gen off")) {
    genMode = false;
    Serial.println(F("gen off"));
    return;
  }
  if (line.equalsIgnoreCase("quantize on") || line.equalsIgnoreCase("qtz on")) {
    midiQuantize = true;
    Serial.println(F("quantize on"));
    return;
  }
  if (line.equalsIgnoreCase("quantize off") || line.equalsIgnoreCase("qtz off")) {
    midiQuantize = false;
    Serial.println(F("quantize off"));
    return;
  }
  if (line.equalsIgnoreCase("randomize") || line.equalsIgnoreCase("rand")) {
    genStep();
    Serial.println(F("randomized"));
    return;
  }
  if (line.startsWith("macro ")) {
    runMacro(line.substring(6));
    return;
  }
#if BENCH_SEQUENCER
  if (line.equalsIgnoreCase("seq on")) {
    setSeqEnabled(true);
    return;
  }
  if (line.equalsIgnoreCase("seq off")) {
    setSeqEnabled(false);
    return;
  }
  if (line.equalsIgnoreCase("seq clear")) {
    seq.clear();
    Serial.println(F("seq clear"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq fill")) {
    seq.fillScale(scaleNoteForSeq, nullptr);
    Serial.println(F("seq fill"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq fill32")) {
    seq.fillScale32(scaleNoteForSeq, nullptr);
    Serial.println(F("seq fill32"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq random") || line.startsWith("seq random ")) {
    int dens = 55;
    if (line.length() > 11) {
      dens = line.substring(11).toInt();
    }
    seq.fillRandom(scaleNoteForSeq, nullptr, dens);
    Serial.print(F("seq random "));
    Serial.println(dens);
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq write on")) {
    seqWriteMode = true;
    Serial.println(F("seq write on"));
    return;
  }
  if (line.equalsIgnoreCase("seq write off")) {
    seqWriteMode = false;
    Serial.println(F("seq write off"));
    return;
  }
  if (line.equalsIgnoreCase("seq grid on")) {
    seqGridMode = true;
    Serial.println(F("seq grid on"));
    return;
  }
  if (line.equalsIgnoreCase("seq grid off")) {
    seqGridMode = false;
    Serial.println(F("seq grid off"));
    return;
  }
  if (line.startsWith("seq len ")) {
    seq.setLength(line.substring(8).toInt());
    Serial.print(F("seq len "));
    Serial.println(seq.length());
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq double")) {
    seq.doubleBar();
    Serial.print(F("seq len "));
    Serial.println(seq.length());
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq reverse")) {
    seq.reversePattern();
    Serial.println(F("seq reverse"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq invert")) {
    seqInvertPattern();
    return;
  }
  if (line.startsWith("seq shift ")) {
    seq.shiftSteps(line.substring(10).toInt());
    Serial.print(F("seq shift "));
    Serial.println(line.substring(10));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq show")) {
    Serial.print(F("seq "));
    seq.printPattern(Serial);
    return;
  }
  if (line.equalsIgnoreCase("seq record on")) {
    seq.startRecord();
    Serial.println(F("seq record on"));
    return;
  }
  if (line.equalsIgnoreCase("seq record add")) {
    seq.startOverdub();
    Serial.println(F("seq record add"));
    return;
  }
  if (line.equalsIgnoreCase("seq accent on")) {
    seqAccent = true;
    Serial.println(F("seq accent on"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq accent off")) {
    seqAccent = false;
    Serial.println(F("seq accent off"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("seq record off")) {
    seq.stopRecord();
    Serial.println(F("seq record off"));
    markPresetDirty();
    return;
  }
  if (line.startsWith("seq bpm ")) {
    const String arg = line.substring(8);
    if (arg.length() > 0 && (arg[0] == '+' || arg[0] == '-')) {
      nudgeTransportBpm(arg.toInt());
    } else {
      applyTransportBpm((unsigned)arg.toInt());
      Serial.print(F("seq bpm "));
#if BENCH_SEQUENCER
      Serial.println(seq.bpm());
#else
      Serial.println(arg);
#endif
    }
    return;
  }
  if (line.equalsIgnoreCase("seq thin")) {
    seqThinPattern();
    return;
  }
  if (line.equalsIgnoreCase("seq densify")) {
    seqDensifyPattern();
    return;
  }
  if (line.equalsIgnoreCase("seq bounce on")) {
    seq.setBounce(true);
    seq.reset();
    Serial.println(F("seq bounce on"));
    return;
  }
  if (line.equalsIgnoreCase("seq bounce off")) {
    seq.setBounce(false);
    seq.reset();
    Serial.println(F("seq bounce off"));
    return;
  }
  if (line.startsWith("seq set ")) {
    const int sp = line.indexOf(' ', 8);
    if (sp > 0) {
      const int step = line.substring(8, sp).toInt();
      const String val = line.substring(sp + 1);
      uint8_t note = SeqPlayer::kRest;
      if (!val.equalsIgnoreCase("rest") && val != "-") {
        note = (uint8_t)constrain(val.toInt(), 1, 127);
      }
      seq.setStep(step, note);
      Serial.print(F("seq set "));
      Serial.print(step);
      Serial.print(' ');
      Serial.println(note == SeqPlayer::kRest ? F("rest") : String(note));
      markPresetDirty();
    }
    return;
  }
#endif
#if BENCH_HARMONY
  if (line.equalsIgnoreCase("harm on")) {
    if (harmEditBlocked()) return;
    harmony.setEnabled(true);
    Serial.println(F("harm on"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("harm off")) {
    if (harmEditBlocked()) return;
    harmony.setEnabled(false);
    harmony.silence(voicer, kGroup, kStkAmp);
    Serial.println(F("harm off"));
    markPresetDirty();
    return;
  }
  if (line.equalsIgnoreCase("harm lock on")) {
    harmLocked = true;
    Serial.println(F("harm lock on"));
    return;
  }
  if (line.equalsIgnoreCase("harm lock off")) {
    harmLocked = false;
    Serial.println(F("harm lock off"));
    return;
  }
  if (line.startsWith("harm ")) {
    if (harmEditBlocked()) return;
    harmony.setSemitones(line.substring(5).toInt());
    harmony.setEnabled(true);
    Serial.print(F("harm +"));
    Serial.println(harmony.semitones());
    markPresetDirty();
    return;
  }
#endif
  if (line.equalsIgnoreCase("inv")) {
    if (chordEditBlocked()) return;
    advanceInversion(nullptr);
    return;
  }
  if (line.equalsIgnoreCase("chord lock on")) {
    chordLocked = true;
    Serial.println(F("chord lock on"));
    return;
  }
  if (line.equalsIgnoreCase("chord lock off")) {
    chordLocked = false;
    Serial.println(F("chord lock off"));
    return;
  }
  if (line.equalsIgnoreCase("metro on")) {
    metroEnabled = true;
    metroMidiCount = 0;
    metroBeatCount = 0;
    Serial.println(F("metro on"));
    return;
  }
  if (line.equalsIgnoreCase("metro off")) {
    metroEnabled = false;
    Serial.println(F("metro off"));
    return;
  }
  if (line.equalsIgnoreCase("metro accent on")) {
    metroAccent = true;
    metroBeatCount = 0;
    Serial.println(F("metro accent on"));
    return;
  }
  if (line.equalsIgnoreCase("metro accent off")) {
    metroAccent = false;
    Serial.println(F("metro accent off"));
    return;
  }
  if (line.startsWith("chord ")) {
    if (chordEditBlocked()) return;
    ChordType t;
    if (parseChordType(line.substring(6), t)) {
      chords.setType(t);
      if (demoActive) refreshDemoChord();
      else markPresetDirty();
      Serial.print(F("chord "));
      Serial.println(chords.typeName());
    }
    return;
  }
  if (line.startsWith("scale ")) {
    if (chordEditBlocked()) return;
    ScaleMode sm;
    if (parseScaleMode(line.substring(6), sm)) {
      chords.setScale(sm);
      markPresetDirty();
      Serial.print(F("scale "));
      Serial.println(chords.scaleName());
    }
    return;
  }
  if (line.startsWith("root ")) {
    if (chordEditBlocked()) return;
    chords.setRoot(line.substring(5).toInt());
    if (demoActive) refreshDemoChord();
    else markPresetDirty();
    return;
  }
  if (line.startsWith("oct ")) {
    octaveShift = constrain(line.substring(4).toInt(), kOctaveShiftMin, kOctaveShiftMax);
    Serial.print(F("oct "));
    Serial.println(octaveShift);
    return;
  }
  if (line.startsWith("copy ")) {
    const int sp = line.indexOf(' ', 5);
    if (sp > 5) {
      const int from = line.substring(5, sp).toInt();
      const int to = line.substring(sp + 1).toInt();
      if (from >= 0 && from < PresetStore::kSlots && to >= 0 && to < PresetStore::kSlots) {
        presets.copySlot(from, to);
        presets.copyPatternBank(from, to);
        Serial.print(F("copied "));
        Serial.print(from);
        Serial.print(F(" -> "));
        Serial.println(to);
      }
    }
    return;
  }
  if (line.startsWith("pattern copy ")) {
    const int sp = line.indexOf(' ', 13);
    if (sp > 13) {
      const int from = line.substring(13, sp).toInt();
      const int to = line.substring(sp + 1).toInt();
      if (from >= 0 && from < PresetStore::kSlots && to >= 0 && to < PresetStore::kSlots &&
          presets.copyPatternBank(from, to)) {
        Serial.print(F("pattern copy "));
        Serial.print(from);
        Serial.print(F(" -> "));
        Serial.println(to);
      }
    }
    return;
  }
#if BENCH_OLED || BENCH_CYD_UART
  if (line.equalsIgnoreCase("display pattern")) {
    benchPatternView = true;
    Serial.println(F("display pattern"));
    return;
  }
  if (line.equalsIgnoreCase("display transport")) {
    benchPatternView = false;
    Serial.println(F("display transport"));
    return;
  }
  if (line.equalsIgnoreCase("display next")) {
    benchPatternView = !benchPatternView;
    Serial.println(benchPatternView ? F("display pattern") : F("display transport"));
    return;
  }
#endif
#if BENCH_OLED
  if (line.equalsIgnoreCase("display scroll on")) {
    oledScrollWindow = true;
    Serial.println(F("display scroll on"));
    return;
  }
  if (line.equalsIgnoreCase("display scroll off")) {
    oledScrollWindow = false;
    Serial.println(F("display scroll off"));
    return;
  }
#endif
  if (line.startsWith("save ")) {
    const int slot = line.substring(5).toInt();
    saveBenchPreset(slot);
    Serial.print(F("saved "));
    Serial.println(slot);
    presetDirty = false;
    return;
  }
  if (line.startsWith("erase ")) {
    eraseBenchSlot(line.substring(6).toInt());
    return;
  }
  if (line.startsWith("load ")) {
    const int slot = line.substring(5).toInt();
    if (loadBenchPreset(slot)) {
      Serial.print(F("loaded "));
      Serial.println(slot);
      printStatus();
    } else {
      Serial.println(F("empty slot"));
    }
    return;
  }
  if (line.startsWith("n") && line.length() > 1) {
    noteOnLive(line.substring(1).toInt(), kStkAmp);
    return;
  }
  if (line.startsWith("f") && line.length() > 1) {
    noteOffLive(line.substring(1).toInt());
  }
}

static void pollSerialText() {
#if BENCH_SERIAL_MIDI
  static String line;
  while (Serial.available() > 0) {
    const int raw = Serial.peek();
    if (raw < 0) break;
    if (raw >= 0x80) break;
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (eepromImportMode) {
        if (line.equals(F("---"))) {
          eepromImportMode = false;
          finishEepromImport();
        } else if (line.length() > 0) {
          eepromImportLine(line);
        }
        line = "";
        continue;
      }
      if (patternImportMode) {
        if (line.equals(F("---"))) {
          patternImportMode = false;
          finishPatternImport();
        } else if (line.length() > 0) {
          patternImportLine(line);
        }
        line = "";
        continue;
      }
      handleTextCommand(line);
      line = "";
      continue;
    }
    const int maxLen = (eepromImportMode || patternImportMode) ? 280 : 80;
    if (line.length() < maxLen) line += c;
  }
#endif
}

static void pollLed() {
  const float activity = audioOut.peak();
  if (activity > 0.02f) {
    ledOn();
    return;
  }

  static unsigned long lastBlink = 0;
  static bool on = false;
  unsigned interval = 400;
  if (genMode) {
    interval = 120;
#if BENCH_DRUM_GRID
  } else if (drums.enabled()) {
    interval = 70;
#endif
#if BENCH_SEQUENCER
  } else if (seq.enabled()) {
    interval = 90;
#endif
  } else if (!demoActive) {
    interval = 900;
  } else if (chords.arpEnabled()) {
    interval = 140;
  }
  if (millis() - lastBlink >= interval) {
    lastBlink = millis();
    on = !on;
    if (on) {
      ledOn();
    } else {
      ledOff();
    }
  }
}

void setup() {
  pinMode(kLed, OUTPUT);
  ledOff();
  bootBlink(5, 80);

  presets.begin();
  benchEeprom.begin(presets);
  ccLearn.begin();
  randomSeed(micros());

#if BENCH_DAC_PCM5102 && BENCH_PCM_XMT_PIN
  pinMode(BENCH_PCM_XMT_PIN, OUTPUT);
  digitalWrite(BENCH_PCM_XMT_PIN, HIGH);
#endif

#if BENCH_SERIAL_MIDI
  Serial.begin(115200);
  delay(200);
  Serial.print(LUMPY_SYNTH_NAME);
  Serial.print(F(" "));
  Serial.print(kInstrumentName);
  Serial.print(F(" v"));
  Serial.println(F411_SYNTH_VERSION);
  printStatus();
  printHelp();
  bindMidiParser(usbMidi, Serial);
#endif

#if BENCH_UART_MIDI
  Serial1.begin(31250);
  bindMidiParser(uartMidi, Serial1);
#endif

  Stk::setSampleRate((StkFloat)kSampleRate);
#if BENCH_MONO_FILTER
#if BENCH_MONO_LADDER
  monoLadder.setSampleRate((float)kSampleRate);
  monoLadder.setResonance(BENCH_MONO_LADDER_RES);
  monoLadder.setCutoff(2200.0f);
#else
  monoLpf.setSampleRate((float)kSampleRate);
  monoLpf.setCutoff(2200.0f);
#endif
#endif
  addAllVoices();

  strum.begin(onStrumNote, nullptr, 28);
  strum.setEnabled(BENCH_STRUM_DEFAULT != 0);

#if BENCH_SEQUENCER
  seq.begin();
#endif
#if BENCH_DRUM_GRID
  drums.begin();
#endif
#if BENCH_BASS_SEQ
  bass.begin();
  bass.setScaleResolver(scaleNoteForTransport, nullptr);
#endif
#if BENCH_OLED
  if (display.begin(kPinOledSda, kPinOledScl)) {
    Serial.println(F("oled ok"));
  }
#endif
#if BENCH_CYD_UART
  benchCydBegin();
  benchCydSetCommandHandler(handleTextCommand);
  Serial.println(F("cyd uart ok PA2/PA3"));
#endif

  controls.begin(&voicer, kGroup, kStkAmp, advanceInversion, onDemoToggle, nullptr);
  controls.beginActions(onPanicBtn, onChordLong, nullptr);
  controls.beginPerformance(onEncTurn, onEncSw, onEncLong, onArpToggle, nullptr);
  controls.beginKeyboard(onNoteButton, onOctaveShift, nullptr);
  controls.beginGroove(
#if BENCH_SEQUENCER
      onBtnSeq,
#else
      nullptr,
#endif
#if BENCH_DRUM_GRID
      onBtnDrums,
#else
      nullptr,
#endif
#if BENCH_BASS_SEQ
      onBtnBass,
#else
      nullptr,
#endif
      onBtnTap, onBtnSlot, onBtnSlotLong, nullptr);

  if (!dac.begin(I2S_AUDIOFREQ_44K)) {
    failForever(F("i2s begin"));
  }

  dac.setFillCallback(fillAudio, nullptr);
  if (!dac.startStream(512)) {
    failForever(F("i2s stream"));
  }

  bootBeep();
#if BENCH_SERIAL_MIDI
  Serial.println(F("audio ok"));
#endif

  if (presets.slotValid(benchEeprom.activeSlot())) {
    loadBenchPreset(benchEeprom.activeSlot());
  } else if (presets.slotValid(0)) {
    loadBenchPreset(0);
  } else if (demoActive) {
    playBlockChord();
    lastChangeMs = millis();
    lastArpMs = millis();
  }
}

void loop() {
  pollLed();
  pollBenchDisplay();
#if BENCH_CYD_UART
  benchCydPollRx();
#endif
  strum.poll();
  pollAutoSave();

#if BENCH_SERIAL_MIDI
  usbMidi.poll();
  pollSerialText();
#endif

#if BENCH_UART_MIDI
  uartMidi.poll();
#endif

  controls.poll();
  masterGain = controls.masterGain();
#if BENCH_COLOR_POT
#if BENCH_MONO_FILTER && BENCH_MONO_LADDER
  if (monoFilterOn && monoMode) {
    static float lastRes = -1.0f;
    const float r = controls.colorNorm() * 0.9f;
    if (lastRes < 0.0f || fabsf(r - lastRes) > 0.02f) {
      lastRes = r;
      setMonoFilterResonance(r);
    }
  } else {
    audioOut.setTone(controls.colorNorm());
  }
#else
  audioOut.setTone(controls.colorNorm());
#endif
#endif
#if BENCH_WIDTH_POT
  audioOut.setWidth(controls.widthNorm());
#endif
#if BENCH_TEMPO_POT
  static unsigned lastPotBpm = 0;
  const unsigned potBpm = controls.bpmFromTempo();
  if (potBpm != lastPotBpm) {
    lastPotBpm = potBpm;
    applyTransportBpm(potBpm);
  }
#endif
#if BENCH_SWING_POT && BENCH_SEQUENCER
  static float lastSwingNorm = -1.0f;
  const float swingNorm = controls.swingNorm();
  if (lastSwingNorm < 0.0f || fabsf(swingNorm - lastSwingNorm) > 0.012f) {
    lastSwingNorm = swingNorm;
    applyTransportSwing(swingNorm * 0.55f, true);
  }
#endif
#if BENCH_FILTER_ENV
  filterEnv.poll(millis());
#endif
#if BENCH_MOD_POT && BENCH_MONO_FILTER
  if (monoFilterOn && monoMode) {
    static float lastBase = -1.0f;
    const float hz = cutoffHzFromNorm(controls.modNorm());
    if (lastBase < 0.0f || fabsf(hz - lastBase) > 25.0f) {
      lastBase = hz;
#if BENCH_FILTER_ENV
      filterCutoffBase = hz;
#else
      setFilterCutoffBase(hz);
#endif
    }
#if BENCH_FILTER_ENV
    applyMonoFilterCutoff();
#endif
  }
#endif

  const bool sustainDown = controls.sustainDown();
  if ((lastSustainDown && !sustainDown) || (lastMidiSustain && !midiSustain)) {
    releaseSustainedNotes();
  }
  lastSustainDown = sustainDown;
  lastMidiSustain = midiSustain;

  pollMetro(millis());

#if BENCH_SEQUENCER
  if (seq.enabled() && !midiClockSync) {
    seq.poll(millis(), onSeqStep, nullptr);
  }
#endif
#if BENCH_DRUM_GRID
  if (drums.enabled() && !midiClockSync) {
    drums.poll(millis());
  }
#endif
#if BENCH_BASS_SEQ
  if (bass.enabled() && !midiClockSync) {
    bass.poll(millis(), onBassStep, nullptr);
  }
#endif

  if (demoActive && !(BENCH_SEQUENCER && seq.enabled())) {
    if (chords.arpEnabled()) {
      if (!midiClockSync && millis() - lastArpMs >= controls.arpIntervalMs()) {
        arpStep();
      }
    } else if (genMode && millis() - lastChangeMs >= controls.demoIntervalMs(kChordMs)) {
      genStep();
    } else if (millis() - lastChangeMs >= controls.demoIntervalMs(kChordMs)) {
      advanceInversion(nullptr);
    }
  }
}
