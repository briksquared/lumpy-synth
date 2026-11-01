#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal SSD1306 128x32 text display (I2C 0x3C). Enable with BENCH_OLED 1.
class BenchDisplay {
 public:
  bool begin(uint32_t sdaPin, uint32_t sclPin);
  bool ready() const { return ready_; }

  void showTransport(unsigned bpm, const char *inst, const char *chord,
                     const char *scale, bool seqOn, bool bassOn, bool drumsOn,
                     bool monoOn, int step, bool harmOn, int harmSemi, int swingPct,
                     bool unisonOn, int activeSlot, bool muteSeq, bool muteBass, bool muteDrums,
                     bool metroOn, bool chordLock);

  void showPattern(unsigned bpm, int step, int windowStart, const char *seqRow, const char *bassRow,
                   const char *drumRow);

  void poll(unsigned long nowMs);

 private:
  bool ready_ = false;
  unsigned long lastDrawMs_ = 0;
  TwoWire *wire_ = nullptr;
  uint8_t buffer_[128]{};

  void command(uint8_t c);
  void data(const uint8_t *buf, size_t len);
  void clearBuffer();
  void flush();
  void drawChar(int col, int row, char c);
  void drawText(int col, int row, const char *text);
};
