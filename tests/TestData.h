// Geradores de dados sintéticos para os testes (nenhum arquivo proprietário).
#pragma once

#include <string>

#include "core/Types.h"

namespace testdata {

inline void putBe32(std::string& b, size_t off, uint32_t v) {
  if (b.size() < off + 4) b.resize(off + 4, '\0');
  b[off] = static_cast<char>(v >> 24);
  b[off + 1] = static_cast<char>(v >> 16);
  b[off + 2] = static_cast<char>(v >> 8);
  b[off + 3] = static_cast<char>(v);
}

inline void putBe16(std::string& b, size_t off, uint16_t v) {
  if (b.size() < off + 2) b.resize(off + 2, '\0');
  b[off] = static_cast<char>(v >> 8);
  b[off + 1] = static_cast<char>(v);
}

inline void putUtf16Be(std::string& b, size_t off, const std::string& ascii) {
  for (size_t i = 0; i < ascii.size(); ++i) putBe16(b, off + i * 2, static_cast<unsigned char>(ascii[i]));
}

// Cabeçalho XEX2 mínimo e coerente com o formato público.
inline std::string makeXex(bool withExecutionInfo, uint32_t titleId, uint32_t mediaId,
                           const std::string& peName) {
  std::string b(0x400, '\0');
  b[0] = 'X';
  b[1] = 'E';
  b[2] = 'X';
  b[3] = '2';
  putBe32(b, 0x04, 0x00000001);  // TITLE
  putBe32(b, 0x08, 0x400);       // início do PE
  putBe32(b, 0x10, 0x100);       // informações de segurança
  uint32_t count = 0;
  size_t o = 0x18;
  putBe32(b, o, 0x00010100);  // entry point (valor)
  putBe32(b, o + 4, 0x82001000);
  o += 8;
  ++count;
  putBe32(b, o, 0x000003FF);  // file format info (offset)
  putBe32(b, o + 4, 0x300);
  o += 8;
  ++count;
  putBe32(b, 0x300, 8);
  putBe16(b, 0x304, 1);  // AES
  putBe16(b, 0x306, 1);  // básica
  putBe32(b, o, 0x000183FF);  // nome PE (offset)
  putBe32(b, o + 4, 0x320);
  o += 8;
  ++count;
  putBe32(b, 0x320, static_cast<uint32_t>(4 + peName.size() + 1));
  for (size_t i = 0; i < peName.size(); ++i) b[0x324 + i] = peName[i];
  if (withExecutionInfo) {
    putBe32(b, o, 0x00040006);
    putBe32(b, o + 4, 0x340);
    o += 8;
    ++count;
    putBe32(b, 0x340, mediaId);
    putBe32(b, 0x344, 0x00000001);
    putBe32(b, 0x348, 0);
    putBe32(b, 0x34C, titleId);
    b[0x352] = 1;
    b[0x353] = 1;
  }
  putBe32(b, 0x14, count);
  putBe32(b, 0x100 + 0x178, 0xFFFFFFFF);  // região
  return b;
}

// Cabeçalho STFS ("LIVE") com metadados e miniatura opcional.
inline std::string makeStfs(uint32_t contentType, uint32_t titleId, const std::string& name,
                            const std::string& png) {
  std::string b(0x971A, '\0');
  b[0] = 'L';
  b[1] = 'I';
  b[2] = 'V';
  b[3] = 'E';
  putBe32(b, 0x344, contentType);
  putBe32(b, 0x348, 2);  // metadados v2
  putBe32(b, 0x354, 0xABCD0001);
  putBe32(b, 0x360, titleId);
  b[0x366] = 1;
  b[0x367] = 1;
  putUtf16Be(b, 0x411, name);
  putUtf16Be(b, 0x1691, name);
  putUtf16Be(b, 0x1611, "Estudio Exemplo");
  if (!png.empty() && png.size() <= 0x3D00) {
    putBe32(b, 0x1712, static_cast<uint32_t>(png.size()));
    putBe32(b, 0x1716, static_cast<uint32_t>(png.size()));
    b.replace(0x171A, png.size(), png);
    b.replace(0x571A, png.size(), png);
  }
  return b;
}

}  // namespace testdata
