#pragma once

#include "StkAll.h"

// ---------------------------------------------------------------------------
// Swap instrument: change STK_VOICE_PRESET (0–11), reflash with flash.ps1
// ---------------------------------------------------------------------------
#define PRESET_BEETHREE 0   // FM electric piano (default)
#define PRESET_CLARINET 1   // breathy reed
#define PRESET_PLUCKED 2    // Karplus-Strong harp/guitar
#define PRESET_STIFKARP 3   // stiff plucked string (banjo-ish)
#define PRESET_RHODEY 4     // Rhodes EP
#define PRESET_WURLEY 5     // Wurlitzer EP
#define PRESET_BRASS 6      // brass section
#define PRESET_FLUTE 7      // flute
#define PRESET_MOOG 8       // Moog-style synth
#define PRESET_MANDOLIN 9   // plucked mandolin
#define PRESET_SAXOFONY 10  // sax
#define PRESET_TUBEBELL 11  // tubular bells

#ifndef STK_VOICE_PRESET
#define STK_VOICE_PRESET PRESET_CLARINET
#endif

#if STK_VOICE_PRESET == PRESET_BEETHREE
using StkVoice = BeeThree;
static constexpr float kStkGain = 280.0f;
static constexpr float kStkAmp = 80.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "BeeThree";

#elif STK_VOICE_PRESET == PRESET_CLARINET
using StkVoice = Clarinet;
static constexpr float kStkGain = 320.0f;
static constexpr float kStkAmp = 90.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Clarinet";

#elif STK_VOICE_PRESET == PRESET_PLUCKED
using StkVoice = Plucked;
static constexpr float kStkGain = 400.0f;
static constexpr float kStkAmp = 100.0f;
static constexpr unsigned kChordMs = 1800;
static constexpr const char *kInstrumentName = "Plucked";

#elif STK_VOICE_PRESET == PRESET_STIFKARP
using StkVoice = StifKarp;
static constexpr float kStkGain = 380.0f;
static constexpr float kStkAmp = 100.0f;
static constexpr unsigned kChordMs = 1800;
static constexpr const char *kInstrumentName = "StifKarp";

#elif STK_VOICE_PRESET == PRESET_RHODEY
using StkVoice = Rhodey;
static constexpr float kStkGain = 260.0f;
static constexpr float kStkAmp = 75.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Rhodey";

#elif STK_VOICE_PRESET == PRESET_WURLEY
using StkVoice = Wurley;
static constexpr float kStkGain = 260.0f;
static constexpr float kStkAmp = 75.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Wurley";

#elif STK_VOICE_PRESET == PRESET_BRASS
using StkVoice = Brass;
static constexpr float kStkGain = 300.0f;
static constexpr float kStkAmp = 85.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Brass";

#elif STK_VOICE_PRESET == PRESET_FLUTE
using StkVoice = Flute;
static constexpr float kStkLowFreq = 8.0f;
static constexpr float kStkGain = 350.0f;
static constexpr float kStkAmp = 90.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Flute";

#elif STK_VOICE_PRESET == PRESET_MOOG
using StkVoice = Moog;
static constexpr float kStkGain = 240.0f;
static constexpr float kStkAmp = 70.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Moog";

#elif STK_VOICE_PRESET == PRESET_MANDOLIN
using StkVoice = Mandolin;
static constexpr float kStkLowFreq = 8.0f;
static constexpr float kStkGain = 400.0f;
static constexpr float kStkAmp = 100.0f;
static constexpr unsigned kChordMs = 1800;
static constexpr const char *kInstrumentName = "Mandolin";

#elif STK_VOICE_PRESET == PRESET_SAXOFONY
using StkVoice = Saxofony;
static constexpr float kStkLowFreq = 8.0f;
static constexpr float kStkGain = 320.0f;
static constexpr float kStkAmp = 90.0f;
static constexpr unsigned kChordMs = 2500;
static constexpr const char *kInstrumentName = "Saxofony";

#elif STK_VOICE_PRESET == PRESET_TUBEBELL
using StkVoice = TubeBell;
static constexpr float kStkGain = 350.0f;
static constexpr float kStkAmp = 95.0f;
static constexpr unsigned kChordMs = 3000;
static constexpr const char *kInstrumentName = "TubeBell";

#else
#error "Unknown STK_VOICE_PRESET — use PRESET_* value 0–11 in instrument_config.h"
#endif

#if STK_VOICE_PRESET == PRESET_FLUTE || STK_VOICE_PRESET == PRESET_MANDOLIN || \
    STK_VOICE_PRESET == PRESET_SAXOFONY
#define STK_VOICE_DECL                                                       \
  static StkVoice inst0(kStkLowFreq), inst1(kStkLowFreq), inst2(kStkLowFreq), \
      inst3(kStkLowFreq), inst4(kStkLowFreq), inst5(kStkLowFreq)
#else
#define STK_VOICE_DECL static StkVoice inst0, inst1, inst2, inst3, inst4, inst5
#endif
