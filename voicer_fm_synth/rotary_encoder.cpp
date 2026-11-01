#include "rotary_encoder.h"

void RotaryEncoder::begin(uint32_t pinClk, uint32_t pinDt, uint32_t pinSw) {
  clk_ = pinClk;
  dt_ = pinDt;
  pinMode(clk_, INPUT_PULLUP);
  pinMode(dt_, INPUT_PULLUP);
  lastAB_ = ((uint8_t)digitalRead(clk_) << 1) | (uint8_t)digitalRead(dt_);

  hasSw_ = pinSw != 0xFFFFFFFFu;
  if (hasSw_) {
    sw_ = pinSw;
    pinMode(sw_, INPUT_PULLUP);
    swLast_ = digitalRead(sw_) == LOW;
  }
}

int RotaryEncoder::readStep() {
  const uint8_t ab = ((uint8_t)digitalRead(clk_) << 1) | (uint8_t)digitalRead(dt_);
  int step = 0;
  // 4-step state machine
  if (lastAB_ != ab) {
    static const int8_t table[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    const int idx = (lastAB_ << 2) | ab;
    step = table[idx & 0x0f];
    lastAB_ = ab;
  }
  return step;
}

bool RotaryEncoder::readSwEdge() {
  if (!hasSw_) return false;
  const bool down = digitalRead(sw_) == LOW;
  const unsigned long now = millis();
  const bool edge = down && !swLast_ && (now - swEdgeMs_) > 40;
  if (edge) swEdgeMs_ = now;
  swLast_ = down;
  return edge;
}

bool RotaryEncoder::readSwHold(unsigned holdMs) {
  if (!hasSw_) return false;
  const bool down = digitalRead(sw_) == LOW;
  const unsigned long now = millis();

  if (down && !swLast_) {
    swEdgeMs_ = now;
    swHoldFired_ = false;
  }
  if (down && !swHoldFired_ && (now - swEdgeMs_) >= holdMs) {
    swHoldFired_ = true;
    swLast_ = down;
    return true;
  }
  if (!down) {
    swHoldFired_ = false;
  }
  swLast_ = down;
  return false;
}
