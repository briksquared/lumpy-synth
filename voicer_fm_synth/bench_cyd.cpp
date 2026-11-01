#include "bench_cyd.h"

#include "bench_config.h"

#if BENCH_CYD_UART

#include <HardwareSerial.h>

static HardwareSerial CydSerial(USART2);
static BenchCydCommandFn commandHandler_ = nullptr;
static String rxLine_;
static unsigned long lastEmitMs_ = 0;

void benchCydBegin() {
  CydSerial.setRx(kPinCydRx);
  CydSerial.setTx(kPinCydTx);
  CydSerial.begin(kCydUartBaud);
}

void benchCydSetCommandHandler(BenchCydCommandFn fn) { commandHandler_ = fn; }

void benchCydPollRx() {
  if (!commandHandler_) return;
  while (CydSerial.available()) {
    const char c = (char)CydSerial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      const String line = rxLine_;
      rxLine_ = "";
      if (line.length() > 0) commandHandler_(line);
    } else if (rxLine_.length() < 96) {
      rxLine_ += c;
    }
  }
}

static void throttleEmit() {
  const unsigned long now = millis();
  if (now - lastEmitMs_ < 250) return;
  lastEmitMs_ = now;
}

void benchCydEmitTransport(unsigned bpm, const char *inst, const char *chord, const char *scale,
                           bool seqOn, bool bassOn, bool drumsOn, bool monoOn, int step,
                           int swingPct, int activeSlot, bool muteSeq, bool muteBass, bool muteDrums,
                           bool metroOn, bool chordLock) {
  throttleEmit();
  CydSerial.print(F("cyd transport bpm="));
  CydSerial.print(bpm);
  CydSerial.print(F(" slot="));
  CydSerial.print(activeSlot);
  CydSerial.print(F(" inst="));
  CydSerial.print(inst ? inst : "?");
  CydSerial.print(F(" chord="));
  CydSerial.print(chord ? chord : "?");
  CydSerial.print(F(" scale="));
  CydSerial.print(scale ? scale : "?");
  CydSerial.print(F(" sq="));
  CydSerial.print(seqOn ? 1 : 0);
  CydSerial.print(F(" bs="));
  CydSerial.print(bassOn ? 1 : 0);
  CydSerial.print(F(" dr="));
  CydSerial.print(drumsOn ? 1 : 0);
  CydSerial.print(F(" mn="));
  CydSerial.print(monoOn ? 1 : 0);
  CydSerial.print(F(" st="));
  CydSerial.print(step % 100);
  CydSerial.print(F(" sw="));
  CydSerial.print(swingPct);
  CydSerial.print(F(" msq="));
  CydSerial.print(muteSeq ? 1 : 0);
  CydSerial.print(F(" mbs="));
  CydSerial.print(muteBass ? 1 : 0);
  CydSerial.print(F(" mdr="));
  CydSerial.print(muteDrums ? 1 : 0);
  CydSerial.print(F(" metro="));
  CydSerial.print(metroOn ? 1 : 0);
  CydSerial.print(F(" lock="));
  CydSerial.print(chordLock ? 1 : 0);
  CydSerial.println();
}

void benchCydEmitPattern(unsigned bpm, int step, int windowStart, const char *seqRow,
                         const char *bassRow, const char *drumRow) {
  throttleEmit();
  CydSerial.print(F("cyd pattern bpm="));
  CydSerial.print(bpm);
  CydSerial.print(F(" st="));
  CydSerial.print(step % 100);
  CydSerial.print(F(" win="));
  CydSerial.print(windowStart % 100);
  CydSerial.print(F(" seq="));
  CydSerial.print(seqRow ? seqRow : "");
  CydSerial.print(F(" bass="));
  CydSerial.print(bassRow ? bassRow : "");
  CydSerial.print(F(" drum="));
  CydSerial.println(drumRow ? drumRow : "");
}

#else

void benchCydBegin() {}
void benchCydSetCommandHandler(BenchCydCommandFn) {}
void benchCydPollRx() {}
void benchCydEmitTransport(unsigned, const char *, const char *, const char *, bool, bool, bool,
                           bool, int, int, int, bool, bool, bool, bool, bool) {}
void benchCydEmitPattern(unsigned, int, int, const char *, const char *, const char *) {}

#endif
