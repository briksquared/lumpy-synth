# LUMPY SYNTH — ESP32 touch companion

An optional touch UI for [LUMPY SYNTH](../README.md), running on an **ESP32-2432S028R**
("Cheap Yellow Display" — ILI9341 240×320 + XPT2046 resistive touch). It communicates with the
STM32 "Black Pill" over UART.

## Build & flash (PlatformIO)

```bash
pio run -t upload
```

Wire the display's UART to the pill — defaults are `TX = GPIO27`, `RX = GPIO22`, 115200 baud
(see `build_flags` in [`platformio.ini`](platformio.ini)). The display libraries (TFT_eSPI,
XPT2046_Touchscreen) are fetched automatically.
