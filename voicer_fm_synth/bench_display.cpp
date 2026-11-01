#include "bench_display.h"
#include <stdio.h>

// 5x7 font — printable ASCII 32..90 subset stored as offset from 32.
static const uint8_t kFont5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

static int fontIndex(char c) {
  if (c < 32 || c > 90) return 0;
  return (int)c - 32;
}

bool BenchDisplay::begin(uint32_t sdaPin, uint32_t sclPin) {
  wire_ = &Wire;
  wire_->setSDA(sdaPin);
  wire_->setSCL(sclPin);
  wire_->begin();
  delay(10);

  command(0xAE);
  command(0xD5);
  command(0x80);
  command(0xA8);
  command(0x1F);
  command(0xD3);
  command(0x00);
  command(0x40);
  command(0x8D);
  command(0x14);
  command(0x20);
  command(0x00);
  command(0xA1);
  command(0xC8);
  command(0xDA);
  command(0x02);
  command(0x81);
  command(0xCF);
  command(0xD9);
  command(0xF1);
  command(0xDB);
  command(0x40);
  command(0xA4);
  command(0xA6);
  command(0xAF);

  ready_ = true;
  clearBuffer();
  flush();
  return true;
}

void BenchDisplay::command(uint8_t c) {
  wire_->beginTransmission(0x3C);
  wire_->write((uint8_t)0x00);
  wire_->write(c);
  wire_->endTransmission();
}

void BenchDisplay::data(const uint8_t *buf, size_t len) {
  const size_t chunk = 16;
  for (size_t i = 0; i < len; i += chunk) {
    wire_->beginTransmission(0x3C);
    wire_->write((uint8_t)0x40);
    const size_t n = (len - i < chunk) ? (len - i) : chunk;
    wire_->write(buf + i, n);
    wire_->endTransmission();
  }
}

void BenchDisplay::clearBuffer() {
  for (int i = 0; i < 128; i++) buffer_[i] = 0;
}

void BenchDisplay::flush() {
  for (uint8_t page = 0; page < 4; page++) {
    command(0xB0 + page);
    command(0x00);
    command(0x10);
    data(buffer_ + page * 32, 32);
  }
}

void BenchDisplay::drawChar(int col, int row, char c) {
  if (col < 0 || col > 20 || row < 0 || row > 3) return;
  const uint8_t *glyph = kFont5x7[fontIndex(c)];
  for (int x = 0; x < 5; x++) {
    const int px = col * 6 + x;
    if (px < 0 || px >= 128) continue;
    buffer_[row * 32 + px] = glyph[x];
  }
}

void BenchDisplay::drawText(int col, int row, const char *text) {
  if (!text) return;
  for (int i = 0; text[i] && col + i <= 20; i++) {
    drawChar(col + i, row, text[i]);
  }
}

void BenchDisplay::showTransport(unsigned bpm, const char *inst, const char *chord,
                                 const char *scale, bool seqOn, bool bassOn, bool drumsOn,
                                 bool monoOn, int step, bool harmOn, int harmSemi, int swingPct,
                                 bool unisonOn, int activeSlot, bool muteSeq, bool muteBass,
                                 bool muteDrums, bool metroOn, bool chordLock) {
  if (!ready_) return;
  clearBuffer();

  char line[22];
  snprintf(line, sizeof(line), "BPM%u S%d%s%s", bpm, activeSlot % 10, metroOn ? " M" : "",
           chordLock ? " L" : "");
  drawText(0, 0, line);
  snprintf(line, sizeof(line), "%s %s %s", inst ? inst : "?", chord ? chord : "?",
           scale ? scale : "?");
  drawText(0, 1, line);
  snprintf(line, sizeof(line), "%s%s%s%s st%02d", seqOn ? "SQ" : "--", bassOn ? " BS" : "",
           drumsOn ? " DR" : "", monoOn ? " MN" : "", step % 100);
  drawText(0, 2, line);
  if (muteSeq || muteBass || muteDrums) {
    snprintf(line, sizeof(line), "%s%s%s%s", muteSeq ? "mSQ" : "", muteBass ? "mBS" : "",
             muteDrums ? "mDR" : "", harmOn ? " H+" : "");
  } else {
    snprintf(line, sizeof(line), "%s%02d%s%s", harmOn ? "H+" : "H-", harmOn ? harmSemi : 0,
             swingPct > 0 ? " SW" : "", unisonOn ? " UN" : "");
  }
  drawText(0, 3, line);

  flush();
}

void BenchDisplay::showPattern(unsigned bpm, int step, int windowStart, const char *seqRow,
                             const char *bassRow, const char *drumRow) {
  if (!ready_) return;
  clearBuffer();

  char line[22];
  if (windowStart > 0) {
    snprintf(line, sizeof(line), "BPM%u s%02d+%02d", bpm, step % 100, windowStart % 100);
  } else {
    snprintf(line, sizeof(line), "BPM%u st%02d", bpm, step % 100);
  }
  drawText(0, 0, line);
  snprintf(line, sizeof(line), "S:%s", seqRow ? seqRow : "");
  drawText(0, 1, line);
  snprintf(line, sizeof(line), "B:%s", bassRow ? bassRow : "");
  drawText(0, 2, line);
  snprintf(line, sizeof(line), "D:%s", drumRow ? drumRow : "");
  drawText(0, 3, line);

  flush();
}

void BenchDisplay::poll(unsigned long nowMs) {
  if (!ready_) return;
  if (nowMs - lastDrawMs_ < 250) return;
  lastDrawMs_ = nowMs;
}
