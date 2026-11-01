#include "bench_eeprom.h"
#include "bench_config.h"
#include "preset_store.h"
#include <EEPROM.h>

void BenchEeprom::setActiveSlot(uint8_t slot) {
  if (slot >= PresetStore::kSlots) slot = 0;
  activeSlot_ = slot;
}

void BenchEeprom::writeHeader() {
  Hdr h{};
  h.magic = kMagic;
  h.schema = kSchema;
  h.activeSlot = activeSlot_;
  h.flags = migratedFromV1_ ? 1 : 0;
  h.apiMajor = LUMPY_API_MAJOR;
  h.apiMinor = 0;
  h.apiPatch = LUMPY_API_PATCH;
  EEPROM.put(kHdrAddr, h);
  schema_ = kSchema;
}

bool BenchEeprom::loadHeader() {
  Hdr h{};
  EEPROM.get(kHdrAddr, h);
  if (h.magic != kMagic || h.schema < kSchema) return false;
  schema_ = h.schema;
  activeSlot_ = h.activeSlot < PresetStore::kSlots ? h.activeSlot : 0;
  migratedFromV1_ = (h.flags & 1) != 0;
  return true;
}

void BenchEeprom::persist() { writeHeader(); }

bool BenchEeprom::migrateFromV1(PresetStore &presets) {
  migratedFromV1_ = presets.migrateLegacyBank0();
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) {
      activeSlot_ = (uint8_t)i;
      break;
    }
  }
  writeHeader();
  return true;
}

void BenchEeprom::begin(PresetStore &presets) {
  if (loadHeader()) return;
  migrateFromV1(presets);
}

bool BenchEeprom::verifyHeaderRoundTrip() {
  const uint8_t orig = activeSlot_;
  const bool origMig = migratedFromV1_;
  activeSlot_ = 3;
  migratedFromV1_ = true;
  writeHeader();
  Hdr h{};
  EEPROM.get(kHdrAddr, h);
  const bool ok = h.magic == kMagic && h.schema == kSchema && h.activeSlot == 3 &&
                  (h.flags & 1) != 0;
  activeSlot_ = orig;
  migratedFromV1_ = origMig;
  writeHeader();
  return ok;
}

void BenchEeprom::printStatus(Stream &out, const PresetStore &presets) const {
  out.println(F("--- bench eeprom ---"));
  out.print(F("schema="));
  out.print(schema_);
  out.print(F(" api="));
  out.print(F411_SYNTH_VERSION);
  out.print(F(" active_slot="));
  out.println(activeSlot_);
  out.print(F("migrated_v1="));
  out.println(migratedFromV1_ ? 1 : 0);
  int valid = 0;
  for (int i = 0; i < PresetStore::kSlots; i++) {
    if (presets.slotValid(i)) valid++;
  }
  out.print(F("slots_valid="));
  out.print(valid);
  out.print(F("/"));
  out.print(PresetStore::kSlots);
  out.print(F(" pattern_bank_bytes="));
  out.println(PresetStore::kPatternBankBytes);
  out.print(F("hdr_addr="));
  out.println(kHdrAddr);
  out.println(F("---"));
}
