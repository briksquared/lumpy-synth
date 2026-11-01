#pragma once

#include <Arduino.h>

// MIDI 1.0 parser: notes, CC, bend, program change, real-time clock.
class MidiParser {
 public:
  using NoteFn = void (*)(uint8_t note, uint8_t velocity, bool on, void *ctx);
  using ControlFn = void (*)(uint8_t cc, uint8_t value, void *ctx);
  using BendFn = void (*)(uint16_t value14, void *ctx);
  using ProgramFn = void (*)(uint8_t program, void *ctx);
  using ClockFn = void (*)(void *ctx);
  using StartFn = void (*)(void *ctx);

  void begin(Stream &port, uint8_t channel, NoteFn note, ControlFn control,
             BendFn bend, ProgramFn program, ClockFn clock, StartFn start,
             void *ctx) {
    port_ = &port;
    channel_ = channel & 0x0f;
    noteFn_ = note;
    controlFn_ = control;
    bendFn_ = bend;
    programFn_ = program;
    clockFn_ = clock;
    startFn_ = start;
    ctx_ = ctx;
    status_ = 0;
    data1_ = 0;
    expect_ = 0;
  }

  void poll() {
    if (!port_) return;
    while (port_->available() > 0) {
      const int raw = port_->read();
      if (raw < 0) break;
      const uint8_t b = (uint8_t)raw;

      if (b >= 0xF8) {
        status_ = 0;
        expect_ = 0;
        if (b == 0xF8 && clockFn_) clockFn_(ctx_);
        if (b == 0xFA && startFn_) startFn_(ctx_);
        continue;
      }

      if (b >= 0x80) {
        status_ = b;
        const uint8_t cmd = b & 0xf0;
        expect_ = (cmd == 0xc0 || cmd == 0xd0) ? 1 : 2;
        continue;
      }

      if (expect_ == 0) continue;
      if (expect_ == 2) {
        data1_ = b;
        expect_ = 1;
        continue;
      }

      dispatch(data1_, b);
      if (status_ < 0xf0) {
        const uint8_t cmd = status_ & 0xf0;
        expect_ = (cmd == 0xc0 || cmd == 0xd0) ? 1 : 2;
      }
    }
  }

 private:
  void dispatch(uint8_t d1, uint8_t d2) {
    const uint8_t cmd = status_ & 0xf0;
    const uint8_t ch = status_ & 0x0f;
    if (ch != channel_) return;

    if (cmd == 0x90) {
      if (!noteFn_) return;
      if (d2 == 0) {
        noteFn_(d1, 0, false, ctx_);
      } else {
        noteFn_(d1, d2, true, ctx_);
      }
    } else if (cmd == 0x80) {
      if (noteFn_) noteFn_(d1, d2, false, ctx_);
    } else if (cmd == 0xb0) {
      if (controlFn_) controlFn_(d1, d2, ctx_);
    } else if (cmd == 0xe0) {
      if (bendFn_) {
        const uint16_t v = (uint16_t)d2 << 7 | d1;
        bendFn_(v, ctx_);
      }
    } else if (cmd == 0xc0) {
      if (programFn_) programFn_(d1, ctx_);
    }
  }

  Stream *port_ = nullptr;
  uint8_t channel_ = 0;
  NoteFn noteFn_ = nullptr;
  ControlFn controlFn_ = nullptr;
  BendFn bendFn_ = nullptr;
  ProgramFn programFn_ = nullptr;
  ClockFn clockFn_ = nullptr;
  StartFn startFn_ = nullptr;
  void *ctx_ = nullptr;
  uint8_t status_ = 0;
  uint8_t data1_ = 0;
  uint8_t expect_ = 0;
};
