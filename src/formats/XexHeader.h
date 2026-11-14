// Leitura dos campos em texto claro do cabeçalho XEX2.
//
// Baseado na documentação pública do formato (Free60 / Xenia). NÃO
// descriptografa a imagem: o nome do jogo não está no cabeçalho, só o
// Title ID / Media ID (cabeçalho opcional EXECUTION_INFO) e metadados.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class IFileSystem;

struct XexInfo {
  uint32_t moduleFlags;
  uint32_t peDataOffset;
  uint32_t securityInfoOffset;
  uint32_t optionalHeaderCount;

  bool hasExecutionInfo;
  uint32_t mediaId;
  uint32_t version;
  uint32_t baseVersion;
  uint32_t titleId;
  uint8_t discNumber;
  uint8_t discCount;

  bool hasEntryPoint;
  uint32_t entryPoint;
  uint32_t imageBase;
  bool hasFileFormat;
  uint16_t encryptionType;   // 0 = nenhuma, 1 = AES
  uint16_t compressionType;  // 0 nenhuma, 1 básica, 2 LZX, 3 delta
  uint32_t systemFlags;
  bool hasGameRegion;
  uint32_t gameRegion;
  std::string originalPeName;
  std::vector<std::string> resourceNames;

  XexInfo();
  bool isTitle() const { return (moduleFlags & 0x01) != 0; }
  bool isDll() const { return (moduleFlags & 0x08) != 0; }
  bool isPatch() const { return (moduleFlags & 0x70) != 0; }
};

// Cabeçalhos maiores que isso são rejeitados (os reais têm ~5-20 KiB).
const size_t kXexMaxHeaderBytes = 1024 * 1024;

Status parseXexHeader(const uint8_t* data, size_t size, XexInfo& out);
Status readXexInfo(IFileSystem& fs, const std::string& path, XexInfo& out);

// Title ID padrão de homebrew/aplicativos sem EXECUTION_INFO ou com ID zero.
bool looksLikeHomebrewTitleId(const XexInfo& info);

}  // namespace nebula
