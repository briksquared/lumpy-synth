#pragma once

#include <Arduino.h>

// Quadrature encoder — CLK + DT pins, optional push on SW (active low).
class RotaryEncoder {
 public:
  void begin(uint32_t pinClk, uint32_t pinDt, uint32_t pinSw = 0xFFFFFFFFu);

  // +1 / -1 / 0 per poll; SW returns true on press edge.
  int readStep();
  bool readSwEdge();
  bool readSwHold(unsigned holdMs);

 private:
  uint32_t clk_ = 0;
  uint32_t dt_ = 0;
  uint32_t sw_ = 0xFFFFFFFFu;
  bool hasSw_ = false;
  uint8_t lastAB_ = 0;
  bool swLast_ = false;
  unsigned long swEdgeMs_ = 0;
  bool swHoldFired_ = false;
};
