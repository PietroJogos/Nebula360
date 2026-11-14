#include "formats/XexHeader.h"

#include "core/StringUtil.h"
#include "formats/ByteReader.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

const uint32_t kKeyResourceInfo = 0x000002FF;
const uint32_t kKeyFileFormatInfo = 0x000003FF;
const uint32_t kKeyEntryPoint = 0x00010100;
const uint32_t kKeyImageBaseAddress = 0x00010201;
const uint32_t kKeyOriginalPeName = 0x000183FF;
const uint32_t kKeySystemFlags = 0x00030000;
const uint32_t kKeyExecutionInfo = 0x00040006;
const uint32_t kKeyGameRatings = 0x00040310;  // não usado, só documentação
const uint32_t kSecurityRegionOffset = 0x178;

}  // namespace

XexInfo::XexInfo()
    : moduleFlags(0), peDataOffset(0), securityInfoOffset(0),
      optionalHeaderCount(0), hasExecutionInfo(false), mediaId(0), version(0),
      baseVersion(0), titleId(0), discNumber(0), discCount(0),
      hasEntryPoint(false), entryPoint(0), imageBase(0), hasFileFormat(false),
      encryptionType(0), compressionType(0), systemFlags(0),
      hasGameRegion(false), gameRegion(0) {}

Status parseXexHeader(const uint8_t* data, size_t size, XexInfo& out) {
  (void)kKeyGameRatings;
  out = XexInfo();
  ByteReader r(data, size);
  if (!r.has(0, 0x18)) return Status(kParseError, "arquivo pequeno demais para XEX");
  if (!(data[0] == 'X' && data[1] == 'E' && data[2] == 'X')) {
    return Status(kParseError, "assinatura XEX ausente");
  }
  if (data[3] != '2') {
    return Status(kUnsupported, std::string("variante XEX não suportada: XEX") +
                                    static_cast<char>(data[3]));
  }
  r.be32(0x04, out.moduleFlags);
  r.be32(0x08, out.peDataOffset);
  r.be32(0x10, out.securityInfoOffset);
  r.be32(0x14, out.optionalHeaderCount);
  if (out.optionalHeaderCount > 256) {
    return Status(kParseError, "quantidade de cabeçalhos opcionais implausível");
  }
  if (!r.has(0x18, static_cast<size_t>(out.optionalHeaderCount) * 8)) {
    return Status(kParseError, "tabela de cabeçalhos opcionais truncada");
  }

  for (uint32_t i = 0; i < out.optionalHeaderCount; ++i) {
    size_t o = 0x18 + static_cast<size_t>(i) * 8;
    uint32_t key = 0;
    uint32_t value = 0;
    r.be32(o, key);
    r.be32(o + 4, value);

    // Byte baixo da chave: 0/1 = valor imediato; demais = offset no cabeçalho.
    // Offsets fora do buffer são ignorados ANTES de qualquer soma, porque no
    // Xbox 360 size_t tem 32 bits e "value + n" poderia dar a volta.
    bool isOffset = (key & 0xFF) > 1;
    if (isOffset && value >= size) continue;

    if (key == kKeyEntryPoint) {
      out.hasEntryPoint = true;
      out.entryPoint = value;
    } else if (key == kKeyImageBaseAddress) {
      out.imageBase = value;
    } else if (key == kKeySystemFlags) {
      out.systemFlags = value;
    } else if (key == kKeyExecutionInfo) {
      // value = offset de uma estrutura de 24 bytes.
      uint32_t mediaId = 0, ver = 0, baseVer = 0, titleId = 0;
      uint8_t disc = 0, discs = 0;
      if (r.be32(value, mediaId) && r.be32(value + 4, ver) &&
          r.be32(value + 8, baseVer) && r.be32(value + 12, titleId) &&
          r.u8(value + 18, disc) && r.u8(value + 19, discs)) {
        out.hasExecutionInfo = true;
        out.mediaId = mediaId;
        out.version = ver;
        out.baseVersion = baseVer;
        out.titleId = titleId;
        out.discNumber = disc;
        out.discCount = discs;
      }
    } else if (key == kKeyFileFormatInfo) {
      uint16_t enc = 0, comp = 0;
      if (r.be16(value + 4, enc) && r.be16(value + 6, comp)) {
        out.hasFileFormat = true;
        out.encryptionType = enc;
        out.compressionType = comp;
      }
    } else if (key == kKeyOriginalPeName) {
      uint32_t len = 0;
      if (r.be32(value, len) && len >= 4 && len <= 260 && r.has(value + 4, len - 4)) {
        std::string name;
        for (size_t k = 0; k < len - 4; ++k) {
          char c = static_cast<char>(data[value + 4 + k]);
          if (c == '\0') break;
          name += c;
        }
        out.originalPeName = name;
      }
    } else if (key == kKeyResourceInfo) {
      uint32_t len = 0;
      if (r.be32(value, len) && len >= 4 && len <= 64 * 1024 && r.has(value, len)) {
        for (size_t p = value + 4; p + 16 <= static_cast<size_t>(value) + len; p += 16) {
          std::string name;
          for (size_t k = 0; k < 8 && data[p + k] != 0; ++k) {
            name += static_cast<char>(data[p + k]);
          }
          out.resourceNames.push_back(name);
        }
      }
    }
  }

  uint32_t region = 0;
  if (out.securityInfoOffset != 0 && out.securityInfoOffset < size &&
      r.be32(out.securityInfoOffset + kSecurityRegionOffset, region)) {
    out.hasGameRegion = true;
    out.gameRegion = region;
  }
  return Status::Ok();
}

Status readXexInfo(IFileSystem& fs, const std::string& path, XexInfo& out) {
  std::string head;
  Status s = fs.readFileRange(path, 0, 0x18, head);
  if (!s.ok()) return s;
  if (head.size() < 0x18) return Status(kParseError, "XEX truncado: " + path);
  ByteReader r(reinterpret_cast<const uint8_t*>(head.data()), head.size());
  uint32_t headerSize = 0;
  r.be32(0x08, headerSize);
  if (headerSize < 0x18 || headerSize > kXexMaxHeaderBytes) {
    // Pode não ser XEX2; o parser devolve o erro adequado com os 24 bytes.
    headerSize = 0x18;
  }
  std::string full;
  s = fs.readFileRange(path, 0, headerSize, full);
  if (!s.ok()) return s;
  s = parseXexHeader(reinterpret_cast<const uint8_t*>(full.data()), full.size(), out);
  return s.withContext(path);
}

bool looksLikeHomebrewTitleId(const XexInfo& info) {
  if (!info.hasExecutionInfo) return true;
  if (info.titleId == 0) return true;
  // Faixa 0xFFFF____ é usada por ferramentas/homebrew comuns.
  if ((info.titleId >> 16) == 0xFFFF) return true;
  return false;
}

}  // namespace nebula
