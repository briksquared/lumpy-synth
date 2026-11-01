#pragma once

#include <Arduino.h>

extern "C" {
#include "stm32f4xx_hal.h"
}

class I2s2Dac {
 public:
  using FillFn = void (*)(int16_t *interleaved, size_t frames, void *ctx);

  bool begin(uint32_t sampleRateHz = I2S_AUDIOFREQ_44K);
  void setFillCallback(FillFn fn, void *ctx);
  bool startStream(size_t bufferFrames = 256);
  static constexpr size_t kMaxStreamFrames = 512;
  bool isReady() const { return ok_; }
  bool isStreaming() const { return streaming_; }
  size_t bufferFrames() const { return bufferFrames_; }

  void fillHalfBuffer(int half);
  void onI2sIrq();
  void onDmaIrq();

  I2S_HandleTypeDef hi2s_{};

 private:
  int16_t *dmaBuf_ = nullptr;
  size_t bufferFrames_ = 0;
  FillFn fillFn_ = nullptr;
  void *fillCtx_ = nullptr;
  bool ok_ = false;
  bool streaming_ = false;
};
