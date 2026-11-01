#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "chord_engine.h"

class SeqPlayer;

struct BenchTransportState {
  bool drumsOn = false;
  bool bassOn = false;
  bool monoOn = false;
  bool legatoOn = true;
  bool glideOn = true;
  bool unisonOn = false;
  bool harmOn = false;
  uint8_t harmSemi = 4;
  bool seqAccent = true;
  uint8_t swingPct = 0;  // 0–55 (%)
};

// Four bench snapshots — chord metadata + per-slot pattern banks.
class PresetStore {
 public:
  static constexpr int kSlots = 4;
  static constexpr int kBytesPerSlot = 8;
  static constexpr int kSeqSteps = 32;

  // Legacy flat layout (bank 0 migration fallback)
  static constexpr int kSeqPatternAddr = kSlots * kBytesPerSlot;
  static constexpr int kTransportAddr = kSeqPatternAddr + 2 + kSeqSteps;
  static constexpr int kTransportBytes = 5;
  static constexpr int kBassPatternAddr = kTransportAddr + kTransportBytes;
  static constexpr int kBassPatternBytes = 17;
  static constexpr int kDrumPatternAddr = kBassPatternAddr + kBassPatternBytes;
  static constexpr int kDrumPatternBytes = 66;

  static constexpr int kPatternBankBase = 162;
  static constexpr int kSeqBlockBytes = 34;
  static constexpr int kBassBlockBytes = 17;
  static constexpr int kDrumBlockBytes = 66;
  static constexpr int kPatternBankBytes =
      kSeqBlockBytes + kBassBlockBytes + kDrumBlockBytes + kTransportBytes;

  void begin();

  bool save(int slot, const ChordEngine &chords, bool demoActive, int octaveShift,
            bool midiQuantize, bool genMode, bool seqEnabled, unsigned seqBpm);
  bool load(int slot, ChordEngine &chords, bool &demoActive, int &octaveShift,
            bool &midiQuantize, bool &genMode, bool &seqEnabled, unsigned &seqBpm);

  bool slotValid(int slot) const;
  bool copySlot(int from, int to);
  bool copyPatternBank(int from, int to);
  bool cloneSlot(int from, int to);
  bool eraseSlot(int slot);

  bool saveSeqPattern(int bank, const SeqPlayer &seq);
  bool loadSeqPattern(int bank, SeqPlayer &seq);

  bool saveTransport(int bank, const BenchTransportState &state);
  bool loadTransport(int bank, BenchTransportState &state);

  bool saveBassPattern(int bank, const int8_t *degrees, int count);
  bool loadBassPattern(int bank, int8_t *degrees, int count);

  bool saveDrumPattern(int bank, const uint8_t *kick, const uint8_t *hat, int count,
                       int length);
  bool loadDrumPattern(int bank, uint8_t *kick, uint8_t *hat, int count, int &length);

  struct ChordSnapshot {
    bool valid = false;
    int8_t root = 60;
    uint8_t chordType = 0;
    uint8_t inversion = 0;
    uint8_t scale = 0;
    int8_t octaveShift = 0;
    bool demo = false;
    bool arp = false;
    bool seqOn = false;
    unsigned seqBpm = 120;
  };

  bool readChordSnapshot(int slot, ChordSnapshot &out) const;
  bool chordSnapshotsEqual(int a, int b) const;
  int chordSlotDiffCount(int a, int b) const;
  int patternBankDiffCount(int a, int b) const;

  void exportSlot(int slot, Stream &out) const;
  bool writeChordSnapshot(int slot, const ChordSnapshot &snap);
  bool importPatternBankHex(int bank, const char *hex, int hexLen = -1);

  // Copy legacy flat bank-0 blocks into pattern bank 0 if missing.
  bool migrateLegacyBank0();

 private:
  static constexpr uint8_t kMagic = 0xA7;

  struct Data {
    uint8_t magic;
    int8_t root;
    uint8_t chordType;
    uint8_t inversion;
    uint8_t flags;
    uint8_t reserved[3];
  };

  int baseAddr(int slot) const { return slot * kBytesPerSlot; }
  int patternBankAddr(int bank) const;
  int seqAddr(int bank) const;
  int bassAddr(int bank) const;
  int drumAddr(int bank) const;
  int transportAddr(int bank) const;

  bool readSlot(int slot, Data &out) const;
  void writeSlot(int slot, const Data &in);

  bool loadSeqPatternLegacy(SeqPlayer &seq);
  bool loadTransportLegacy(BenchTransportState &state);
  bool loadBassPatternLegacy(int8_t *degrees, int count);
  bool loadDrumPatternLegacy(uint8_t *kick, uint8_t *hat, int count, int &length);
};
