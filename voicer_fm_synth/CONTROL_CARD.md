# F411 Synth Bench — Control Card

Print or keep on phone while wiring.

## Audio (required)

| Black Pill | CJMCU-1334 |
|------------|------------|
| 3V3 | VIN |
| GND | GND |
| PB13 | BCLK |
| PB12 | WSEL |
| PB15 | DIN |

**PCM5102A swap:** same I2S2 pins; strap **XMT → 3V3** and **FMT → GND** on module. Set `BENCH_DAC_PCM5102 1` in `bench_config.h`. Details: **`WIRING.md`**.

## Default groove panel (v1.41)

Wire only what you have — disable unused blocks in `bench_config.h` (`#define BENCH_* 0`).

### Pots (10k linear)

GND ────[ pot ]──── 3V3  
              wiper → pin

| Label | Pin | Function |
|-------|-----|----------|
| VOL | PA1 | Master volume |
| MOD | PA4 | Vibrato / mod (CC1) · **cutoff** when `perf moog` |
| COLOR | PB0 | Noise / tone (CC4) · **resonance** when `perf moog` |
| TEMPO | PB1 | BPM + demo/arp speed |
| SWING | PA5 | Seq swing 0–55% |
| BREATH | PA5 | *(optional — disable SWING)* breath CC128 |
| WIDTH | PA11 | Stereo width — **conflicts with USB** |

### Buttons (pin → GND, internal pull-up)

| Label | Pin | Function |
|-------|-----|----------|
| CHORD | PA0 | Short = next inversion · **hold 600 ms** = next chord type |
| DEMO | PB2 | Toggle auto demo |
| PANIC | PB6 | All notes off |
| SEQ | PA6 | Toggle sequencer |
| DRUMS | PA7 | Toggle drum grid |
| BASS | PA8 | Toggle bass lane |
| TAP | PA9 | Tap tempo (sets BPM) |
| SLOT | PA2 | Short = next preset · **hold 800 ms** = commit EEPROM |
| ARP | PB5 | Toggle arpeggiator |
| OCT− | PA3 | Keyboard octave down |
| OCT+ | PB14 | Keyboard octave up |
| SUSTAIN | PA2 | Sustain pedal (optional) |

### Triad keys (default 3-button layout)

| Label | Pin | Function |
|-------|-----|----------|
| 1 | PB7 | Scale degree 0 |
| 2 | PB8 | Scale degree 1 |
| 3 | PB9 | Scale degree 2 |

Set `BENCH_NOTE_BUTTON_COUNT 7` for full diatonic row on PB7–PB9 + PA6–PA9 (disable layer buttons on PA6–8).

### Rotary encoder

| Signal | Pin |
|--------|-----|
| CLK | PB10 |
| DT | PB3 |
| SW | PB4 → push = next chord type · **hold 700 ms** = metro toggle |
| GND | GND |

**Turn** → transpose root ±1 semitone.

## LED (onboard)

| Blink speed | Meaning |
|-------------|---------|
| ~400 ms | Demo on, block chords |
| ~140 ms | Demo on, arp mode |
| ~900 ms | Demo off / live play |

## Serial verify

```
bench controls   — pot norms + button states
bench cal        — raw ADC sweep
bench lint       — pin conflict warnings
bench filter     — mono 4-pole ladder + filter envelope (MOD/CC74=cutoff, COLOR=res, CC107=env depth)
bench midi       — USB CDC + UART MIDI config
bench dac        — I2S DAC module (UDA1334A / PCM5102A)
bench eeprom     — schema v2 header, active slot, migration status
bench api        — frozen API manifest (fw, schema, test count)
bench latency    — I2S DMA buffer size and estimated ms
```

## Enable only what you wire

In `bench_config.h`:

```c
#define BENCH_LAYER_BUTTONS 0   // if PA6-PA9 used for 7-note keyboard
#define BENCH_SWING_POT 0       // if PA5 used for breath
#define BENCH_ENCODER 0
```

## MIDI / serial

Full command list: type `help` @ 115200 USB CDC.

EEPROM slots 0–3 store chord + pattern bank. **SLOT** button cycles loads; `save N` / `commit` to write.

## MIDI (USB serial or UART PA10 @ 31250)

| Message | Action |
|---------|--------|
| Note on/off | Live play |
| CC1 / CC4 | Mod / color |
| CC7 | MIDI volume |
| CC64 | Sustain pedal |
| CC44–46 | Toggle seq / drums / bass |
| CC32–35 | Load EEPROM preset slot |
| CC104 | Pattern anchor |
| CC105 | Macro prism |
| CC74 | Ladder filter cutoff (log map; auto-enables filter in mono) |
| CC4 | Ladder resonance when filter + mono active |
| CC107 | Filter envelope depth (how much cutoff sweeps on note) |
| CC108 | Pattern latch (toggle bounce + accent) |
| CC109 | Pattern rivet (thicken + align) |
| CC110 | Pattern bolt (rivet + pulse) |
| CC111 | Pattern weld (thicken + align + echo) |
| CC112 | Pattern forge (carve + rivet) |
| Clock F8 / Start FA | Transport sync (`clock on`) |

**EEPROM backup:** `eeprom export 0` — chord snapshot + pattern bank hex (paste to host log).  
**EEPROM restore:** `eeprom import 3` — paste export lines, end with `---`.  
**EEPROM clone:** `eeprom clone 0 3` — copy chord slot + pattern bank in one command.  
**EEPROM diff:** `eeprom diff 0 1` — chord + pattern bank byte diff counts.  
**Headroom:** `bench perf` — free RAM, audio peak, I2S stream status.  
**Filter CPU:** `bench filter load` — `fillAudio()` cost with mono + ladder on.  
**Filter sweep:** `bench filter sweep` — audible cutoff sweep for ladder A/B.  
**Res sweep:** `bench filter res sweep` — resonance sweep (cutoff held) for ladder Q.

### DIN MIDI In (optional)

| Part | Qty |
|------|-----|
| 5-pin DIN socket | 1 |
| 6N137 optocoupler | 1 |
| 220 Ω | 1 |
| 10 kΩ | 1 |
| 1N4148 | 1 |

Opto out → **PA10** (USART1 RX). Enable: `.\flash_uart_midi.ps1`. Full circuit: **`WIRING.md`**.

Type `help` @ 115200 for full serial command list.
