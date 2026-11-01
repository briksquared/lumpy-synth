/**
 * LUMPY SYNTH — ESP32-2432S028R companion display
 * UI skin: Transmission Works · DSP DREAMS perform-home layout
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "ui_dsp_dreams.h"

#ifndef CYD_PILL_TX
#define CYD_PILL_TX 22
#endif
#ifndef CYD_PILL_RX
#define CYD_PILL_RX 27
#endif
#ifndef CYD_UART_BAUD
#define CYD_UART_BAUD 115200
#endif

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);
HardwareSerial &pill = Serial1;

struct CydState {
  bool linked = false;
  bool patternView = false;
  unsigned bpm = 120;
  int slot = 0;
  int step = 0;
  int win = 0;
  bool sq = false;
  bool bs = false;
  bool dr = false;
  bool metro = false;
  bool chordLock = false;
  char inst[16] = "LUMPY";
  char chord[12] = "maj";
  char scale[12] = "pent";
  char seqRow[20] = "";
  char bassRow[20] = "";
  char drumRow[20] = "";
};

static CydState state;
static String rxLine;
static uint32_t uiFrame = 0;

static void sendPill(const char *cmd) {
  pill.print(cmd);
  pill.print('\n');
}

static void drawHeader() {
  using namespace DspUi;
  tft.fillRect(0, 0, kW, kHeaderH, kPanel);
  tft.drawFastHLine(0, 0, kW, kAccent);
  tft.drawFastHLine(0, 1, kW, kAccentDim);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kText, kPanel);
  tft.drawString(state.inst, 8, 8, 2);

  char sub[40];
  snprintf(sub, sizeof(sub), "%s · %s · S%d", state.chord, state.scale, state.slot);
  tft.setTextColor(kTextDim, kPanel);
  tft.drawString(sub, 8, 24, 1);

  char bpm[16];
  snprintf(bpm, sizeof(bpm), "%u BPM", state.bpm);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(kAccent, kPanel);
  tft.drawString(bpm, kW - 8, 10, 2);

  const uint16_t linkCol = state.linked ? kLive : kWarn;
  tft.fillCircle(kW - 14, 26, 4, linkCol);
  tft.setTextColor(kTextDim, kPanel);
  tft.drawString(state.linked ? "LINK" : "WAIT", kW - 22, 22, 1);
}

static void drawRibbonTitle(const char *title) {
  using namespace DspUi;
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(kTextDim, kBg);
  char line[32];
  snprintf(line, sizeof(line), "~~~ %s ~~~", title);
  tft.drawString(line, kW / 2, 40, 1);
}

static void drawParamChip(int x, int y, int w, int h, const char *label, const char *value, bool hot) {
  using namespace DspUi;
  const uint16_t fill = hot ? DspUi::breathe(kAccentDim, kAccent, uiFrame) : kPanel;
  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.drawRoundRect(x, y, w, h, 5, hot ? kAccent : kBorder);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(kTextDim, fill);
  tft.drawString(label, x + w / 2, y + 4, 1);
  tft.setTextColor(kText, fill);
  tft.drawString(value, x + w / 2, y + 16, 2);
}

static void drawTransportBody() {
  using namespace DspUi;
  char buf[16];
  drawParamChip(8, 58, 70, 38, "SLOT", (snprintf(buf, sizeof(buf), "%d", state.slot), buf), false);
  snprintf(buf, sizeof(buf), "%02d", state.step % 100);
  drawParamChip(86, 58, 70, 38, "STEP", buf, true);
  drawParamChip(164, 58, 70, 38, "CHORD", state.chord, false);
  drawParamChip(242, 58, 70, 38, "SCALE", state.scale, false);

  drawParamChip(8, 104, 148, 38, "SEQ", state.sq ? "ON" : "OFF", state.sq);
  drawParamChip(164, 104, 148, 38, "LAYERS", state.bs && state.dr ? "BS+DR" : state.bs ? "BS" : state.dr ? "DR" : "---",
                state.bs || state.dr);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kTextDim, kBg);
  tft.drawString(state.metro ? "METRO locked" : "METRO off", 8, 152, 1);
  if (state.chordLock) tft.drawString("CHORD LOCK", 120, 152, 1);
}

static void drawStepCell(int x, int y, int w, int h, char c, bool play) {
  using namespace DspUi;
  const bool hit = (c != '.' && c != '-' && c != '>');
  uint16_t fill = kPanel;
  if (play) fill = DspUi::breathe(kAccentDim, kAccent, uiFrame + (uint32_t)x);
  else if (hit) fill = kBorder;
  tft.fillRoundRect(x, y, w, h, 2, fill);
  if (play) tft.drawRoundRect(x, y, w, h, 2, kAccent);
  if (c != '.' && c != '-') {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(play ? kBg : kTextDim, fill);
    char s[2] = {c, 0};
    tft.drawString(s, x + w / 2, y + h / 2, 1);
  }
}

static void drawPatternRow(int y, const char *tag, const char *row, int cursorInWin) {
  using namespace DspUi;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kAccent, kBg);
  tft.drawString(tag, 4, y + 4, 2);
  const int cellW = 17;
  const int x0 = 22;
  for (int i = 0; i < 16; i++) {
    char c = (row[i] != 0) ? row[i] : '.';
    const bool play = (c == '*' || c == '>' || c == '@' || c == '^');
    drawStepCell(x0 + i * cellW, y, cellW - 1, 22, c, play);
    (void)cursorInWin;
  }
}

static void drawPatternBody() {
  drawPatternRow(58, "S", state.seqRow, state.step);
  drawPatternRow(88, "B", state.bassRow, state.step);
  drawPatternRow(118, "D", state.drumRow, state.step);
  char win[24];
  snprintf(win, sizeof(win), "window +%02d", state.win % 100);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(DspUi::kTextDim, DspUi::kBg);
  tft.drawString(win, DspUi::kW - 6, 148, 1);
}

static void drawFooter() {
  using namespace DspUi;
  tft.fillRect(0, kFooterY, kW, 14, kPanel);
  tft.drawFastHLine(0, kFooterY, kW, kBorder);

  const int barX = 8;
  const int barW = 200;
  const int fillW = (state.step % 16 + 1) * barW / 16;
  tft.fillRoundRect(barX, kFooterY + 3, barW, 8, 2, kBorder);
  tft.fillRoundRect(barX, kFooterY + 3, fillW, 8, 2, kAccentDim);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kTextDim, kPanel);
  tft.drawString("EXP", barX, kFooterY + 1, 1);

  const bool live = state.sq || state.bs || state.dr;
  tft.fillCircle(kW - 52, kFooterY + 7, 4, live ? kLive : kBorder);
  tft.setTextColor(live ? kLive : kTextDim, kPanel);
  tft.drawString(live ? "LIVE" : "IDLE", kW - 44, kFooterY + 2, 1);

  if (state.metro) {
    tft.setTextColor(kMorph, kPanel);
    tft.drawString("SYNC", kW - 90, kFooterY + 2, 1);
  }
}

static void drawRibbonButton(int x, int y, int w, int h, const char *label, bool on, bool focus) {
  using namespace DspUi;
  uint16_t fill = on ? DspUi::breathe(kAccentDim, kAccent, uiFrame + (uint32_t)x) : kPanel;
  if (focus) fill = kAccent;
  const int bh = focus ? h + 4 : h;
  const int by = focus ? y - 2 : y;
  tft.fillRoundRect(x, by, w, bh, 6, fill);
  tft.drawRoundRect(x, by, w, bh, 6, on || focus ? kAccent : kBorder);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor((on || focus) ? kBg : kText, fill);
  tft.drawString(label, x + w / 2, by + bh / 2, 2);
}

static void drawRibbon() {
  using namespace DspUi;
  tft.fillRect(0, kRibbonY, kW, kH - kRibbonY, kPanel);
  const int gap = 6;
  const int bw = (kW - gap * 5) / 4;
  int x = gap;
  drawRibbonButton(x, kRibbonY + 4, bw, kRibbonH - 8, "SQ", state.sq, false);
  x += bw + gap;
  drawRibbonButton(x, kRibbonY + 4, bw, kRibbonH - 8, "BS", state.bs, false);
  x += bw + gap;
  drawRibbonButton(x, kRibbonY + 4, bw, kRibbonH - 8, "DR", state.dr, false);
  x += bw + gap;
  drawRibbonButton(x, kRibbonY + 4, bw, kRibbonH - 8, "MTR", state.metro, false);

  drawRibbonButton(kW - 54, kRibbonY + 2, 46, kRibbonH - 4, state.patternView ? "TRN" : "PAT", true, true);
}

static void redraw() {
  uiFrame = millis();
  tft.fillScreen(DspUi::kBg);
  drawHeader();
  drawRibbonTitle(state.patternView ? "PATTERN" : "TRANSPORT");
  if (state.patternView) drawPatternBody();
  else drawTransportBody();
  drawFooter();
  drawRibbon();
}

static void parseTransport(const String &line) {
  state.linked = true;
  state.patternView = false;
  int i = line.indexOf("bpm=");
  if (i >= 0) state.bpm = (unsigned)line.substring(i + 4).toInt();
  i = line.indexOf(" slot=");
  if (i >= 0) state.slot = line.substring(i + 6).toInt();
  i = line.indexOf(" inst=");
  if (i >= 0) {
    int e = line.indexOf(' ', i + 6);
    line.substring(i + 6, e < 0 ? line.length() : e).toCharArray(state.inst, sizeof(state.inst));
  }
  i = line.indexOf(" chord=");
  if (i >= 0) {
    int e = line.indexOf(' ', i + 7);
    line.substring(i + 7, e < 0 ? line.length() : e).toCharArray(state.chord, sizeof(state.chord));
  }
  i = line.indexOf(" scale=");
  if (i >= 0) {
    int e = line.indexOf(' ', i + 7);
    line.substring(i + 7, e < 0 ? line.length() : e).toCharArray(state.scale, sizeof(state.scale));
  }
  i = line.indexOf(" sq=");
  if (i >= 0) state.sq = line.charAt(i + 4) == '1';
  i = line.indexOf(" bs=");
  if (i >= 0) state.bs = line.charAt(i + 4) == '1';
  i = line.indexOf(" dr=");
  if (i >= 0) state.dr = line.charAt(i + 4) == '1';
  i = line.indexOf(" st=");
  if (i >= 0) state.step = line.substring(i + 4).toInt();
  i = line.indexOf(" metro=");
  if (i >= 0) state.metro = line.charAt(i + 7) == '1';
  i = line.indexOf(" lock=");
  if (i >= 0) state.chordLock = line.charAt(i + 6) == '1';
  redraw();
}

static void parsePattern(const String &line) {
  state.linked = true;
  state.patternView = true;
  int i = line.indexOf("bpm=");
  if (i >= 0) state.bpm = (unsigned)line.substring(i + 4).toInt();
  i = line.indexOf(" st=");
  if (i >= 0) state.step = line.substring(i + 4).toInt();
  i = line.indexOf(" win=");
  if (i >= 0) state.win = line.substring(i + 5).toInt();
  i = line.indexOf(" seq=");
  if (i >= 0) {
    int e = line.indexOf(" bass=");
    line.substring(i + 5, e < 0 ? line.length() : e).toCharArray(state.seqRow, sizeof(state.seqRow));
  }
  i = line.indexOf(" bass=");
  if (i >= 0) {
    int e = line.indexOf(" drum=");
    line.substring(i + 6, e < 0 ? line.length() : e).toCharArray(state.bassRow, sizeof(state.bassRow));
  }
  i = line.indexOf(" drum=");
  if (i >= 0) line.substring(i + 6).toCharArray(state.drumRow, sizeof(state.drumRow));
  redraw();
}

static void pollPillRx() {
  while (pill.available()) {
    const char c = (char)pill.read();
    if (c == '\r') continue;
    if (c == '\n') {
      const String line = rxLine;
      rxLine = "";
      if (line.startsWith("cyd transport ")) parseTransport(line);
      else if (line.startsWith("cyd pattern ")) parsePattern(line);
    } else if (rxLine.length() < 160) {
      rxLine += c;
    }
  }
}

static void handleTouch() {
  if (!touch.touched()) return;
  TS_Point p = touch.getPoint();
  const int x = map(p.x, 200, 3700, 0, DspUi::kW);
  const int y = map(p.y, 240, 3800, 0, DspUi::kH);

  if (y >= DspUi::kRibbonY) {
    const int gap = 6;
    const int bw = (DspUi::kW - gap * 5) / 4;
    if (x < gap + bw) {
      state.sq = !state.sq;
      sendPill(state.sq ? "seq on" : "seq off");
    } else if (x < gap + (bw + gap) * 2) {
      state.bs = !state.bs;
      sendPill(state.bs ? "bass on" : "bass off");
    } else if (x < gap + (bw + gap) * 3) {
      state.dr = !state.dr;
      sendPill(state.dr ? "drums on" : "drums off");
    } else if (x < gap + (bw + gap) * 4) {
      state.metro = !state.metro;
      sendPill(state.metro ? "metro on" : "metro off");
    } else if (x >= DspUi::kW - 54) {
      state.patternView = !state.patternView;
      sendPill(state.patternView ? "display pattern" : "display transport");
    }
    redraw();
  }
}

void setup() {
  Serial.begin(115200);
  pill.begin(CYD_UART_BAUD, SERIAL_8N1, CYD_PILL_RX, CYD_PILL_TX);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(DspUi::kBg);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  redraw();
  Serial.println("LUMPY CYD · DSP DREAMS UI · PA2->GPIO27 PA3<-GPIO22");
}

void loop() {
  pollPillRx();
  handleTouch();

  static uint32_t lastAnim = 0;
  const uint32_t now = millis();
  if (now - lastAnim > 120) {
    lastAnim = now;
    if (state.linked) {
      drawFooter();
      drawRibbon();
      if (state.patternView) {
        drawPatternBody();
      } else {
        if (state.sq || state.bs || state.dr) {
          drawParamChip(8, 104, 148, 38, "SEQ", state.sq ? "ON" : "OFF", state.sq);
          drawParamChip(164, 104, 148, 38, "LAYERS",
                        state.bs && state.dr ? "BS+DR" : state.bs ? "BS" : state.dr ? "DR" : "---",
                        state.bs || state.dr);
        }
      }
    }
  }
  delay(8);
}
