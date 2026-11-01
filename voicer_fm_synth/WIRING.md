# F411 Synth Bench — Wiring

```
                    BLACK PILL F411                    CJMCU-1334 (UDA1334A)
                   ┌──────────────┐                   ┌──────────────┐
              3V3 ─┤ 3V3          │                   │ VIN          │
              GND ─┤ GND          ├───────────────────┤ GND          │
                   │              │                   │              │
         PB13 BCLK ┤              ├───────────────────┤ BCLK         │
         PB12 WSEL ┤              ├───────────────────┤ WSEL         │
         PB15  DIN ┤              ├───────────────────┤ DIN          │
                   │              │                   │  ● 3.5mm jack│
    PA0  USER btn  ┤              │                   └──────────────┘
    PA1  VOL pot   ┤              │
                   │   USB-C      │  → PC serial @ 115200 (MIDI + text)
                   └──────────────┘

  POT (10k):  GND ────[====]──── 3V3     wiper → pin
  BUTTON:     pin ────[ SW ]──── GND     (internal pull-up)
```

## Required (minimum audio)

| Signal | MCU | DAC |
|--------|-----|-----|
| Power | 3V3 | VIN |
| Ground | GND | GND |
| Bit clock | PB13 | BCLK |
| Word select | PB12 | WSEL |
| Data | PB15 | DIN |

## Optional controls

See `CONTROL_CARD.md` for the full pin map (pots PA1/PA4/PA5/PB0/PB1/PA10, buttons PB2–PB9, encoder PB3/PB4/PB10, sustain PA2).

## UART MIDI (optional DIN jack)

| Signal | MCU pin |
|--------|---------|
| MIDI IN (opto out) | **PA10** (USART1 RX @ 31250) |
| GND | GND |

Enable firmware: `.\flash_uart_midi.ps1` or `#define BENCH_UART_MIDI 1` in `bench_config.h`.

**Do not** wire the width pot to PA10 when UART MIDI is enabled — use **PA11** for width (conflicts with USB DM; keep width off).

### DIN MIDI In BOM (BPSynth-compatible)

| Part | Qty | Notes |
|------|-----|-------|
| 5-pin DIN socket | 1 | Panel mount |
| 6N137 optocoupler | 1 | Or PC900 |
| 220 Ω resistor | 1 | MIDI In series |
| 10 kΩ resistor | 1 | Opto pull-up |
| 1N4148 diode | 1 | Reverse protection |
| Jumper wire | — | Opto out → PA10 |

```
DIN pin 4/5 ──[220Ω]──┬── opto LED ── GND
                       └── 1N4148 (cathode to DIN)
opto out ── PA10 (MCU RX)
opto VCC ── 3V3   GND ── GND
```

Verify: `bench midi` on serial · `.\go.ps1 -MidiOnly`

## PCM5102A DAC swap (optional upgrade)

Same **I2S2** wiring as CJMCU — swap the module, reflash is optional (firmware binary unchanged unless you set `BENCH_DAC_PCM5102`).

```
                    BLACK PILL F411                    PCM5102A breakout
                   ┌──────────────┐                   ┌──────────────┐
              3V3 ─┤ 3V3          │                   │ VIN / 3V3    │
              GND ─┤ GND          ├───────────────────┤ GND          │
         PB13 BCLK ┤              ├───────────────────┤ BCK / BCLK   │
         PB12 WSEL ┤              ├───────────────────┤ LRCK / WS    │
         PB15  DIN ┤              ├───────────────────┤ DIN / DATA   │
                   │              │                   │ XMT ── 3V3   │  (unmute — tie on module)
                   └──────────────┘                   │ FMT ── GND   │  (I2S Philips)
                                                      │  ● line out  │
                                                      └──────────────┘
```

| Signal | MCU | PCM5102A | UDA1334A (current) |
|--------|-----|----------|---------------------|
| BCLK | PB13 | BCK | BCLK |
| LRCLK | PB12 | LRCK / WS | WSEL |
| DATA | PB15 | DIN | DIN |
| Mute | — | **XMT → 3V3** (or GPIO HIGH) | n/a |
| Format | — | **FMT → GND** (I2S) | fixed on module |

In `bench_config.h`:

```c
#define BENCH_DAC_PCM5102 1   // after hardware swap
#define BENCH_PCM_XMT_PIN 0   // 0 = strap XMT to 3V3; or PBx for soft mute
```

Verify: `bench dac` on serial · `.\go.ps1 -DacOnly`

**A/B tip:** Record noise floor with phone mic — PCM5102A is typically ~12 dB quieter SNR than UDA1334A. See `planning/LUMPY_SYNTH_RESEARCH.md` §6.

## OLED (optional SSD1306 128x32)

| Signal | MCU pin |
|--------|---------|
| SDA | PB9 |
| SCL | PB8 |
| VCC | 3V3 |
| GND | GND |

Enable: `.\flash_oled.ps1` or `#define BENCH_OLED 1` in `bench_config.h`.

## ESP32-2432S028R touch display (optional CYD companion)

**ESP32-2432S028R** — 2.8" ILI9341 240×320 + XPT2046 resistive touch (“Cheap Yellow Display”). Runs separate firmware in `esp32_cyd_display/`; talks to the Black Pill over UART.

| Signal | Black Pill | CYD board |
|--------|------------|-----------|
| TX | **PA2** (USART2) | **GPIO 27** RX |
| RX | **PA3** (USART2) | **GPIO 22** TX |
| Ground | GND | GND |

1. Flash pill with CYD UART: `.\flash_cyd_uart.ps1` or `#define BENCH_CYD_UART 1`
2. Flash display: `cd ..\esp32_cyd_display` → `pio run -t upload`
3. Full guide: [`esp32_cyd_display/README.md`](../esp32_cyd_display/README.md)

Touch sends `seq on`, `bass on`, `display pattern`, `metro on`, etc. Pill pushes `cyd transport` / `cyd pattern` lines at 4 Hz.

**Pin note:** PA2/PA3 are normally SLOT and OCT− buttons — leave those unwired when CYD UART is enabled.

## Flash / run

1. **BOOT0 → 3V3**, tap **NRST** → DFU flash
2. Release **BOOT0**, tap **NRST** → normal run
3. Slow LED blink = running; solid LED = audio activity
