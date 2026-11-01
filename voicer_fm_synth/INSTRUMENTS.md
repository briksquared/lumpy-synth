# STK instrument presets — flash without editing headers

| # | Name | Command |
|---|------|---------|
| 0 | BeeThree (FM EP) | `.\flash_instrument.ps1 -Preset beethree` |
| 1 | Clarinet | `.\flash_instrument.ps1 -Preset clarinet` |
| 2 | Plucked | `.\flash_instrument.ps1 -Preset plucked` |
| 3 | StifKarp | `.\flash_instrument.ps1 -Preset stifkarp` |
| 4 | Rhodey | `.\flash_instrument.ps1 -Preset rhodey` |
| 5 | Wurley | `.\flash_instrument.ps1 -Preset wurley` |
| 6 | Brass | `.\flash_instrument.ps1 -Preset brass` |
| 7 | Flute | `.\flash_instrument.ps1 -Preset flute` |
| 8 | Moog | `.\flash_instrument.ps1 -Preset moog` |
| 9 | Mandolin | `.\flash_instrument.ps1 -Preset mandolin` |
| 10 | Saxofony | `.\flash_instrument.ps1 -Preset saxofony` |
| 11 | TubeBell | `.\flash_instrument.ps1 -Preset tubebell` |

Or by number: `.\flash_instrument.ps1 -Number 2`

Verify all compile: `.\build_all_instruments.ps1`

Edit default in firmware: `instrument_config.h` → `STK_VOICE_PRESET`, then `.\flash.ps1`
