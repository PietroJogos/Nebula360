#include "library/Scanner.h"

#include <algorithm>

#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
#include "formats/StfsHeader.h"
#include "formats/XexHeader.h"
#include "library/Category.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

const char* const kTag = "scanner";
const size_t kMaxEntriesPerDirectory = 20000;
const size_t kMaxErrorMessages = 50;

// Comparação para ordenar entradas por nome (ordem determinística).
struct EntryNameLess {
  bool operator()(const DirEntry& a, const DirEntry& b) const {
    return str::toLowerAscii(a.name) < str::toLowerAscii(b.name);
  }
};

bool shouldSkipDirectory(const std::string& name) {
  if (name.empty() || name[0] == '.') return true;
  std::string lower = str::toLowerAscii(name);
  return str::endsWith(lower, ".data") || lower == "$systemupdate" ||
         lower == "$recycle.bin" || lower == "system volume information" ||
         lower == "cache";
}

bool isEightHexDigits(const std::string& name) {
  return name.size() == 8 && str::isHexString(name);
}

// "Meu_Jogo.Homebrew" -> "Meu Jogo Homebrew".
std::string titleFromFolder(const std::string& folderName) {
  std::string t = str::replaceAll(folderName, "_", " ");
  t = str::trim(t);
  return t.empty() ? folderName : t;
}

}  // namespace

Scanner::Scanner(IFileSystem& fs) : fs_(fs), running_(false), cancelled_(false) {}

void Scanner::start(const std::vector<ScanRoot>& roots) {
  roots_ = roots;
  stack_.clear();
  results_.clear();
  accessibleRoots_.clear();
  errors_.clear();
  stats_ = ScanStats();
  cancelled_ = false;
  // Empilha em ordem reversa para processar na ordem configurada.
  for (size_t i = roots_.size(); i > 0; --i) {
    const ScanRoot& r = roots_[i - 1];
    if (!r.enabled || str::trim(r.path).empty()) continue;
    Pending p;
    p.path = path::stripTrailingSeparator(str::trim(r.path), fs_.separator());
    p.depth = 0;
    p.root = i - 1;
    stack_.push_back(p);
  }
  running_ = !stack_.empty();
  NB_INFO(kTag, "varredura iniciada com " << stack_.size() << " pasta(s)");
}

void Scanner::cancel() {
  if (!running_) return;
  cancelled_ = true;
  running_ = false;
  stack_.clear();
  NB_INFO(kTag, "varredura cancelada pelo usuário");
}

bool Scanner::step(uint32_t maxDirectories) {
  uint32_t budget = maxDirectories == 0 ? 1 : maxDirectories;
  while (running_ && budget > 0 && !stack_.empty()) {
    Pending item = stack_[stack_.size() - 1];
    stack_.pop_back();
    visit(item);
    --budget;
  }
  if (running_ && stack_.empty()) {
    running_ = false;
    NB_INFO(kTag, "varredura concluída: " << stats_.gamesFound << " jogo(s), "
                                          << stats_.directoriesVisited << " pasta(s), "
                                          << stats_.errors << " erro(s)");
  }
  return !running_;
}

void Scanner::runToCompletion() {
  while (!step(64)) {
  }
}

void Scanner::recordError(const Status& status) {
  ++stats_.errors;
  NB_WARN(kTag, status.toString());
  if (errors_.size() < kMaxErrorMessages) errors_.push_back(status.toString());
}

std::string Scanner::resolveCategory(const Pending& item,
                                     const std::string& detected) const {
  const std::string& hint = roots_[item.root].category;
  if (!hint.empty() && hint != category::kAuto && category::isValid(hint)) return hint;
  return detected;
}

void Scanner::visit(const Pending& item) {
  currentPath_ = item.path;
  std::vector<DirEntry> entries;
  Status s = fs_.listDirectory(item.path, entries);
  if (!s.ok()) {
    recordError(s);
    return;
  }
  ++stats_.directoriesVisited;
  if (item.depth == 0) accessibleRoots_.push_back(item.path);
  if (entries.size() > kMaxEntriesPerDirectory) {
    recordError(Status(kLimitExceeded, "pasta com entradas demais, ignorando o excedente: " +
                                           item.path));
    entries.resize(kMaxEntriesPerDirectory);
  }
  std::sort(entries.begin(), entries.end(), EntryNameLess());

  const char sep = fs_.separator();
  // 1) Pasta de jogo extraído/homebrew.
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].isDirectory) continue;
    std::string lower = str::toLowerAscii(entries[i].name);
    if (lower == "default.xex") {
      addXexGame(item, path::join(item.path, entries[i].name, sep));
      return;
    }
  }
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].isDirectory && str::toLowerAscii(entries[i].name) == "default.xbe") {
      addXbeGame(item);
      return;
    }
  }

  // 2) Contêineres STFS em pastas de tipo de conteúdo (8 dígitos hex).
  if (isEightHexDigits(path::fileName(item.path, sep))) {
    for (size_t i = 0; i < entries.size(); ++i) {
      const DirEntry& e = entries[i];
      if (e.isDirectory || e.size < kStfsMinimumBytes) continue;
      if (!path::extensionLower(e.name, sep).empty()) continue;
      examineStfsCandidate(item, path::join(item.path, e.name, sep));
    }
  }

  // 3) Subpastas (empilhadas em ordem reversa = visitadas em ordem alfabética).
  if (item.depth >= roots_[item.root].maxDepth) return;
  for (size_t i = entries.size(); i > 0; --i) {
    const DirEntry& e = entries[i - 1];
    if (!e.isDirectory || shouldSkipDirectory(e.name)) continue;
    Pending child;
    child.path = path::join(item.path, e.name, sep);
    child.depth = item.depth + 1;
    child.root = item.root;
    stack_.push_back(child);
  }
}

void Scanner::addXexGame(const Pending& item, const std::string& xexPath) {
  const char sep = fs_.separator();
  ++stats_.filesExamined;
  DetectedGame g;
  g.launchPath = xexPath;
  g.folder = item.path;
  g.root = roots_[item.root].path;
  g.format = gameformat::kXex;
  g.title = titleFromFolder(path::fileName(item.path, sep));

  XexInfo info;
  Status s = readXexInfo(fs_, xexPath, info);
  std::string detected = category::kHomebrew;
  if (s.ok()) {
    g.titleId = info.hasExecutionInfo ? info.titleId : 0;
    g.mediaId = info.hasExecutionInfo ? info.mediaId : 0;
    detected = looksLikeHomebrewTitleId(info) ? category::kHomebrew : category::kXbox360;
  } else {
    // Continua listando: o usuário pode querer tentar iniciar mesmo assim.
    recordError(s);
  }
  g.category = resolveCategory(item, detected);
  results_.push_back(g);
  ++stats_.gamesFound;
}

void Scanner::addXbeGame(const Pending& item) {
  const char sep = fs_.separator();
  DetectedGame g;
  g.launchPath = path::join(item.path, "default.xbe", sep);
  g.folder = item.path;
  g.root = roots_[item.root].path;
  g.format = gameformat::kXbe;
  g.title = titleFromFolder(path::fileName(item.path, sep));
  g.category = resolveCategory(item, category::kXboxClassic);
  results_.push_back(g);
  ++stats_.gamesFound;
}

void Scanner::examineStfsCandidate(const Pending& item, const std::string& filePath) {
  ++stats_.filesExamined;
  std::string magic;
  Status s = fs_.readFileRange(filePath, 0, 4, magic);
  if (!s.ok()) {
    recordError(s);
    return;
  }
  if (!hasStfsMagic(reinterpret_cast<const uint8_t*>(magic.data()), magic.size())) return;

  StfsInfo info;
  s = readStfsInfo(fs_, filePath, info);
  if (!s.ok()) {
    recordError(s);
    return;
  }
  if (!isLaunchableContentType(info.contentType)) return;

  DetectedGame g;
  g.launchPath = filePath;
  g.folder = item.path;
  g.root = roots_[item.root].path;
  g.titleId = info.titleId;
  g.mediaId = info.mediaId;
  g.iconPng = !info.titleThumbnailPng.empty() ? info.titleThumbnailPng : info.thumbnailPng;
  g.title = info.bestName();
  if (g.title.empty()) g.title = str::toHex32(info.titleId);

  std::string detected = category::kXbox360;
  g.format = gameformat::kGod;
  if (info.contentType == kContentArcadeTitle) {
    detected = category::kArcade;
    g.format = gameformat::kXbla;
  } else if (info.contentType == kContentGameDemo) {
    detected = category::kDemo;
  } else if (info.contentType == kContentXboxOriginalGame) {
    detected = category::kXboxClassic;
  } else if (info.contentType == kContentCommunityGame) {
    detected = category::kHomebrew;
  }
  g.category = resolveCategory(item, detected);
  results_.push_back(g);
  ++stats_.gamesFound;
}

}  // namespace nebula
