#include "i2s2_dac.h"

namespace {

struct PllPreset {
  uint32_t m;
  uint32_t n;
  uint32_t r;
};

constexpr PllPreset kPllPresets[] = {
    {25, 344, 2},
    {16, 192, 2},
    {25, 271, 2},
    {16, 271, 2},
    {25, 302, 2},
    {8, 256, 5},
    {16, 384, 4},
};

constexpr size_t kMaxDmaFrames = 512;
static int16_t g_dmaBuf[kMaxDmaFrames * 2];

void setupPins() {
  __HAL_RCC_GPIOB_CLK_ENABLE();
  pin_function(digitalPinToPinName(PB12),
               STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI2));
  pin_function(digitalPinToPinName(PB13),
               STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI2));
  pin_function(digitalPinToPinName(PB15),
               STM_PIN_DATA(STM_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI2));
}

bool applyPll(const PllPreset &pll) {
  RCC_PeriphCLKInitTypeDef clk{};
  clk.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  clk.PLLI2S.PLLI2SM = pll.m;
  clk.PLLI2S.PLLI2SN = pll.n;
  clk.PLLI2S.PLLI2SR = pll.r;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    return false;
  }
  __HAL_RCC_I2S_CONFIG(RCC_I2SCLKSOURCE_PLLI2S);
  return __HAL_RCC_GET_FLAG(RCC_FLAG_PLLI2SRDY) != RESET;
}

uint32_t pllSourceHz() {
  if ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLSOURCE_HSE &&
      __HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY)) {
    return HSE_VALUE;
  }
  return HSI_VALUE;
}

uint32_t i2sClockHz() {
  uint32_t clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S);
  if (clk >= 1000000U) {
    return clk;
  }
  const uint32_t pllm = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SM) >>
                        RCC_PLLI2SCFGR_PLLI2SM_Pos;
  const uint32_t plln = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SN) >>
                        RCC_PLLI2SCFGR_PLLI2SN_Pos;
  const uint32_t pllr = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SR) >>
                        RCC_PLLI2SCFGR_PLLI2SR_Pos;
  if (pllm == 0U || pllr == 0U) {
    return 0U;
  }
  return pllSourceHz() / pllm * plln / pllr;
}

bool computeDivider(uint32_t i2sclk, uint32_t sampleRateHz, uint32_t &div,
                    uint32_t &odd) {
  const uint32_t packetlength = 32U;
  if (sampleRateHz == 0U || i2sclk < packetlength) {
    return false;
  }
  uint32_t tmp = ((((i2sclk / packetlength) * 10U) / sampleRateHz) + 5U) / 10U;
  odd = tmp & 1U;
  div = (tmp - odd) / 2U;
  return div >= 2U && div <= 255U;
}

void fillHi2sDefaults(I2S_HandleTypeDef &hi2s, uint32_t sampleRateHz) {
  hi2s.Instance = SPI2;
  hi2s.State = HAL_I2S_STATE_RESET;
  hi2s.ErrorCode = HAL_I2S_ERROR_NONE;
  hi2s.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s.Init.AudioFreq = sampleRateHz;
  hi2s.Init.CPOL = I2S_CPOL_LOW;
  hi2s.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
}

bool tunePllAndDivider(uint32_t sampleRateHz, uint32_t &divOut, uint32_t &oddOut) {
  bool pllLocked = false;
  for (const PllPreset &pll : kPllPresets) {
    if (applyPll(pll)) {
      pllLocked = true;
      break;
    }
  }
  if (!pllLocked) {
    applyPll(kPllPresets[0]);
  }

  uint32_t i2sclk = i2sClockHz();
  if (i2sclk < 1000000U) {
    i2sclk = pllSourceHz() / 16U * 192U / 2U;
  }

  uint32_t div = 61U;
  uint32_t odd = 0U;
  if (!computeDivider(i2sclk, sampleRateHz, div, odd)) {
    div = 61U;
    odd = 0U;
  }

  divOut = div;
  oddOut = odd;
  return true;
}

void applyManualSpi2Registers(uint32_t div, uint32_t odd) {
  SPI2->I2SCFGR = 0U;
  SPI2->I2SPR = div | (odd << 8U);
  SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD | I2S_MODE_MASTER_TX |
                  I2S_STANDARD_PHILIPS | I2S_DATAFORMAT_16B;
}

}  // namespace

static DMA_HandleTypeDef g_hdma;

void setupDmaLink(I2S_HandleTypeDef &hi2s) {
  __HAL_RCC_DMA1_CLK_ENABLE();

  if (g_hdma.Instance != 0) {
    HAL_DMA_DeInit(&g_hdma);
  }

  g_hdma.Instance = DMA1_Stream4;
  g_hdma.Init.Channel = DMA_CHANNEL_0;
  g_hdma.Init.Direction = DMA_MEMORY_TO_PERIPH;
  g_hdma.Init.PeriphInc = DMA_PINC_DISABLE;
  g_hdma.Init.MemInc = DMA_MINC_ENABLE;
  g_hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  g_hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  g_hdma.Init.Mode = DMA_CIRCULAR;
  g_hdma.Init.Priority = DMA_PRIORITY_HIGH;
  g_hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  HAL_DMA_Init(&g_hdma);
  __HAL_LINKDMA(&hi2s, hdmatx, g_hdma);

  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  HAL_NVIC_SetPriority(SPI2_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SPI2_IRQn);
}

I2s2Dac *g_dac = nullptr;

extern "C" void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s) {
  if (hi2s->Instance != SPI2 || g_dac == nullptr) {
    return;
  }
  __HAL_RCC_SPI2_CLK_ENABLE();
  setupPins();
  setupDmaLink(*hi2s);
}

extern "C" void HAL_I2S_MspDeInit(I2S_HandleTypeDef *hi2s) {
  if (hi2s->Instance == SPI2) {
    HAL_DMA_DeInit(hi2s->hdmatx);
    __HAL_RCC_SPI2_CLK_DISABLE();
  }
}

extern "C" void DMA1_Stream4_IRQHandler() {
  if (g_dac != nullptr) {
    g_dac->onDmaIrq();
  }
}

extern "C" void SPI2_IRQHandler() {
  if (g_dac != nullptr) {
    g_dac->onI2sIrq();
  }
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
  if (g_dac != nullptr && hi2s->Instance == SPI2) {
    g_dac->fillHalfBuffer(0);
  }
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
  if (g_dac != nullptr && hi2s->Instance == SPI2) {
    g_dac->fillHalfBuffer(1);
  }
}

bool I2s2Dac::begin(uint32_t sampleRateHz) {
  ok_ = false;
  streaming_ = false;
  g_dac = this;

  if (hi2s_.State != HAL_I2S_STATE_RESET) {
    HAL_I2S_DMAStop(&hi2s_);
    HAL_I2S_DeInit(&hi2s_);
  }

  uint32_t div = 0;
  uint32_t odd = 0;
  if (!tunePllAndDivider(sampleRateHz, div, odd)) {
    return false;
  }

  setupPins();
  __HAL_RCC_SPI2_CLK_ENABLE();

  fillHi2sDefaults(hi2s_, sampleRateHz);
  if (HAL_I2S_Init(&hi2s_) != HAL_OK) {
    fillHi2sDefaults(hi2s_, sampleRateHz);
    hi2s_.Instance = SPI2;
    hi2s_.State = HAL_I2S_STATE_READY;
    applyManualSpi2Registers(div, odd);
    setupDmaLink(hi2s_);
  } else {
    SPI2->I2SPR = div | (odd << 8U);
  }

  ok_ = true;
  return true;
}

void I2s2Dac::setFillCallback(FillFn fn, void *ctx) {
  fillFn_ = fn;
  fillCtx_ = ctx;
}

void I2s2Dac::fillHalfBuffer(int half) {
  if (fillFn_ == nullptr || dmaBuf_ == nullptr || bufferFrames_ == 0) {
    return;
  }
  const size_t halfFrames = bufferFrames_ / 2U;
  int16_t *dest = dmaBuf_ + (half * halfFrames * 2U);
  fillFn_(dest, halfFrames, fillCtx_);
}

bool I2s2Dac::startStream(size_t bufferFrames) {
  if (!ok_ || fillFn_ == nullptr || bufferFrames < 64 || (bufferFrames % 2) != 0) {
    return false;
  }

  if (bufferFrames > kMaxDmaFrames) {
    bufferFrames = kMaxDmaFrames;
  }

  if (streaming_) {
    HAL_I2S_DMAStop(&hi2s_);
  }

  bufferFrames_ = bufferFrames;
  dmaBuf_ = g_dmaBuf;

  fillHalfBuffer(0);
  fillHalfBuffer(1);

  const uint16_t dmaCount = (uint16_t)(bufferFrames_ * 2U);
  if (HAL_I2S_Transmit_DMA(&hi2s_, (uint16_t *)dmaBuf_, dmaCount) != HAL_OK) {
    streaming_ = false;
    return false;
  }

  streaming_ = true;
  return true;
}

void I2s2Dac::onI2sIrq() { HAL_I2S_IRQHandler(&hi2s_); }

void I2s2Dac::onDmaIrq() { HAL_DMA_IRQHandler(&g_hdma); }
