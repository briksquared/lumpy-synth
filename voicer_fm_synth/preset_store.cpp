#include "preset_store.h"
#include "seq_player.h"
#include "bass_seq.h"
#include "drum_grid.h"

void PresetStore::begin() {}

int PresetStore::patternBankAddr(int bank) const {
  if (bank < 0 || bank >= kSlots) return -1;
  return kPatternBankBase + bank * kPatternBankBytes;
}

int PresetStore::seqAddr(int bank) const { return patternBankAddr(bank); }

int PresetStore::bassAddr(int bank) const {
  const int base = patternBankAddr(bank);
  return base < 0 ? -1 : base + kSeqBlockBytes;
}

int PresetStore::drumAddr(int bank) const {
  const int base = patternBankAddr(bank);
  return base < 0 ? -1 : base + kSeqBlockBytes + kBassBlockBytes;
}

int PresetStore::transportAddr(int bank) const {
  const int base = patternBankAddr(bank);
  return base < 0 ? -1 : base + kSeqBlockBytes + kBassBlockBytes + kDrumBlockBytes;
}

bool PresetStore::readSlot(int slot, Data &out) const {
  if (slot < 0 || slot >= kSlots) return false;
  const int addr = baseAddr(slot);
  EEPROM.get(addr, out);
  return out.magic == kMagic;
}

void PresetStore::writeSlot(int slot, const Data &in) {
  if (slot < 0 || slot >= kSlots) return;
  const int addr = baseAddr(slot);
  EEPROM.put(addr, in);
}

bool PresetStore::slotValid(int slot) const {
  Data d{};
  return readSlot(slot, d);
}

bool PresetStore::copySlot(int from, int to) {
  if (from < 0 || from >= kSlots || to < 0 || to >= kSlots) return false;
  for (int i = 0; i < kBytesPerSlot; i++) {
    uint8_t b = 0;
    EEPROM.get(baseAddr(from) + i, b);
    EEPROM.put(baseAddr(to) + i, b);
  }
  return true;
}

bool PresetStore::copyPatternBank(int from, int to) {
  const int fromAddr = patternBankAddr(from);
  const int toAddr = patternBankAddr(to);
  if (fromAddr < 0 || toAddr < 0) return false;
  for (int i = 0; i < kPatternBankBytes; i++) {
    uint8_t b = 0;
    EEPROM.get(fromAddr + i, b);
    EEPROM.put(toAddr + i, b);
  }
  return true;
}

bool PresetStore::cloneSlot(int from, int to) {
  if (from < 0 || from >= kSlots || to < 0 || to >= kSlots) return false;
  if (!copySlot(from, to)) return false;
  return copyPatternBank(from, to);
}

bool PresetStore::eraseSlot(int slot) {
  if (slot < 0 || slot >= kSlots) return false;
  Data d{};
  writeSlot(slot, d);
  const int bankAddr = patternBankAddr(slot);
  if (bankAddr >= 0) {
    for (int i = 0; i < kPatternBankBytes; i++) {
      EEPROM.write(bankAddr + i, 0);
    }
  }
  return true;
}

bool PresetStore::save(int slot, const ChordEngine &chords, bool demoActive, int octaveShift,
                       bool midiQuantize, bool genMode, bool seqEnabled, unsigned seqBpm) {
  if (slot < 0 || slot >= kSlots) return false;
  Data d{};
  d.magic = kMagic;
  d.root = (int8_t)chords.root();
  d.chordType = (uint8_t)chords.type();
  d.inversion = (uint8_t)chords.inversion();
  d.flags = (uint8_t)((chords.arpEnabled() ? 1 : 0) | (demoActive ? 2 : 0) |
                      (midiQuantize ? 4 : 0) | (genMode ? 8 : 0) | (seqEnabled ? 16 : 0));
  d.reserved[0] = (uint8_t)chords.scale();
  d.reserved[1] = (uint8_t)(octaveShift + 128);
  d.reserved[2] = (uint8_t)constrain((int)seqBpm, 40, 240);
  writeSlot(slot, d);
  return true;
}

bool PresetStore::load(int slot, ChordEngine &chords, bool &demoActive, int &octaveShift,
                       bool &midiQuantize, bool &genMode, bool &seqEnabled, unsigned &seqBpm) {
  Data d{};
  if (!readSlot(slot, d)) return false;
  chords.setRoot(d.root);
  chords.setType((ChordType)d.chordType);
  chords.setInversion(d.inversion);
  chords.setArp((d.flags & 1) != 0);
  demoActive = (d.flags & 2) != 0;
  midiQuantize = (d.flags & 4) != 0;
  genMode = (d.flags & 8) != 0;
  seqEnabled = (d.flags & 16) != 0;
  if (d.reserved[0] < (uint8_t)ScaleMode::Count) {
    chords.setScale((ScaleMode)d.reserved[0]);
  }
  octaveShift = (int)d.reserved[1] - 128;
  seqBpm = d.reserved[2] ? d.reserved[2] : 120u;
  return true;
}

bool PresetStore::saveSeqPattern(int bank, const SeqPlayer &seq) {
  const int addr = seqAddr(bank);
  if (addr < 0) return false;
  const uint8_t header = 0xB5;
  EEPROM.put(addr, header);
  EEPROM.put(addr + 1, (uint8_t)seq.length());
  for (int i = 0; i < SeqPlayer::kMaxSteps; i++) {
    EEPROM.put(addr + 2 + i, seq.stepNote(i));
  }
  return true;
}

bool PresetStore::loadSeqPatternLegacy(SeqPlayer &seq) {
  uint8_t header = 0;
  EEPROM.get(kSeqPatternAddr, header);
  if (header != 0xB5) return false;
  uint8_t len = kSeqSteps;
  EEPROM.get(kSeqPatternAddr + 1, len);
  if (len > 0 && len <= SeqPlayer::kMaxSteps) {
    seq.setLength(len);
  }
  for (int i = 0; i < SeqPlayer::kMaxSteps; i++) {
    uint8_t n = 0;
    EEPROM.get(kSeqPatternAddr + 2 + i, n);
    seq.setStep(i, n);
  }
  return true;
}

bool PresetStore::loadSeqPattern(int bank, SeqPlayer &seq) {
  const int addr = seqAddr(bank);
  if (addr < 0) return false;
  uint8_t header = 0;
  EEPROM.get(addr, header);
  if (header != 0xB5) {
    if (bank == 0) return loadSeqPatternLegacy(seq);
    return false;
  }
  uint8_t len = kSeqSteps;
  EEPROM.get(addr + 1, len);
  if (len > 0 && len <= SeqPlayer::kMaxSteps) {
    seq.setLength(len);
  }
  for (int i = 0; i < SeqPlayer::kMaxSteps; i++) {
    uint8_t n = 0;
    EEPROM.get(addr + 2 + i, n);
    seq.setStep(i, n);
  }
  return true;
}

bool PresetStore::saveTransport(int bank, const BenchTransportState &state) {
  const int addr = transportAddr(bank);
  if (addr < 0) return false;
  const uint8_t header = 0xC4;
  uint8_t flags = 0;
  if (state.drumsOn) flags |= 1;
  if (state.bassOn) flags |= 2;
  if (state.monoOn) flags |= 4;
  if (state.legatoOn) flags |= 8;
  if (state.glideOn) flags |= 16;
  if (state.unisonOn) flags |= 32;
  const uint8_t harmByte =
      (uint8_t)((state.harmOn ? 0x80 : 0) | (state.harmSemi & 0x7F));
  const uint8_t perfByte = (uint8_t)((state.seqAccent ? 0x80 : 0) |
                                     (state.swingPct & 0x7F));
  EEPROM.put(addr, header);
  EEPROM.put(addr + 1, flags);
  EEPROM.put(addr + 2, harmByte);
  EEPROM.put(addr + 3, perfByte);
  EEPROM.put(addr + 4, (uint8_t)0);
  return true;
}

bool PresetStore::loadTransportLegacy(BenchTransportState &state) {
  uint8_t header = 0;
  EEPROM.get(kTransportAddr, header);
  if (header != 0xC3 && header != 0xC4) return false;
  uint8_t flags = 0;
  EEPROM.get(kTransportAddr + 1, flags);
  state.drumsOn = (flags & 1) != 0;
  state.bassOn = (flags & 2) != 0;
  state.monoOn = (flags & 4) != 0;
  state.legatoOn = (flags & 8) != 0;
  state.glideOn = (flags & 16) != 0;
  state.unisonOn = (flags & 32) != 0;
  if (header == 0xC4) {
    uint8_t harmByte = 0;
    uint8_t perfByte = 0;
    EEPROM.get(kTransportAddr + 2, harmByte);
    EEPROM.get(kTransportAddr + 3, perfByte);
    state.harmOn = (harmByte & 0x80) != 0;
    state.harmSemi = (uint8_t)constrain((int)(harmByte & 0x7F), 1, 12);
    state.seqAccent = (perfByte & 0x80) != 0;
    state.swingPct = (uint8_t)constrain((int)(perfByte & 0x7F), 0, 55);
  }
  return true;
}

bool PresetStore::loadTransport(int bank, BenchTransportState &state) {
  const int addr = transportAddr(bank);
  if (addr < 0) return false;
  uint8_t header = 0;
  EEPROM.get(addr, header);
  if (header != 0xC3 && header != 0xC4) {
    if (bank == 0) return loadTransportLegacy(state);
    return false;
  }
  uint8_t flags = 0;
  EEPROM.get(addr + 1, flags);
  state.drumsOn = (flags & 1) != 0;
  state.bassOn = (flags & 2) != 0;
  state.monoOn = (flags & 4) != 0;
  state.legatoOn = (flags & 8) != 0;
  state.glideOn = (flags & 16) != 0;
  state.unisonOn = (flags & 32) != 0;
  if (header == 0xC4) {
    uint8_t harmByte = 0;
    uint8_t perfByte = 0;
    EEPROM.get(addr + 2, harmByte);
    EEPROM.get(addr + 3, perfByte);
    state.harmOn = (harmByte & 0x80) != 0;
    state.harmSemi = (uint8_t)constrain((int)(harmByte & 0x7F), 1, 12);
    state.seqAccent = (perfByte & 0x80) != 0;
    state.swingPct = (uint8_t)constrain((int)(perfByte & 0x7F), 0, 55);
  }
  return true;
}

bool PresetStore::saveBassPattern(int bank, const int8_t *degrees, int count) {
  if (!degrees || count <= 0) return false;
  const int addr = bassAddr(bank);
  if (addr < 0) return false;
  const uint8_t header = 0xD4;
  EEPROM.put(addr, header);
  for (int i = 0; i < count; i++) {
    EEPROM.put(addr + 1 + i, (uint8_t)(degrees[i] + 128));
  }
  return true;
}

bool PresetStore::loadBassPatternLegacy(int8_t *degrees, int count) {
  if (!degrees || count <= 0) return false;
  uint8_t header = 0;
  EEPROM.get(kBassPatternAddr, header);
  if (header != 0xD4) return false;
  for (int i = 0; i < count; i++) {
    uint8_t raw = 0;
    EEPROM.get(kBassPatternAddr + 1 + i, raw);
    degrees[i] = (int8_t)raw - 128;
  }
  return true;
}

bool PresetStore::loadBassPattern(int bank, int8_t *degrees, int count) {
  if (!degrees || count <= 0) return false;
  const int addr = bassAddr(bank);
  if (addr < 0) return false;
  uint8_t header = 0;
  EEPROM.get(addr, header);
  if (header != 0xD4) {
    if (bank == 0) return loadBassPatternLegacy(degrees, count);
    return false;
  }
  for (int i = 0; i < count; i++) {
    uint8_t raw = 0;
    EEPROM.get(addr + 1 + i, raw);
    degrees[i] = (int8_t)raw - 128;
  }
  return true;
}

bool PresetStore::saveDrumPattern(int bank, const uint8_t *kick, const uint8_t *hat, int count,
                                  int length) {
  if (!kick || !hat || count <= 0) return false;
  const int addr = drumAddr(bank);
  if (addr < 0) return false;
  const uint8_t header = 0xE5;
  const uint8_t len = (uint8_t)constrain(length, 1, count);
  EEPROM.put(addr, header);
  EEPROM.put(addr + 1, len);
  for (int i = 0; i < count; i++) {
    EEPROM.put(addr + 2 + i, kick[i]);
    EEPROM.put(addr + 2 + count + i, hat[i]);
  }
  return true;
}

bool PresetStore::loadDrumPatternLegacy(uint8_t *kick, uint8_t *hat, int count, int &length) {
  if (!kick || !hat || count <= 0) return false;
  uint8_t header = 0;
  EEPROM.get(kDrumPatternAddr, header);
  if (header != 0xE5) return false;
  uint8_t len = 16;
  EEPROM.get(kDrumPatternAddr + 1, len);
  if (len > 0 && len <= count) {
    length = len;
  }
  for (int i = 0; i < count; i++) {
    uint8_t k = 0;
    uint8_t h = 0;
    EEPROM.get(kDrumPatternAddr + 2 + i, k);
    EEPROM.get(kDrumPatternAddr + 2 + count + i, h);
    kick[i] = k;
    hat[i] = h;
  }
  return true;
}

bool PresetStore::loadDrumPattern(int bank, uint8_t *kick, uint8_t *hat, int count, int &length) {
  if (!kick || !hat || count <= 0) return false;
  const int addr = drumAddr(bank);
  if (addr < 0) return false;
  uint8_t header = 0;
  EEPROM.get(addr, header);
  if (header != 0xE5) {
    if (bank == 0) return loadDrumPatternLegacy(kick, hat, count, length);
    return false;
  }
  uint8_t len = 16;
  EEPROM.get(addr + 1, len);
  if (len > 0 && len <= count) {
    length = len;
  }
  for (int i = 0; i < count; i++) {
    uint8_t k = 0;
    uint8_t h = 0;
    EEPROM.get(addr + 2 + i, k);
    EEPROM.get(addr + 2 + count + i, h);
    kick[i] = k;
    hat[i] = h;
  }
  return true;
}

bool PresetStore::readChordSnapshot(int slot, ChordSnapshot &out) const {
  Data d{};
  if (!readSlot(slot, d)) {
    out.valid = false;
    return false;
  }
  out.valid = true;
  out.root = d.root;
  out.chordType = d.chordType;
  out.inversion = d.inversion;
  out.scale = d.reserved[0];
  out.octaveShift = (int8_t)((int)d.reserved[1] - 128);
  out.demo = (d.flags & 2) != 0;
  out.arp = (d.flags & 1) != 0;
  out.seqOn = (d.flags & 16) != 0;
  out.seqBpm = d.reserved[2] ? d.reserved[2] : 120u;
  return true;
}

bool PresetStore::chordSnapshotsEqual(int a, int b) const {
  ChordSnapshot sa{}, sb{};
  if (!readChordSnapshot(a, sa) || !readChordSnapshot(b, sb)) return false;
  return sa.root == sb.root && sa.chordType == sb.chordType && sa.inversion == sb.inversion &&
         sa.scale == sb.scale && sa.octaveShift == sb.octaveShift && sa.demo == sb.demo &&
         sa.arp == sb.arp && sa.seqOn == sb.seqOn && sa.seqBpm == sb.seqBpm;
}

int PresetStore::chordSlotDiffCount(int a, int b) const {
  if (a < 0 || a >= kSlots || b < 0 || b >= kSlots) return -1;
  int diffs = 0;
  for (int i = 0; i < kBytesPerSlot; i++) {
    uint8_t ba = 0;
    uint8_t bb = 0;
    EEPROM.get(baseAddr(a) + i, ba);
    EEPROM.get(baseAddr(b) + i, bb);
    if (ba != bb) diffs++;
  }
  return diffs;
}

int PresetStore::patternBankDiffCount(int a, int b) const {
  const int addrA = patternBankAddr(a);
  const int addrB = patternBankAddr(b);
  if (addrA < 0 || addrB < 0) return -1;
  int diffs = 0;
  for (int i = 0; i < kPatternBankBytes; i++) {
    uint8_t ba = 0;
    uint8_t bb = 0;
    EEPROM.get(addrA + i, ba);
    EEPROM.get(addrB + i, bb);
    if (ba != bb) diffs++;
  }
  return diffs;
}

bool PresetStore::migrateLegacyBank0() {
  bool did = false;
  uint8_t header = 0;
  EEPROM.get(seqAddr(0), header);
  if (header != 0xB5) {
    SeqPlayer seq;
    seq.begin();
    if (loadSeqPatternLegacy(seq)) {
      saveSeqPattern(0, seq);
      did = true;
    }
  }
  EEPROM.get(transportAddr(0), header);
  if (header != 0xC3 && header != 0xC4) {
    BenchTransportState t;
    if (loadTransportLegacy(t)) {
      saveTransport(0, t);
      did = true;
    }
  }
  EEPROM.get(bassAddr(0), header);
  if (header != 0xD4) {
    int8_t deg[BassSeq::kSteps];
    if (loadBassPatternLegacy(deg, BassSeq::kSteps)) {
      saveBassPattern(0, deg, BassSeq::kSteps);
      did = true;
    }
  }
  EEPROM.get(drumAddr(0), header);
  if (header != 0xE5) {
    uint8_t kick[DrumGrid::kMaxSteps]{};
    uint8_t hat[DrumGrid::kMaxSteps]{};
    int len = DrumGrid::kSteps;
    if (loadDrumPatternLegacy(kick, hat, DrumGrid::kMaxSteps, len)) {
      saveDrumPattern(0, kick, hat, DrumGrid::kMaxSteps, len);
      did = true;
    }
  }
  return did;
}

bool PresetStore::writeChordSnapshot(int slot, const ChordSnapshot &snap) {
  if (slot < 0 || slot >= kSlots) return false;
  Data d{};
  d.magic = kMagic;
  d.root = snap.root;
  d.chordType = snap.chordType;
  d.inversion = snap.inversion;
  d.flags = (uint8_t)((snap.arp ? 1 : 0) | (snap.demo ? 2 : 0) | (snap.seqOn ? 16 : 0));
  d.reserved[0] = snap.scale;
  d.reserved[1] = (uint8_t)((int)snap.octaveShift + 128);
  d.reserved[2] = (uint8_t)constrain((int)snap.seqBpm, 40, 240);
  writeSlot(slot, d);
  return true;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool PresetStore::importPatternBankHex(int bank, const char *hex, int hexLen) {
  const int addr = patternBankAddr(bank);
  if (addr < 0 || !hex) return false;
  if (hexLen < 0) hexLen = (int)strlen(hex);
  const int needed = kPatternBankBytes * 2;
  if (hexLen < needed) return false;
  for (int i = 0; i < kPatternBankBytes; i++) {
    const int h = hexNibble(hex[i * 2]);
    const int l = hexNibble(hex[i * 2 + 1]);
    if (h < 0 || l < 0) return false;
    EEPROM.write(addr + i, (uint8_t)((h << 4) | l));
  }
  return true;
}

void PresetStore::exportSlot(int slot, Stream &out) const {
  out.println(F("--- eeprom export ---"));
  ChordSnapshot snap{};
  readChordSnapshot(slot, snap);
  out.print(F("slot="));
  out.print(slot);
  out.print(F(" valid="));
  out.println(snap.valid ? 1 : 0);
  if (snap.valid) {
    out.print(F("root="));
    out.print(snap.root);
    out.print(F(" type="));
    out.print(snap.chordType);
    out.print(F(" inv="));
    out.print(snap.inversion);
    out.print(F(" scale="));
    out.print(snap.scale);
    out.print(F(" oct="));
    out.println(snap.octaveShift);
    out.print(F("demo="));
    out.print(snap.demo ? 1 : 0);
    out.print(F(" arp="));
    out.print(snap.arp ? 1 : 0);
    out.print(F(" seq="));
    out.print(snap.seqOn ? 1 : 0);
    out.print(F(" bpm="));
    out.println(snap.seqBpm);
  }
  const int addr = patternBankAddr(slot);
  if (addr >= 0) {
    out.print(F("bank_addr="));
    out.println(addr);
    out.print(F("bank_hex="));
    for (int i = 0; i < kPatternBankBytes; i++) {
      uint8_t b = 0;
      EEPROM.get(addr + i, b);
      if (b < 16) out.print('0');
      out.print(b, HEX);
    }
    out.println();
  }
  out.println(F("---"));
}
