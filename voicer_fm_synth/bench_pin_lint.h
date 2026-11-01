#pragma once

#include "bench_config.h"
#include <Arduino.h>

// Returns warning count (0 = clean for current bench_config.h).
inline int benchPinLintCount() {
  int warns = 0;
#if BENCH_WIDTH_POT
  warns++;
#endif
#if BENCH_SWING_POT && BENCH_BREATH_POT
  warns++;
#endif
#if BENCH_SLOT_BUTTON && BENCH_SUSTAIN_PEDAL
  warns++;
#endif
#if BENCH_LAYER_BUTTONS && BENCH_NOTE_BUTTONS && BENCH_NOTE_BUTTON_COUNT >= 7
  warns++;
#endif
#if BENCH_OLED && BENCH_NOTE_BUTTONS && BENCH_NOTE_BUTTON_COUNT >= 2
  warns++;
#endif
#if BENCH_DAC_PCM5102 && BENCH_PCM_XMT_PIN
#if BENCH_PCM_XMT_PIN == (uint32_t)PB8 || BENCH_PCM_XMT_PIN == (uint32_t)PB9
  warns++;
#endif
#if BENCH_PCM_XMT_PIN == (uint32_t)PB15
  warns++;
#endif
#endif
  return warns;
}

inline void benchPinLint(Stream &out) {
  out.println(F("--- bench lint ---"));
#if BENCH_WIDTH_POT
  out.println(F("WARN PA11 WIDTH pot — USB DM conflict"));
#endif
#if BENCH_SWING_POT && BENCH_BREATH_POT
  out.println(F("WARN PA5 shared — SWING + BREATH pots"));
#endif
#if BENCH_SLOT_BUTTON && BENCH_SUSTAIN_PEDAL
  out.println(F("WARN PA2 shared — SLOT + SUSTAIN"));
#endif
#if BENCH_LAYER_BUTTONS && BENCH_NOTE_BUTTONS && BENCH_NOTE_BUTTON_COUNT >= 7
  out.println(F("WARN PA6-PA9 — layer buttons vs 7-key row"));
#endif
#if BENCH_OLED && BENCH_NOTE_BUTTONS && BENCH_NOTE_BUTTON_COUNT >= 2
  out.println(F("WARN PB8/PB9 — OLED vs note keys"));
#endif
#if BENCH_CYD_UART
  out.println(F("INFO PA2/PA3 — CYD UART (USART2); conflicts SLOT/OCT- buttons"));
#endif
#if BENCH_DAC_PCM5102 && BENCH_PCM_XMT_PIN
#if BENCH_PCM_XMT_PIN == (uint32_t)PB8 || BENCH_PCM_XMT_PIN == (uint32_t)PB9
  out.println(F("WARN PCM XMT pin — OLED / note key conflict"));
#endif
#if BENCH_PCM_XMT_PIN == (uint32_t)PB15
  out.println(F("WARN PCM XMT on PB15 — I2S DIN sacred"));
#endif
#endif
  out.print(F("i2s sacred PB12 WS PB13 BCLK PB15 DIN"));
  out.println();
  const int warns = benchPinLintCount();
  out.print(F("bench lint "));
  out.print(warns);
  out.println(warns == 0 ? F(" PASS") : F(" WARN"));
  out.println(F("---"));
}
