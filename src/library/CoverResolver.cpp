#include "library/CoverResolver.h"

#include "core/Path.h"
#include "core/StringUtil.h"
#include "library/Game.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {
const char* const kImageExtensions[] = {"png", "jpg", "jpeg"};
const char* const kFolderCoverNames[] = {"cover", "boxart", "folder"};
}  // namespace

CoverResolver::CoverResolver(IFileSystem& fs, const std::string& dataDir)
    : fs_(fs), dataDir_(dataDir) {}

std::string CoverResolver::coversDir() const {
  return path::join(dataDir_, "covers", fs_.separator());
}

std::string CoverResolver::iconCacheDir() const {
  return path::join(path::join(dataDir_, "cache", fs_.separator()), "icons",
                    fs_.separator());
}

std::string CoverResolver::firstExisting(const std::string& dir,
                                         const std::string& baseName) {
  if (dir.empty()) return "";
  for (size_t i = 0; i < sizeof(kImageExtensions) / sizeof(kImageExtensions[0]); ++i) {
    std::string candidate =
        path::join(dir, baseName + "." + kImageExtensions[i], fs_.separator());
    if (fs_.exists(candidate) && !fs_.isDirectory(candidate)) return candidate;
  }
  return "";
}

std::string CoverResolver::resolve(const Game& game) {
  std::string found;
  if (game.titleId != 0) {
    found = firstExisting(coversDir(), str::toHex32(game.titleId));
    if (!found.empty()) return found;
  }
  found = firstExisting(coversDir(), game.id);
  if (!found.empty()) return found;
  for (size_t i = 0; i < sizeof(kFolderCoverNames) / sizeof(kFolderCoverNames[0]); ++i) {
    // Só para pastas de jogo (XEX/XBE): pastas de GOD são compartilhadas.
    if (game.format != "xex" && game.format != "xbe") break;
    found = firstExisting(game.folder, kFolderCoverNames[i]);
    if (!found.empty()) return found;
  }
  std::string icon = path::join(iconCacheDir(), game.id + ".png", fs_.separator());
  if (fs_.exists(icon)) return icon;
  return "";
}

Status CoverResolver::storeIcon(const std::string& gameId, const std::string& png,
                                std::string& outPath) {
  outPath = path::join(iconCacheDir(), gameId + ".png", fs_.separator());
  if (png.empty()) return Status(kInvalidArgument, "miniatura vazia");
  if (fs_.exists(outPath)) return Status::Ok();
  Status s = fs_.createDirectories(iconCacheDir());
  if (!s.ok()) return s;
  return fs_.writeFileAtomic(outPath, png);
}

}  // namespace nebula
