#pragma once

#include <Arduino.h>

struct BenchCcMap {
  uint8_t tone = 74;
  uint8_t width = 71;
  uint8_t mod = 1;
  uint8_t color = 4;
  uint8_t breath = 128;
  uint8_t swing = 16;
  uint8_t harmSemi = 17;

  void setDefaults();
};

enum CcLearnSlot : uint8_t {
  kLearnNone = 0,
  kLearnTone,
  kLearnWidth,
  kLearnMod,
  kLearnColor,
  kLearnBreath,
  kLearnSwing,
  kLearnHarm,
};

class CcLearn {
 public:
  void begin();

  const BenchCcMap &map() const { return map_; }
  BenchCcMap &map() { return map_; }

  void setPending(CcLearnSlot slot) { pending_ = slot; }
  CcLearnSlot pending() const { return pending_; }
  void clearPending() { pending_ = kLearnNone; }

  bool captureCc(uint8_t cc, Stream &out);
  void resetDefaults();
  void persist();
  bool verifyEepromRoundTrip();
  void printMap(Stream &out) const;
  const char *pendingName() const;

  static bool parseSlot(const String &name, CcLearnSlot &out);

 private:
  static constexpr int kEepromAddr = 154;  // after drum pattern block
  static constexpr uint8_t kMagic = 0xF6;

  BenchCcMap map_{};
  CcLearnSlot pending_ = kLearnNone;

  bool load();
  void save();
  uint8_t *slotPtr(CcLearnSlot slot);
  const char *slotName(CcLearnSlot slot) const;
};
