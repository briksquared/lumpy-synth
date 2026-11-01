#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Transmission Works · DSP DREAMS perform-home palette (UI_CONTEXT_SPEC)
namespace DspUi {
constexpr uint16_t kBg = 0x0841;       // ~#0B0D10
constexpr uint16_t kPanel = 0x1082;    // ~#151A21
constexpr uint16_t kBorder = 0x3186;   // ~#2D3748
constexpr uint16_t kAccent = 0x4F5F; // ~#2FE6D6
constexpr uint16_t kAccentDim = 0x2D4F;
constexpr uint16_t kText = 0xDEFB;    // ~#E2E8F0
constexpr uint16_t kTextDim = 0x6B6D;  // ~#718096
constexpr uint16_t kLive = 0x07E0;
constexpr uint16_t kWarn = 0xF800;
constexpr uint16_t kMorph = 0xFD20;

constexpr int kW = 320;
constexpr int kH = 240;
constexpr int kHeaderH = 34;
constexpr int kFooterY = 188;
constexpr int kRibbonY = 204;
constexpr int kRibbonH = 32;

inline uint16_t breathe(uint16_t base, uint16_t peak, uint32_t t, uint32_t periodMs = 1400) {
  const float phase = (float)(t % periodMs) / (float)periodMs;
  const float w = 0.5f + 0.5f * sinf(phase * 6.28318f);
  const int r = (int)((1.0f - w) * ((base >> 11) & 0x1F) + w * ((peak >> 11) & 0x1F));
  const int g = (int)((1.0f - w) * ((base >> 5) & 0x3F) + w * ((peak >> 5) & 0x3F));
  const int b = (int)((1.0f - w) * (base & 0x1F) + w * (peak & 0x1F));
  return (uint16_t)((r & 0x1F) << 11 | (g & 0x3F) << 5 | (b & 0x1F));
}
}  // namespace DspUi
