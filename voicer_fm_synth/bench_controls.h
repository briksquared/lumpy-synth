#pragma once

#include "bench_config.h"
#include "StkAll.h"
#include "rotary_encoder.h"

class BenchControls {
 public:
  using ChordStepFn = void (*)(void *ctx);
  using DemoToggleFn = void (*)(bool on, void *ctx);
  using ArpToggleFn = void (*)(bool on, void *ctx);
  using EncTurnFn = void (*)(int steps, void *ctx);
  using EncSwFn = void (*)(void *ctx);
  using NoteBtnFn = void (*)(int index, bool down, void *ctx);
  using OctaveFn = void (*)(int delta, void *ctx);
  using PanicFn = void (*)(void *ctx);
  using ChordLongFn = void (*)(void *ctx);
  using GrooveBtnFn = void (*)(void *ctx);
  using SlotLongFn = void (*)(void *ctx);

  using EncLongFn = void (*)(void *ctx);

  void begin(stk::Voicer *voicer, int group, float stkAmp,
             ChordStepFn chordStep, DemoToggleFn demoToggle, void *ctx);

  void beginActions(PanicFn panic, ChordLongFn chordLong, void *actionCtx);

  void beginPerformance(EncTurnFn encTurn, EncSwFn encSw, EncLongFn encLong,
                        ArpToggleFn arpToggle, void *perfCtx);

  void beginKeyboard(NoteBtnFn noteBtn, OctaveFn octaveFn, void *kbCtx);

  void beginGroove(GrooveBtnFn seq, GrooveBtnFn drums, GrooveBtnFn bass,
                   GrooveBtnFn tap, GrooveBtnFn slot, SlotLongFn slotLong,
                   void *grooveCtx);

  void poll();

  float masterGain() const { return masterGain_; }
  float colorNorm() const { return colorSmooth_; }
  float widthNorm() const { return widthSmooth_; }
  float swingNorm() const { return swingSmooth_; }
  float modNorm() const { return modSmooth_; }
  unsigned demoIntervalMs(unsigned presetMs) const;
  unsigned arpIntervalMs() const;
  unsigned bpmFromTempo() const;
  bool sustainDown() const { return sustainDown_; }

 private:
  struct BtnMap {
    uint32_t pin;
    bool enabled;
    bool lastDown;
    unsigned long lastEdgeMs;
  };

  void pollPots();
  void pollButtons();
  void pollChordButton();
  void pollNoteButtons();
  void pollOctaveButtons();
  void pollLayerButtons();
  void pollSlotButton();
  void pollEncoder();
  void pollSustain();
  bool readBtnEdge(BtnMap &b, unsigned debounceMs = 40);
  float readPot(uint32_t pin, float &smooth);
  void sendCc(int cc, float normalized);

  stk::Voicer *voicer_ = nullptr;
  int group_ = 0;
  float stkAmp_ = 80.0f;
  ChordStepFn chordStep_ = nullptr;
  DemoToggleFn demoToggle_ = nullptr;
  void *ctx_ = nullptr;
  EncTurnFn encTurn_ = nullptr;
  EncSwFn encSw_ = nullptr;
  EncLongFn encLong_ = nullptr;
  ArpToggleFn arpToggle_ = nullptr;
  void *perfCtx_ = nullptr;
  NoteBtnFn noteBtn_ = nullptr;
  OctaveFn octaveFn_ = nullptr;
  void *kbCtx_ = nullptr;
  PanicFn panicFn_ = nullptr;
  ChordLongFn chordLongFn_ = nullptr;
  void *actionCtx_ = nullptr;
  GrooveBtnFn seqBtn_ = nullptr;
  GrooveBtnFn drumsBtn_ = nullptr;
  GrooveBtnFn bassBtn_ = nullptr;
  GrooveBtnFn tapBtn_ = nullptr;
  GrooveBtnFn slotBtn_ = nullptr;
  SlotLongFn slotLongFn_ = nullptr;
  void *grooveCtx_ = nullptr;
  bool chordHoldFired_ = false;
  bool slotHoldFired_ = false;
  RotaryEncoder encoder_;
  BtnMap btnArp_{kPinBtnArp, BENCH_ARP_BUTTON != 0, false, 0};
  BtnMap btnSeq_{kPinBtnSeq, BENCH_LAYER_BUTTONS != 0, false, 0};
  BtnMap btnDrums_{kPinBtnDrums, BENCH_LAYER_BUTTONS != 0, false, 0};
  BtnMap btnBass_{kPinBtnBass, BENCH_LAYER_BUTTONS != 0, false, 0};
  BtnMap btnTap_{kPinBtnTap, BENCH_TAP_BUTTON != 0, false, 0};
  BtnMap btnSlot_{kPinBtnSlot, BENCH_SLOT_BUTTON != 0, false, 0};
  BtnMap btnOctDown_{kPinBtnOctDown, BENCH_OCTAVE_BUTTONS != 0, false, 0};
  BtnMap btnOctUp_{kPinBtnOctUp, BENCH_OCTAVE_BUTTONS != 0, false, 0};
  bool sustainDown_ = false;

  float masterGain_ = kMasterGainDefault;
  float tempoNorm_ = 0.5f;
  float tempoSmooth_ = 0.5f;
  float modSmooth_ = 0.0f;
  float breathSmooth_ = 0.0f;
  float colorSmooth_ = 0.0f;
  float widthSmooth_ = 0.5f;
  float swingSmooth_ = 0.35f;
  float volSmooth_ = 1.0f;
  float modSent_ = -1.0f;
  float breathSent_ = -1.0f;
  float colorSent_ = -1.0f;

  BtnMap btnChord_{kPinBtnChord, BENCH_BUTTON_CHORD != 0, false, 0};
  BtnMap btnDemo_{kPinBtnDemo, BENCH_BUTTON_DEMO != 0, false, 0};
  BtnMap btnPanic_{kPinBtnPanic, BENCH_BUTTON_PANIC != 0, false, 0};

  BtnMap btnNotes_[BENCH_NOTE_BUTTON_COUNT]{};
  bool demoOn_ = BENCH_AUTO_DEMO != 0;
};
