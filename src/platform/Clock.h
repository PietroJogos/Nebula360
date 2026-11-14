// Relógio abstrato: tempo monotônico (animações, repetição de botões) e
// hora de parede (datas da biblioteca).
#pragma once

#include "core/Types.h"

namespace nebula {

class IClock {
 public:
  virtual ~IClock() {}
  virtual uint64_t monotonicMs() = 0;
  virtual int64_t unixTimeSeconds() = 0;
};

// Relógio controlado manualmente (testes e modo headless determinístico).
class ManualClock : public IClock {
 public:
  ManualClock(uint64_t startMs = 0, int64_t unixTime = 1700000000)
      : now_(startMs), unix_(unixTime) {}
  void advance(uint64_t ms) { now_ += ms; }
  void setUnixTime(int64_t t) { unix_ = t; }
  virtual uint64_t monotonicMs() NB_OVERRIDE { return now_; }
  virtual int64_t unixTimeSeconds() NB_OVERRIDE {
    return unix_ + static_cast<int64_t>(now_ / 1000);
  }

 private:
  uint64_t now_;
  int64_t unix_;
};

}  // namespace nebula
