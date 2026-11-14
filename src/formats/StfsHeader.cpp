#include "formats/StfsHeader.h"

#include "core/StringUtil.h"
#include "formats/ByteReader.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

// Offsets do bloco de metadados (Free60 / STFS).
const size_t kOffContentType = 0x344;
const size_t kOffMetadataVersion = 0x348;
const size_t kOffContentSize = 0x34C;
const size_t kOffMediaId = 0x354;
const size_t kOffVersion = 0x358;
const size_t kOffBaseVersion = 0x35C;
const size_t kOffTitleId = 0x360;
const size_t kOffDiscNumber = 0x366;
const size_t kOffDiscInSet = 0x367;
const size_t kOffDataFileCount = 0x39D;
const size_t kOffDescriptorType = 0x3A9;
const size_t kOffDisplayName = 0x411;      // 18 idiomas x 0x80
const size_t kOffDescription = 0xD11;      // 18 idiomas x 0x80
const size_t kOffPublisherName = 0x1611;   // 0x80
const size_t kOffTitleName = 0x1691;       // 0x80
const size_t kOffThumbnailSize = 0x1712;
const size_t kOffTitleThumbnailSize = 0x1716;
const size_t kOffThumbnail = 0x171A;
// v1: miniatura 0x4000 em 0x171A; v2: miniatura 0x3D00 em 0x171A seguida de
// nomes adicionais (0x300) em 0x541A. Nos dois casos a miniatura do título
// começa em 0x571A.
const size_t kOffTitleThumbnail = 0x571A;
const size_t kThumbnailMaxV1 = 0x4000;
const size_t kThumbnailMaxV2 = 0x3D00;
const size_t kLocaleStringBytes = 0x80;

std::string readUtf16(const ByteReader& r, size_t offset, size_t bytes) {
  if (!r.has(offset, bytes)) return "";
  return str::trim(str::utf16beToUtf8(r.data() + offset, bytes));
}

bool isPng(const uint8_t* p, size_t n) {
  static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (n < 8) return false;
  for (int i = 0; i < 8; ++i) {
    if (p[i] != kSig[i]) return false;
  }
  return true;
}

std::string readImage(const ByteReader& r, size_t offset, uint32_t size,
                      size_t maxSize) {
  if (size == 0 || size > maxSize || !r.has(offset, size)) return "";
  if (!isPng(r.data() + offset, size)) return "";
  return std::string(reinterpret_cast<const char*>(r.data() + offset), size);
}

}  // namespace

StfsInfo::StfsInfo()
    : contentType(0), metadataVersion(0), contentSize(0), mediaId(0),
      version(0), baseVersion(0), titleId(0), discNumber(0), discCount(0),
      descriptorType(0), dataFileCount(0) {}

std::string StfsInfo::bestName() const {
  if (!displayName.empty()) return displayName;
  if (!titleName.empty()) return titleName;
  return "";
}

bool hasStfsMagic(const uint8_t* d, size_t size) {
  if (size < 4) return false;
  return (d[0] == 'C' && d[1] == 'O' && d[2] == 'N' && d[3] == ' ') ||
         (d[0] == 'L' && d[1] == 'I' && d[2] == 'V' && d[3] == 'E') ||
         (d[0] == 'P' && d[1] == 'I' && d[2] == 'R' && d[3] == 'S');
}

Status parseStfsHeader(const uint8_t* data, size_t size, StfsInfo& out) {
  out = StfsInfo();
  if (!hasStfsMagic(data, size)) return Status(kParseError, "assinatura STFS ausente");
  if (size < kStfsMinimumBytes) return Status(kParseError, "cabeçalho STFS truncado");
  ByteReader r(data, size);
  out.magic = std::string(reinterpret_cast<const char*>(data), 4);
  r.be32(kOffContentType, out.contentType);
  r.be32(kOffMetadataVersion, out.metadataVersion);
  r.be64(kOffContentSize, out.contentSize);
  r.be32(kOffMediaId, out.mediaId);
  r.be32(kOffVersion, out.version);
  r.be32(kOffBaseVersion, out.baseVersion);
  r.be32(kOffTitleId, out.titleId);
  r.u8(kOffDiscNumber, out.discNumber);
  r.u8(kOffDiscInSet, out.discCount);
  r.be32(kOffDataFileCount, out.dataFileCount);
  r.be32(kOffDescriptorType, out.descriptorType);
  out.displayName = readUtf16(r, kOffDisplayName, kLocaleStringBytes);
  out.description = readUtf16(r, kOffDescription, kLocaleStringBytes);
  out.publisherName = readUtf16(r, kOffPublisherName, kLocaleStringBytes);
  out.titleName = readUtf16(r, kOffTitleName, kLocaleStringBytes);

  uint32_t thumbSize = 0;
  uint32_t titleThumbSize = 0;
  if (r.be32(kOffThumbnailSize, thumbSize) &&
      r.be32(kOffTitleThumbnailSize, titleThumbSize)) {
    bool v2 = out.metadataVersion >= 2;
    size_t maxThumb = v2 ? kThumbnailMaxV2 : kThumbnailMaxV1;
    out.thumbnailPng = readImage(r, kOffThumbnail, thumbSize, maxThumb);
    out.titleThumbnailPng =
        readImage(r, kOffTitleThumbnail, titleThumbSize, maxThumb);
  }
  return Status::Ok();
}

Status readStfsInfo(IFileSystem& fs, const std::string& path, StfsInfo& out) {
  std::string buf;
  Status s = fs.readFileRange(path, 0, kStfsHeaderReadBytes, buf);
  if (!s.ok()) return s;
  return parseStfsHeader(reinterpret_cast<const uint8_t*>(buf.data()),
                         buf.size(), out)
      .withContext(path);
}

const char* stfsContentTypeName(uint32_t t) {
  switch (t) {
    case kContentSavedGame: return "SavedGame";
    case kContentMarketplace: return "MarketplaceContent";
    case kContentInstalledGame: return "InstalledGame";
    case kContentXboxOriginalGame: return "XboxOriginalGame";
    case kContentGamesOnDemand: return "GamesOnDemand";
    case kContentGameDemo: return "GameDemo";
    case kContentInstaller: return "Installer";
    case kContentArcadeTitle: return "ArcadeTitle";
    case kContentCommunityGame: return "CommunityGame";
  }
  return "Unknown";
}

bool isLaunchableContentType(uint32_t t) {
  return t == kContentGamesOnDemand || t == kContentArcadeTitle ||
         t == kContentGameDemo || t == kContentXboxOriginalGame ||
         t == kContentCommunityGame;
}

}  // namespace nebula
