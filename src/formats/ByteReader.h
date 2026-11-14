// Leitura big-endian com verificação de limites (Xbox 360 é big-endian;
// ler byte a byte deixa o código correto em qualquer CPU).
#pragma once

#include "core/Types.h"

namespace nebula {

class ByteReader {
 public:
  ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  size_t size() const { return size_; }
  const uint8_t* data() const { return data_; }
  bool has(size_t offset, size_t length) const {
    return offset <= size_ && length <= size_ - offset;
  }

  bool u8(size_t offset, uint8_t& out) const {
    if (!has(offset, 1)) return false;
    out = data_[offset];
    return true;
  }
  bool be16(size_t offset, uint16_t& out) const {
    if (!has(offset, 2)) return false;
    out = static_cast<uint16_t>((data_[offset] << 8) | data_[offset + 1]);
    return true;
  }
  bool be32(size_t offset, uint32_t& out) const {
    if (!has(offset, 4)) return false;
    out = (static_cast<uint32_t>(data_[offset]) << 24) |
          (static_cast<uint32_t>(data_[offset + 1]) << 16) |
          (static_cast<uint32_t>(data_[offset + 2]) << 8) |
          static_cast<uint32_t>(data_[offset + 3]);
    return true;
  }
  bool be64(size_t offset, uint64_t& out) const {
    uint32_t hi = 0;
    uint32_t lo = 0;
    if (!be32(offset, hi) || !be32(offset + 4, lo)) return false;
    out = (static_cast<uint64_t>(hi) << 32) | lo;
    return true;
  }

 private:
  const uint8_t* data_;
  size_t size_;
};

}  // namespace nebula
