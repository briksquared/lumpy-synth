#include "cc_map.h"
#include <EEPROM.h>

void BenchCcMap::setDefaults() {
  tone = 20;    // was 74 — conflicts with kCcFilterCutoff (GM brightness)
  width = 10;   // was 71 — conflicts with kCcToggleMetro
  mod = 1;
  color = 4;
  breath = 128;
  swing = 16;
  harmSemi = 17;
}

void CcLearn::begin() {
  if (!load()) {
    map_.setDefaults();
  }
}

bool CcLearn::load() {
  uint8_t header = 0;
  EEPROM.get(kEepromAddr, header);
  if (header != kMagic) return false;
  EEPROM.get(kEepromAddr + 1, map_.tone);
  EEPROM.get(kEepromAddr + 2, map_.width);
  EEPROM.get(kEepromAddr + 3, map_.mod);
  EEPROM.get(kEepromAddr + 4, map_.color);
  EEPROM.get(kEepromAddr + 5, map_.breath);
  EEPROM.get(kEepromAddr + 6, map_.swing);
  EEPROM.get(kEepromAddr + 7, map_.harmSemi);
  return true;
}

void CcLearn::save() {
  EEPROM.put(kEepromAddr, kMagic);
  EEPROM.put(kEepromAddr + 1, map_.tone);
  EEPROM.put(kEepromAddr + 2, map_.width);
  EEPROM.put(kEepromAddr + 3, map_.mod);
  EEPROM.put(kEepromAddr + 4, map_.color);
  EEPROM.put(kEepromAddr + 5, map_.breath);
  EEPROM.put(kEepromAddr + 6, map_.swing);
  EEPROM.put(kEepromAddr + 7, map_.harmSemi);
}

void CcLearn::resetDefaults() {
  map_.setDefaults();
  pending_ = kLearnNone;
  save();
}

void CcLearn::persist() { save(); }

bool CcLearn::verifyEepromRoundTrip() {
  const BenchCcMap orig = map_;
  map_.swing = 93;
  save();
  uint8_t header = 0;
  uint8_t swing = 0;
  EEPROM.get(kEepromAddr, header);
  EEPROM.get(kEepromAddr + 6, swing);
  const bool ok = header == kMagic && swing == 93;
  map_ = orig;
  save();
  return ok;
}

uint8_t *CcLearn::slotPtr(CcLearnSlot slot) {
  switch (slot) {
    case kLearnTone:
      return &map_.tone;
    case kLearnWidth:
      return &map_.width;
    case kLearnMod:
      return &map_.mod;
    case kLearnColor:
      return &map_.color;
    case kLearnBreath:
      return &map_.breath;
    case kLearnSwing:
      return &map_.swing;
    case kLearnHarm:
      return &map_.harmSemi;
    default:
      return nullptr;
  }
}

const char *CcLearn::slotName(CcLearnSlot slot) const {
  switch (slot) {
    case kLearnTone:
      return "tone";
    case kLearnWidth:
      return "width";
    case kLearnMod:
      return "mod";
    case kLearnColor:
      return "color";
    case kLearnBreath:
      return "breath";
    case kLearnSwing:
      return "swing";
    case kLearnHarm:
      return "harm";
    default:
      return "?";
  }
}

const char *CcLearn::pendingName() const { return slotName(pending_); }

bool CcLearn::parseSlot(const String &name, CcLearnSlot &out) {
  if (name.equalsIgnoreCase("tone")) {
    out = kLearnTone;
    return true;
  }
  if (name.equalsIgnoreCase("width")) {
    out = kLearnWidth;
    return true;
  }
  if (name.equalsIgnoreCase("mod")) {
    out = kLearnMod;
    return true;
  }
  if (name.equalsIgnoreCase("color")) {
    out = kLearnColor;
    return true;
  }
  if (name.equalsIgnoreCase("breath")) {
    out = kLearnBreath;
    return true;
  }
  if (name.equalsIgnoreCase("swing")) {
    out = kLearnSwing;
    return true;
  }
  if (name.equalsIgnoreCase("harm")) {
    out = kLearnHarm;
    return true;
  }
  return false;
}

bool CcLearn::captureCc(uint8_t cc, Stream &out) {
  if (pending_ == kLearnNone) return false;
  uint8_t *slot = slotPtr(pending_);
  if (!slot) return false;
  *slot = cc;
  const CcLearnSlot learned = pending_;
  pending_ = kLearnNone;
  save();
  out.print(F("learned "));
  out.print(slotName(learned));
  out.print(F(" CC"));
  out.println(cc);
  return true;
}

void CcLearn::printMap(Stream &out) const {
  out.println(F("cc map:"));
  out.print(F("  tone="));
  out.print(map_.tone);
  out.print(F(" width="));
  out.println(map_.width);
  out.print(F("  mod="));
  out.print(map_.mod);
  out.print(F(" color="));
  out.println(map_.color);
  out.print(F("  breath="));
  out.print(map_.breath);
  out.print(F(" swing="));
  out.println(map_.swing);
  out.print(F("  harm="));
  out.println(map_.harmSemi);
  if (pending_ != kLearnNone) {
    out.print(F("  waiting: "));
    out.println(pendingName());
  }
}
