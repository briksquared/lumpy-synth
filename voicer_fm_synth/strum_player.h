#pragma once

#include <Arduino.h>

// Non-blocking chord strum — staggers note-ons across poll() calls.
class StrumPlayer {
 public:
  using NoteOnFn = void (*)(int note, void *ctx);

  void begin(NoteOnFn fn, void *ctx, unsigned stepMs = 28) {
    noteOnFn_ = fn;
    ctx_ = ctx;
    stepMs_ = stepMs;
  }

  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  void toggle() { enabled_ = !enabled_; }
  bool active() const { return active_; }

  void start(const int *notes, int count) {
    if (!noteOnFn_ || count <= 0) return;
    count_ = count > 6 ? 6 : count;
    for (int i = 0; i < count_; i++) notes_[i] = notes[i];

    if (!enabled_) {
      for (int i = 0; i < count_; i++) noteOnFn_(notes_[i], ctx_);
      active_ = false;
      return;
    }

    active_ = true;
    index_ = 1;
    nextMs_ = millis() + stepMs_;
    noteOnFn_(notes_[0], ctx_);
  }

  void poll() {
    if (!active_ || !noteOnFn_) return;
    if ((long)(millis() - nextMs_) < 0) return;
    if (index_ < count_) {
      noteOnFn_(notes_[index_], ctx_);
      index_++;
      nextMs_ = millis() + stepMs_;
    } else {
      active_ = false;
    }
  }

 private:
  NoteOnFn noteOnFn_ = nullptr;
  void *ctx_ = nullptr;
  unsigned stepMs_ = 28;
  bool enabled_ = true;
  bool active_ = false;
  int notes_[6]{};
  int count_ = 0;
  int index_ = 0;
  unsigned long nextMs_ = 0;
};
