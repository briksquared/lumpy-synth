#include "bench_controls.h"
#include <Arduino.h>
#include <math.h>

void BenchControls::begin(stk::Voicer *voicer, int group, float stkAmp,
                          ChordStepFn chordStep, DemoToggleFn demoToggle,
                          void *ctx) {
  voicer_ = voicer;
  group_ = group;
  stkAmp_ = stkAmp;
  chordStep_ = chordStep;
  demoToggle_ = demoToggle;
  ctx_ = ctx;
  demoOn_ = BENCH_AUTO_DEMO != 0;

  analogReadResolution(12);

#if BENCH_VOLUME_POT
  pinMode(kPinVol, INPUT_ANALOG);
#endif
#if BENCH_MOD_POT
  pinMode(kPinMod, INPUT_ANALOG);
#endif
#if BENCH_BREATH_POT
  pinMode(kPinBreath, INPUT_ANALOG);
#endif
#if BENCH_COLOR_POT
  pinMode(kPinColor, INPUT_ANALOG);
#endif
#if BENCH_TEMPO_POT
  pinMode(kPinTempo, INPUT_ANALOG);
#endif
#if BENCH_SWING_POT
  pinMode(kPinSwing, INPUT_ANALOG);
#endif
#if BENCH_WIDTH_POT
  pinMode(kPinWidth, INPUT_ANALOG);
#endif

  if (btnChord_.enabled) pinMode(btnChord_.pin, INPUT_PULLUP);
  if (btnDemo_.enabled) pinMode(btnDemo_.pin, INPUT_PULLUP);
  if (btnPanic_.enabled) pinMode(btnPanic_.pin, INPUT_PULLUP);
#if BENCH_LAYER_BUTTONS
  if (btnSeq_.enabled) pinMode(btnSeq_.pin, INPUT_PULLUP);
  if (btnDrums_.enabled) pinMode(btnDrums_.pin, INPUT_PULLUP);
  if (btnBass_.enabled) pinMode(btnBass_.pin, INPUT_PULLUP);
#endif
#if BENCH_TAP_BUTTON
  if (btnTap_.enabled) pinMode(btnTap_.pin, INPUT_PULLUP);
#endif
#if BENCH_SLOT_BUTTON
  if (btnSlot_.enabled) pinMode(btnSlot_.pin, INPUT_PULLUP);
#endif
#if BENCH_ARP_BUTTON
  if (btnArp_.enabled) pinMode(btnArp_.pin, INPUT_PULLUP);
#endif
#if BENCH_SUSTAIN_PEDAL
  pinMode(kPinSustain, INPUT_PULLUP);
#endif
#if BENCH_ENCODER
  encoder_.begin(kPinEncClk, kPinEncDt, kPinEncSw);
#endif
}

void BenchControls::beginActions(PanicFn panic, ChordLongFn chordLong, void *actionCtx) {
  panicFn_ = panic;
  chordLongFn_ = chordLong;
  actionCtx_ = actionCtx;
}

void BenchControls::beginPerformance(EncTurnFn encTurn, EncSwFn encSw,
                                     EncLongFn encLong, ArpToggleFn arpToggle,
                                     void *perfCtx) {
  encTurn_ = encTurn;
  encSw_ = encSw;
  encLong_ = encLong;
  arpToggle_ = arpToggle;
  perfCtx_ = perfCtx;
}

void BenchControls::beginKeyboard(NoteBtnFn noteBtn, OctaveFn octaveFn,
                                  void *kbCtx) {
  noteBtn_ = noteBtn;
  octaveFn_ = octaveFn;
  kbCtx_ = kbCtx;
#if BENCH_NOTE_BUTTONS
  for (int i = 0; i < BENCH_NOTE_BUTTON_COUNT; i++) {
    btnNotes_[i] = {kNotePins[i], true, false, 0};
    pinMode(kNotePins[i], INPUT_PULLUP);
  }
#endif
#if BENCH_OCTAVE_BUTTONS
  if (btnOctDown_.enabled) pinMode(btnOctDown_.pin, INPUT_PULLUP);
  if (btnOctUp_.enabled) pinMode(btnOctUp_.pin, INPUT_PULLUP);
#endif
}

void BenchControls::beginGroove(GrooveBtnFn seq, GrooveBtnFn drums, GrooveBtnFn bass,
                                GrooveBtnFn tap, GrooveBtnFn slot, SlotLongFn slotLong,
                                void *grooveCtx) {
  seqBtn_ = seq;
  drumsBtn_ = drums;
  bassBtn_ = bass;
  tapBtn_ = tap;
  slotBtn_ = slot;
  slotLongFn_ = slotLong;
  grooveCtx_ = grooveCtx;
}

unsigned BenchControls::demoIntervalMs(unsigned presetMs) const {
#if BENCH_TEMPO_POT
  (void)presetMs;
  const float t = 1.0f - tempoNorm_;
  const unsigned span = kDemoMsMax - kDemoMsMin;
  return kDemoMsMin + (unsigned)(t * (float)span);
#else
  return presetMs;
#endif
}

unsigned BenchControls::arpIntervalMs() const {
#if BENCH_TEMPO_POT
  const float t = 1.0f - tempoNorm_;
  return 80 + (unsigned)(t * 720.0f);
#else
  return 220;
#endif
}

unsigned BenchControls::bpmFromTempo() const {
#if BENCH_TEMPO_POT
  const float t = 1.0f - tempoNorm_;
  return 60u + (unsigned)(t * 140.0f);
#else
  return 120u;
#endif
}

float BenchControls::readPot(uint32_t pin, float &smooth) {
  const float raw = (float)analogRead(pin) / 4095.0f;
  smooth += 0.12f * (raw - smooth);
  return smooth;
}

void BenchControls::sendCc(int cc, float normalized) {
  if (!voicer_) return;
  const float value = normalized * 128.0f;
  voicer_->controlChange(cc, value, group_);
}

bool BenchControls::readBtnEdge(BtnMap &b, unsigned debounceMs) {
  if (!b.enabled) return false;
  const bool down = digitalRead(b.pin) == LOW;
  const unsigned long now = millis();
  const bool edge = down && !b.lastDown && (now - b.lastEdgeMs) > debounceMs;
  if (edge) b.lastEdgeMs = now;
  b.lastDown = down;
  return edge;
}

void BenchControls::pollPots() {
#if BENCH_VOLUME_POT
  readPot(kPinVol, volSmooth_);
  masterGain_ = kMasterGainMin + volSmooth_ * (kMasterGainMax - kMasterGainMin);
#endif

#if BENCH_TEMPO_POT
  tempoNorm_ = readPot(kPinTempo, tempoSmooth_);
#endif

#if BENCH_MOD_POT
  const float mod = readPot(kPinMod, modSmooth_);
  if (modSent_ < 0.0f || fabsf(mod - modSent_) > 0.008f) {
    modSent_ = mod;
    sendCc(1, mod);
  }
#endif

#if BENCH_BREATH_POT
  const float breath = readPot(kPinBreath, breathSmooth_);
  if (breathSent_ < 0.0f || fabsf(breath - breathSent_) > 0.008f) {
    breathSent_ = breath;
    sendCc(128, breath);
  }
#endif

#if BENCH_COLOR_POT
  const float color = readPot(kPinColor, colorSmooth_);
  if (colorSent_ < 0.0f || fabsf(color - colorSent_) > 0.008f) {
    colorSent_ = color;
    sendCc(4, color);
  }
#endif

#if BENCH_SWING_POT
  readPot(kPinSwing, swingSmooth_);
#endif

#if BENCH_WIDTH_POT
  readPot(kPinWidth, widthSmooth_);
#endif
}

void BenchControls::pollNoteButtons() {
#if BENCH_NOTE_BUTTONS
  if (!noteBtn_) return;
  for (int i = 0; i < BENCH_NOTE_BUTTON_COUNT; i++) {
    BtnMap &b = btnNotes_[i];
    const bool down = digitalRead(b.pin) == LOW;
    if (down != b.lastDown) {
      if (down) {
        demoOn_ = false;
        if (demoToggle_) demoToggle_(false, ctx_);
      }
      noteBtn_(i, down, kbCtx_);
    }
    b.lastDown = down;
  }
#endif
}

void BenchControls::pollOctaveButtons() {
#if BENCH_OCTAVE_BUTTONS
  if (!octaveFn_) return;
  if (readBtnEdge(btnOctDown_)) octaveFn_(-1, kbCtx_);
  if (readBtnEdge(btnOctUp_)) octaveFn_(+1, kbCtx_);
#endif
}

void BenchControls::pollSlotButton() {
#if BENCH_SLOT_BUTTON
  if (!btnSlot_.enabled) return;
  const bool down = digitalRead(btnSlot_.pin) == LOW;
  const unsigned long now = millis();

  if (down && !btnSlot_.lastDown) {
    btnSlot_.lastEdgeMs = now;
    slotHoldFired_ = false;
  }
  if (down && !slotHoldFired_ && slotLongFn_ &&
      (now - btnSlot_.lastEdgeMs) >= (unsigned)BENCH_SLOT_LONG_MS) {
    slotHoldFired_ = true;
    slotLongFn_(grooveCtx_);
  }
  if (!down && btnSlot_.lastDown && !slotHoldFired_) {
    if ((now - btnSlot_.lastEdgeMs) > 40 && slotBtn_) slotBtn_(grooveCtx_);
  }
  btnSlot_.lastDown = down;
#endif
}

void BenchControls::pollLayerButtons() {
#if BENCH_LAYER_BUTTONS
  if (readBtnEdge(btnSeq_) && seqBtn_) seqBtn_(grooveCtx_);
  if (readBtnEdge(btnDrums_) && drumsBtn_) drumsBtn_(grooveCtx_);
  if (readBtnEdge(btnBass_) && bassBtn_) bassBtn_(grooveCtx_);
#endif
#if BENCH_TAP_BUTTON
  if (readBtnEdge(btnTap_) && tapBtn_) tapBtn_(grooveCtx_);
#endif
  pollSlotButton();
}

void BenchControls::pollEncoder() {
#if BENCH_ENCODER
  const int step = encoder_.readStep();
  if (step != 0 && encTurn_) encTurn_(step, perfCtx_);
#if BENCH_ENCODER_METRO
  if (encoder_.readSwHold((unsigned)BENCH_ENCODER_METRO_MS) && encLong_) {
    encLong_(perfCtx_);
  } else
#endif
  if (encoder_.readSwEdge() && encSw_) encSw_(perfCtx_);
#endif
}

void BenchControls::pollSustain() {
#if BENCH_SUSTAIN_PEDAL
  sustainDown_ = digitalRead(kPinSustain) == LOW;
#else
  sustainDown_ = false;
#endif
}

void BenchControls::pollChordButton() {
  if (!btnChord_.enabled) return;
  const bool down = digitalRead(btnChord_.pin) == LOW;
  const unsigned long now = millis();

  if (down && !btnChord_.lastDown) {
    btnChord_.lastEdgeMs = now;
    chordHoldFired_ = false;
  }
  if (down && !chordHoldFired_ && (now - btnChord_.lastEdgeMs) >= kChordLongPressMs) {
    chordHoldFired_ = true;
    if (chordLongFn_) chordLongFn_(actionCtx_);
  }
  if (!down && btnChord_.lastDown && !chordHoldFired_) {
    if ((now - btnChord_.lastEdgeMs) > 40 && chordStep_) chordStep_(ctx_);
  }
  btnChord_.lastDown = down;
}

void BenchControls::pollButtons() {
  pollChordButton();

  if (readBtnEdge(btnDemo_)) {
    demoOn_ = !demoOn_;
    if (demoToggle_) demoToggle_(demoOn_, ctx_);
  }

  if (readBtnEdge(btnPanic_)) {
    if (panicFn_) {
      panicFn_(actionCtx_);
    } else if (voicer_) {
      voicer_->silence();
    }
  }

  if (readBtnEdge(btnArp_) && arpToggle_) {
    arpToggle_(true, perfCtx_);
  }

  pollNoteButtons();
  pollOctaveButtons();
  pollLayerButtons();
}

void BenchControls::poll() {
  pollPots();
  pollSustain();
  pollEncoder();
  pollButtons();
}
