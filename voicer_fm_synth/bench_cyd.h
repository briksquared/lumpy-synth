#pragma once

#include <Arduino.h>

// UART telemetry + touch command RX for ESP32-2432S028R companion display.
// Enable with BENCH_CYD_UART 1 — USART2 PA2 TX → CYD GPIO27 RX, PA3 RX ← CYD GPIO22 TX.

typedef void (*BenchCydCommandFn)(const String &line);

void benchCydBegin();
void benchCydSetCommandHandler(BenchCydCommandFn fn);
void benchCydPollRx();

void benchCydEmitTransport(unsigned bpm, const char *inst, const char *chord, const char *scale,
                           bool seqOn, bool bassOn, bool drumsOn, bool monoOn, int step,
                           int swingPct, int activeSlot, bool muteSeq, bool muteBass, bool muteDrums,
                           bool metroOn, bool chordLock);

void benchCydEmitPattern(unsigned bpm, int step, int windowStart, const char *seqRow,
                         const char *bassRow, const char *drumRow);
