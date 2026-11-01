#pragma once

#include <Arduino.h>

class PresetStore;

// Global EEPROM layout header (v2) — after four pattern banks @ addr 650.
class BenchEeprom {
 public:
  static constexpr int kHdrAddr = 650;
  static constexpr uint8_t kMagic = 0xEE;
  static constexpr uint8_t kSchema = 2;

  void begin(PresetStore &presets);

  uint8_t schema() const { return schema_; }
  uint8_t activeSlot() const { return activeSlot_; }
  bool migratedFromV1() const { return migratedFromV1_; }

  void setActiveSlot(uint8_t slot);
  void persist();

  bool migrateFromV1(PresetStore &presets);
  bool verifyHeaderRoundTrip();
  void printStatus(Stream &out, const PresetStore &presets) const;

 private:
  struct Hdr {
    uint8_t magic;
    uint8_t schema;
    uint8_t activeSlot;
    uint8_t flags;
    uint8_t apiMajor;
    uint8_t apiMinor;
    uint8_t apiPatch;
    uint8_t reserved;
  };

  bool loadHeader();
  void writeHeader();

  uint8_t schema_ = 0;
  uint8_t activeSlot_ = 0;
  bool migratedFromV1_ = false;
};
