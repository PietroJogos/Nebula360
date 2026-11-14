// SHA-256 (FIPS 180-4), implementação própria e portável (independe de endianness).
#pragma once

#include <string>

#include "core/Types.h"

namespace nebula {

class Sha256 {
 public:
  Sha256();
  void reset();
  void update(const void* data, size_t length);
  void finish(uint8_t digest[32]);
  std::string finishHex();  // 64 caracteres minúsculos

  static std::string hashHex(const std::string& data);

 private:
  void processBlock(const uint8_t* block);

  uint32_t state_[8];
  uint64_t totalBytes_;
  uint8_t buffer_[64];
  size_t bufferUsed_;
};

}  // namespace nebula
