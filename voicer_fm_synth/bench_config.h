#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Bench I/O — edit here, reflash with flash.ps1
// Set any BENCH_* block to 0 if that control is not wired (avoids float noise).
// ---------------------------------------------------------------------------

#ifndef BENCH_AUTO_DEMO
#define BENCH_AUTO_DEMO 1
#endif

#ifndef BENCH_SERIAL_MIDI
#define BENCH_SERIAL_MIDI 1
#endif

#ifndef BENCH_UART_MIDI
#define BENCH_UART_MIDI 0
#endif

// --- Pots (10k linear: GND — wiper — 3V3) ----------------------------------

#ifndef BENCH_VOLUME_POT
#define BENCH_VOLUME_POT 1
#endif

#ifndef BENCH_MOD_POT
#define BENCH_MOD_POT 1
#endif

#ifndef BENCH_BREATH_POT
#define BENCH_BREATH_POT 0
#endif

#ifndef BENCH_COLOR_POT
#define BENCH_COLOR_POT 1
#endif

#ifndef BENCH_TEMPO_POT
#define BENCH_TEMPO_POT 1
#endif

#ifndef BENCH_SWING_POT
#define BENCH_SWING_POT 1
#endif

#ifndef BENCH_WIDTH_POT
#define BENCH_WIDTH_POT 0
#endif

#ifndef BENCH_MIDI_CLOCK
#define BENCH_MIDI_CLOCK 1
#endif

#ifndef BENCH_AUTO_SAVE
#define BENCH_AUTO_SAVE 1
#endif

#ifndef BENCH_STRUM_DEFAULT
#define BENCH_STRUM_DEFAULT 1
#endif

#ifndef BENCH_MIDI_QUANTIZE
#define BENCH_MIDI_QUANTIZE 0
#endif

#ifndef BENCH_SEQUENCER
#define BENCH_SEQUENCER 1
#endif

#ifndef BENCH_DRUM_GRID
#define BENCH_DRUM_GRID 1
#endif

#ifndef BENCH_BASS_SEQ
#define BENCH_BASS_SEQ 1
#endif

#ifndef BENCH_OLED
#define BENCH_OLED 0
#endif

#ifndef BENCH_CYD_UART
#define BENCH_CYD_UART 0
#endif

#ifndef BENCH_CC_MACROS
#define BENCH_CC_MACROS 1
#endif

#ifndef BENCH_HARMONY
#define BENCH_HARMONY 1
#endif

#ifndef BENCH_DRUM_PADS
#define BENCH_DRUM_PADS 1
#endif

#define LUMPY_SYNTH_NAME "LUMPY SYNTH"
#define F411_SYNTH_VERSION "3.20.0"

// EEPROM schema v2 — global header @ addr 650 (see bench_eeprom.h).
#define LUMPY_EEPROM_SCHEMA 2
#define LUMPY_API_MAJOR 3
#define LUMPY_API_PATCH 20

// --- DAC module -------------------------------------------------------------
// BENCH_DAC_PCM5102 0 = CJMCU-1334 (UDA1334A, default breadboard)
// BENCH_DAC_PCM5102 1 = PCM5102A breakout (same I2S2 pins PB12/13/15)
// BENCH_PCM_XMT_PIN 0 = tie XMT to 3V3 on module (most breakouts)
// BENCH_PCM_XMT_PIN N = drive GPIO HIGH to unmute (PCM5102 XMT active-low mute)

#ifndef BENCH_DAC_PCM5102
#define BENCH_DAC_PCM5102 0
#endif

#ifndef BENCH_PCM_XMT_PIN
#define BENCH_PCM_XMT_PIN 0
#endif

// --- Buttons ----------------------------------------------------------------

#ifndef BENCH_BUTTON_CHORD
#define BENCH_BUTTON_CHORD 1
#endif

#ifndef BENCH_BUTTON_DEMO
#define BENCH_BUTTON_DEMO 1
#endif

#ifndef BENCH_BUTTON_PANIC
#define BENCH_BUTTON_PANIC 1
#endif

#ifndef BENCH_NOTE_BUTTONS
#define BENCH_NOTE_BUTTONS 1
#endif

#ifndef BENCH_NOTE_BUTTON_COUNT
#define BENCH_NOTE_BUTTON_COUNT 3
#endif

#ifndef BENCH_OCTAVE_BUTTONS
#define BENCH_OCTAVE_BUTTONS 1
#endif

#ifndef BENCH_ENCODER
#define BENCH_ENCODER 1
#endif

#ifndef BENCH_ARP_BUTTON
#define BENCH_ARP_BUTTON 1
#endif

#ifndef BENCH_LAYER_BUTTONS
#define BENCH_LAYER_BUTTONS 1
#endif

#ifndef BENCH_TAP_BUTTON
#define BENCH_TAP_BUTTON 1
#endif

#ifndef BENCH_SLOT_BUTTON
#define BENCH_SLOT_BUTTON 1
#endif

#ifndef BENCH_MONO_FILTER
#define BENCH_MONO_FILTER 1
#endif

#ifndef BENCH_MONO_LADDER
#define BENCH_MONO_LADDER 1
#endif

#ifndef BENCH_MONO_LADDER_RES
#define BENCH_MONO_LADDER_RES 0.35f
#endif

#ifndef BENCH_FILTER_ENV
#define BENCH_FILTER_ENV 1
#endif

#ifndef BENCH_SLOT_LONG_MS
#define BENCH_SLOT_LONG_MS 800
#endif

#ifndef BENCH_SUSTAIN_PEDAL
#define BENCH_SUSTAIN_PEDAL 0
#endif

// SSD1306 128x32 — PB8 SCL, PB9 SDA (enable BENCH_OLED 1 when wired)
static constexpr uint32_t kPinOledSda = PB9;
static constexpr uint32_t kPinOledScl = PB8;

// ESP32-2432S028R (CYD) UART link — USART2 when BENCH_CYD_UART 1
// Black Pill PA2 TX → CYD GPIO27 RX · PA3 RX ← CYD GPIO22 TX · GND common
// Conflicts with SLOT (PA2) / OCT− (PA3) buttons — disable those when wired.
static constexpr uint32_t kPinCydTx = PA_2;
static constexpr uint32_t kPinCydRx = PA_3;
static constexpr uint32_t kCydUartBaud = 115200;

// Pin map
static constexpr uint32_t kPinVol = PA1;
static constexpr uint32_t kPinMod = PA4;
static constexpr uint32_t kPinBreath = PA5;
static constexpr uint32_t kPinColor = PB0;
static constexpr uint32_t kPinTempo = PB1;
static constexpr uint32_t kPinWidth = PA11;  // PA10 reserved for UART1 RX (DIN MIDI)
static constexpr uint32_t kPinSwing = PA5;

static constexpr uint32_t kPinBtnChord = PA0;
static constexpr uint32_t kPinBtnDemo = PB2;
static constexpr uint32_t kPinBtnPanic = PB6;
static constexpr uint32_t kPinBtnSeq = PA6;
static constexpr uint32_t kPinBtnDrums = PA7;
static constexpr uint32_t kPinBtnBass = PA8;
static constexpr uint32_t kPinBtnTap = PA9;
static constexpr uint32_t kPinBtnSlot = PA2;
static constexpr uint32_t kPinBtnOctDown = PA3;
static constexpr uint32_t kPinBtnOctUp = PB14;

// Scale keyboard — degrees from root (triad default: PB7 PB8 PB9)
#if BENCH_NOTE_BUTTON_COUNT >= 7
static constexpr uint32_t kNotePins[7] = {PB7, PB8, PB9, PA6, PA7, PA8, PA9};
#elif BENCH_NOTE_BUTTON_COUNT >= 3
static constexpr uint32_t kNotePins[3] = {PB7, PB8, PB9};
#else
static constexpr uint32_t kNotePins[1] = {PB7};
#endif

#ifndef BENCH_ENCODER_METRO
#define BENCH_ENCODER_METRO 1
#endif

#ifndef BENCH_ENCODER_METRO_MS
#define BENCH_ENCODER_METRO_MS 700
#endif

static constexpr uint32_t kPinUartMidiRx = PA10;
static constexpr uint32_t kPinEncDt = PB3;
static constexpr uint32_t kPinEncSw = PB4;
static constexpr uint32_t kPinBtnArp = PB5;
static constexpr uint32_t kPinSustain = PA2;

static constexpr uint32_t kMidiChannel = 0;

static constexpr float kMasterGainDefault = 1.0f;
static constexpr float kMasterGainMin = 0.08f;
static constexpr float kMasterGainMax = 1.0f;

static constexpr unsigned kDemoMsMin = 600;
static constexpr unsigned kDemoMsMax = 6000;

static constexpr int kOctaveShiftMin = -2;
static constexpr int kOctaveShiftMax = 2;

static constexpr unsigned kChordLongPressMs = 600;
static constexpr int kMidiClockPerArpStep = 6;

// MIDI CC macro pads (value >= 64 triggers on rising edge)
static constexpr uint8_t kCcMacroBase = 36;   // CC36..42 = jam..full
static constexpr uint8_t kCcMacroPanic = 43;
static constexpr uint8_t kCcToggleSeq = 44;
static constexpr uint8_t kCcToggleDrums = 45;
static constexpr uint8_t kCcToggleBass = 46;
static constexpr uint8_t kCcToggleHarm = 47;
static constexpr uint8_t kCcMacroDub = 48;
static constexpr uint8_t kCcToggleUnison = 49;
static constexpr uint8_t kCcPresetBase = 32;   // CC32..35 = load EEPROM slot 0..3
static constexpr uint8_t kCcStash = 50;
static constexpr uint8_t kCcUndo = 51;
static constexpr uint8_t kCcAuditionBase = 52;  // CC52..55 = audition slot 0..3
static constexpr uint8_t kCcCommit = 56;        // save active slot to EEPROM
static constexpr uint8_t kCcMuteSeq = 57;
static constexpr uint8_t kCcMuteBass = 58;
static constexpr uint8_t kCcMuteDrums = 59;
static constexpr uint8_t kCcSoloSeq = 60;
static constexpr uint8_t kCcSoloBass = 61;
static constexpr uint8_t kCcSoloDrums = 62;
static constexpr uint8_t kCcUnmuteAll = 63;
static constexpr uint8_t kCcToggleBounce = 70;
static constexpr uint8_t kCcToggleMetro = 71;
static constexpr uint8_t kCcToggleChordLock = 72;
static constexpr uint8_t kCcToggleMetroAccent = 73;
static constexpr uint8_t kCcMacroStage = 75;
static constexpr uint8_t kCcToggleHarmLock = 76;
static constexpr uint8_t kCcToggleAutosave = 77;
static constexpr uint8_t kCcTransportReset = 78;
static constexpr uint8_t kCcPatternMirror = 79;
static constexpr uint8_t kCcPatternRotate = 80;
static constexpr uint8_t kCcPatternShuffle = 81;
static constexpr uint8_t kCcPatternStrip = 82;
static constexpr uint8_t kCcMacroGlitch = 83;
static constexpr uint8_t kCcPatternThicken = 84;
static constexpr uint8_t kCcMacroRave = 85;
static constexpr uint8_t kCcPatternAlign = 86;
static constexpr uint8_t kCcMacroHush = 87;
static constexpr uint8_t kCcMacroSpark = 88;
static constexpr uint8_t kCcPatternContrast = 89;
static constexpr uint8_t kCcMacroFrost = 90;
static constexpr uint8_t kCcPatternEcho = 91;
static constexpr uint8_t kCcMacroPulse = 92;
static constexpr uint8_t kCcPatternCompact = 93;
static constexpr uint8_t kCcMacroSurge = 94;
static constexpr uint8_t kCcPatternWeave = 95;
static constexpr uint8_t kCcPatternCarve = 96;
static constexpr uint8_t kCcMacroHaze = 97;
static constexpr uint8_t kCcPatternSnap = 98;
static constexpr uint8_t kCcMacroGlow = 99;
static constexpr uint8_t kCcPatternBraid = 100;
static constexpr uint8_t kCcMacroEmber = 101;
static constexpr uint8_t kCcPatternDrift = 102;
static constexpr uint8_t kCcMacroSlate = 103;
static constexpr uint8_t kCcPatternAnchor = 104;
static constexpr uint8_t kCcMacroPrism = 105;
static constexpr uint8_t kCcFilterCutoff = 74;  // GM brightness — mono ladder cutoff
static constexpr uint8_t kCcFilterEnvDepth = 107;
static constexpr uint8_t kCcPatternLatch = 108;
static constexpr uint8_t kCcPatternRivet = 109;
static constexpr uint8_t kCcPatternBolt = 110;
static constexpr uint8_t kCcPatternWeld = 111;
static constexpr uint8_t kCcPatternForge = 112;
static constexpr uint8_t kCcPatternMint = 113;
static constexpr uint8_t kCcPatternAnvil = 114;
static constexpr uint8_t kCcPatternTemper = 115;
static constexpr uint8_t kCcPatternQuench = 116;
static constexpr uint8_t kCcPatternSmelt = 117;
static constexpr uint8_t kCcPatternCast = 118;
static constexpr uint8_t kCcPatternMold = 119;
static constexpr uint8_t kCcPatternStamp = 120;
static constexpr uint8_t kCcPatternPlate = 121;
static constexpr uint8_t kCcPatternSheet = 122;
static constexpr uint8_t kCcPatternCoil = 123;
static constexpr uint8_t kCcPatternWire = 124;
static constexpr uint8_t kCcPatternStrand = 125;
static constexpr uint8_t kCcPatternCable = 126;
static constexpr uint8_t kCcPatternHarness = 127;

static constexpr uint32_t kPinEncClk = PB10;

// GM-style drum pads when BENCH_DRUM_PADS (note on = hit; write mode adds step edit)
static constexpr uint8_t kMidiPadKick = 36;
static constexpr uint8_t kMidiPadKickOff = 37;
static constexpr uint8_t kMidiPadHat = 42;
static constexpr uint8_t kMidiPadHatOff = 43;
// Seq grid: notes 48..79 toggle steps 0..31 (when seq grid on)
static constexpr uint8_t kMidiSeqGridBase = 48;
// Bass grid: notes 80..95 toggle steps 0..15 (degree = note % 7)
static constexpr uint8_t kMidiBassGridBase = 80;
