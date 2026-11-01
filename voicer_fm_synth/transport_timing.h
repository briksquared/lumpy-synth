#pragma once

#include <Arduino.h>

// Shared swing timing for 16th-note lanes (0 = straight, 0.55 = max shuffle).
inline unsigned transportSwingInterval(unsigned baseMs, int stepIndex, float swing) {
  if (swing < 0.01f || baseMs < 2) return baseMs;
  if (stepIndex & 1) {
    return baseMs + (unsigned)(baseMs * swing);
  }
  return baseMs - (unsigned)(baseMs * swing * 0.5f);
}
