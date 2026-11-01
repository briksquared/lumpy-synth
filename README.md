# LUMPY SYNTH

A six-voice polysynth firmware for the **STM32F411 "Black Pill"** with a **CJMCU-1334 (UDA1334A)**
I2S DAC — a breadboard synth you play over MIDI. It runs the
[Arduino-STK](https://github.com/pschatzmann/Arduino-STK) synthesis engine on a custom **I2S2 DMA**
driver, with a chord engine, sequencers, a drum grid, and EEPROM preset banks.

## Features

- **6 voices**, 44.1 kHz, I2S2 DMA audio out
- Chord engine, strum player, mono lead, harmony lane
- Step sequencer, bass sequencer, and a drum grid
- EEPROM preset banks
- MIDI in over UART, with CC mapping
- Optional **SSD1306 OLED** on the pill, and an optional **ESP32 touch-display companion** over
  UART (see [`esp32_cyd_display/`](esp32_cyd_display/))

## Hardware

- STM32F411CE "Black Pill"
- CJMCU-1334 / UDA1334A I2S DAC
- Rotary encoders, buttons, and a pot
- *(optional)* SSD1306 128×32 OLED
- *(optional)* ESP32-2432S028R "Cheap Yellow Display" as a touch UI

Full pin map and wiring: [`voicer_fm_synth/WIRING.md`](voicer_fm_synth/WIRING.md).

## Build & flash (PlatformIO)

```bash
cd voicer_fm_synth
pio run                 # builds; fetches Arduino-STK automatically
pio run -t upload       # flash over USB DFU (hold BOOT0, tap reset, release)
```

- Controls: [`voicer_fm_synth/CONTROL_CARD.md`](voicer_fm_synth/CONTROL_CARD.md)
- Instruments: [`voicer_fm_synth/INSTRUMENTS.md`](voicer_fm_synth/INSTRUMENTS.md)

## Optional touch display (ESP32)

[`esp32_cyd_display/`](esp32_cyd_display/) is a companion UI for an ESP32-2432S028R that talks to the
pill over UART. Build it with PlatformIO and wire its TX/RX to the pill (see that folder's README).

## License

[MIT](LICENSE). Built on [Arduino-STK](https://github.com/pschatzmann/Arduino-STK). Provided as-is —
flashing hardware is at your own risk; back up anything you care about first.
