#include "library/Library.h"

#include "core/Log.h"
#include "core/Path.h"
#include "core/Sha256.h"
#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/Scanner.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {
const char* const kTag = "library";
const uint64_t kMaxLibraryBytes = 32 * 1024 * 1024;
}  // namespace

std::string makeGameId(const std::string& launchPath) {
  return Sha256::hashHex(str::toLowerAscii(launchPath)).substr(0, 16);
}

Library::Library() : revision_(0) {}

const Game* Library::find(const std::string& id) const {
  std::map<std::string, size_t>::const_iterator it = index_.find(id);
  return it == index_.end() ? NULL : &games_[it->second];
}

Game* Library::findMutable(const std::string& id) {
  std::map<std::string, size_t>::const_iterator it = index_.find(id);
  return it == index_.end() ? NULL : &games_[it->second];
}

void Library::reindex() {
  index_.clear();
  for (size_t i = 0; i < games_.size(); ++i) index_[games_[i].id] = i;
}

MergeStats Library::mergeScan(const std::vector<DetectedGame>& found,
                              const std::vector<std::string>& scannedRoots,
                              int64_t now, char sep) {
  MergeStats stats;
  std::map<std::string, bool> seen;
  for (size_t i = 0; i < found.size(); ++i) {
    const DetectedGame& d = found[i];
    std::string id = makeGameId(d.launchPath);
    seen[id] = true;
    Game* g = findMutable(id);
    if (g == NULL) {
      Game n;
      n.id = id;
      n.title = d.title;
      n.path = d.launchPath;
      n.folder = d.folder;
      n.root = d.root;
      n.format = d.format;
      n.category = d.category;
      n.titleId = d.titleId;
      n.mediaId = d.mediaId;
      n.addedAt = now;
      games_.push_back(n);
      index_[id] = games_.size() - 1;
      ++stats.added;
    } else {
      if (g->missing) ++stats.restored;
      g->missing = false;
      g->title = d.title;
      g->path = d.launchPath;
      g->folder = d.folder;
      g->root = d.root;
      g->format = d.format;
      g->category = d.category;
      g->titleId = d.titleId;
      g->mediaId = d.mediaId;
      ++stats.updated;
    }
  }
  for (size_t i = 0; i < games_.size(); ++i) {
    Game& g = games_[i];
    if (g.missing || seen.find(g.id) != seen.end()) continue;
    for (size_t r = 0; r < scannedRoots.size(); ++r) {
      if (path::isWithin(g.path, scannedRoots[r], sep)) {
        g.missing = true;
        ++stats.markedMissing;
        break;
      }
    }
  }
  touch();
  NB_INFO(kTag, "mesclagem: " << stats.added << " novo(s), " << stats.updated
                              << " atualizado(s), " << stats.markedMissing
                              << " ausente(s), " << stats.restored << " restaurado(s)");
  return stats;
}

bool Library::setFavorite(const std::string& id, bool favorite) {
  Game* g = findMutable(id);
  if (g == NULL) return false;
  if (g->favorite != favorite) {
    g->favorite = favorite;
    touch();
  }
  return true;
}

bool Library::toggleFavorite(const std::string& id) {
  Game* g = findMutable(id);
  if (g == NULL) return false;
  g->favorite = !g->favorite;
  touch();
  return true;
}

bool Library::setCategoryOverride(const std::string& id, const std::string& cat) {
  Game* g = findMutable(id);
  if (g == NULL) return false;
  if (!cat.empty() && !category::isValid(cat)) return false;
  g->categoryOverride = (cat == g->category) ? std::string() : cat;
  touch();
  return true;
}

bool Library::setCoverPath(const std::string& id, const std::string& coverPath) {
  Game* g = findMutable(id);
  if (g == NULL) return false;
  if (g->coverPath != coverPath) {
    g->coverPath = coverPath;
    touch();
  }
  return true;
}

bool Library::recordLaunch(const std::string& id, int64_t now) {
  Game* g = findMutable(id);
  if (g == NULL) return false;
  g->lastPlayedAt = now;
  ++g->playCount;
  touch();
  return true;
}

size_t Library::removeMissing() {
  std::vector<Game> kept;
  for (size_t i = 0; i < games_.size(); ++i) {
    if (!games_[i].missing) kept.push_back(games_[i]);
  }
  size_t removed = games_.size() - kept.size();
  if (removed > 0) {
    games_.swap(kept);
    reindex();
    touch();
  }
  return removed;
}

void Library::clear() {
  games_.clear();
  index_.clear();
  touch();
}

JsonValue Library::toJson() const {
  JsonValue root = JsonValue::object();
  root.set("schema", kSchemaVersion);
  JsonValue list = JsonValue::array();
  for (size_t i = 0; i < games_.size(); ++i) {
    const Game& g = games_[i];
    JsonValue o = JsonValue::object();
    o.set("id", g.id);
    o.set("title", g.title);
    o.set("path", g.path);
    o.set("folder", g.folder);
    o.set("root", g.root);
    o.set("format", g.format);
    o.set("category", g.category);
    if (!g.categoryOverride.empty()) o.set("categoryOverride", g.categoryOverride);
    if (!g.coverPath.empty()) o.set("cover", g.coverPath);
    o.set("titleId", str::toHex32(g.titleId));
    o.set("mediaId", str::toHex32(g.mediaId));
    o.set("favorite", g.favorite);
    if (g.missing) o.set("missing", true);
    o.set("addedAt", g.addedAt);
    o.set("lastPlayedAt", g.lastPlayedAt);
    o.set("playCount", g.playCount);
    list.push(o);
  }
  root.set("games", list);
  return root;
}

Status Library::fromJson(const JsonValue& json, size_t& skipped) {
  skipped = 0;
  if (!json.isObject()) return Status(kParseError, "biblioteca: raiz não é objeto");
  int64_t schema = json.get("schema").asInt(-1);
  if (schema != kSchemaVersion) {
    return Status(kUnsupported, "biblioteca: versão de esquema não suportada (" +
                                    str::toString(schema) + ")");
  }
  const JsonValue& list = json.get("games");
  if (!list.isArray()) return Status(kParseError, "biblioteca: 'games' não é lista");
  std::vector<Game> games;
  std::map<std::string, bool> ids;
  for (size_t i = 0; i < list.size(); ++i) {
    const JsonValue& o = list.at(i);
    Game g;
    g.id = o.get("id").asString();
    g.path = o.get("path").asString();
    g.title = o.get("title").asString();
    if (g.id.empty() || g.path.empty() || ids.find(g.id) != ids.end()) {
      ++skipped;
      continue;
    }
    if (g.title.empty()) g.title = g.path;
    g.folder = o.get("folder").asString();
    g.root = o.get("root").asString();
    g.format = o.get("format").asString(gameformat::kXex);
    g.category = o.get("category").asString(category::kHomebrew);
    if (!category::isValid(g.category)) g.category = category::kHomebrew;
    g.categoryOverride = o.get("categoryOverride").asString();
    if (!g.categoryOverride.empty() && !category::isValid(g.categoryOverride)) {
      g.categoryOverride.clear();
    }
    g.coverPath = o.get("cover").asString();
    str::parseHex32(o.get("titleId").asString(), g.titleId);
    str::parseHex32(o.get("mediaId").asString(), g.mediaId);
    g.favorite = o.get("favorite").asBool(false);
    g.missing = o.get("missing").asBool(false);
    g.addedAt = o.get("addedAt").asInt(0);
    g.lastPlayedAt = o.get("lastPlayedAt").asInt(0);
    int64_t plays = o.get("playCount").asInt(0);
    g.playCount = plays < 0 ? 0 : static_cast<uint32_t>(plays);
    ids[g.id] = true;
    games.push_back(g);
  }
  games_.swap(games);
  reindex();
  touch();
  return Status::Ok();
}

Status loadLibrary(IFileSystem& fs, const std::string& filePath, Library& library) {
  library.clear();
  if (!fs.exists(filePath)) {
    NB_INFO(kTag, "biblioteca ainda não existe: " << filePath);
    return Status::Ok();
  }
  std::string text;
  Status s = fs.readFile(filePath, text, kMaxLibraryBytes);
  if (!s.ok()) return s.withContext("ler biblioteca");
  JsonValue json;
  s = parseJson(text, json);
  size_t skipped = 0;
  if (s.ok()) s = library.fromJson(json, skipped);
  if (!s.ok()) {
    std::string backup = filePath + ".corrompido";
    if (fs.exists(backup)) fs.removeFile(backup);
    Status r = fs.renameFile(filePath, backup);
    NB_ERROR(kTag, "biblioteca inválida (" << s.toString() << "); cópia salva em "
                                           << backup << (r.ok() ? "" : " (falhou)"));
    library.clear();
    return Status(kParseError, s.message());
  }
  if (skipped > 0) NB_WARN(kTag, skipped << " entrada(s) inválida(s) ignorada(s)");
  NB_INFO(kTag, "biblioteca carregada: " << library.size() << " jogo(s)");
  return Status::Ok();
}

Status saveLibrary(IFileSystem& fs, const std::string& filePath,
                   const Library& library) {
  Status s = fs.writeFileAtomic(filePath, library.toJson().serialize(true));
  if (!s.ok()) {
    NB_ERROR(kTag, "falha ao salvar biblioteca: " << s.toString());
    return s;
  }
  return Status::Ok();
}

}  // namespace nebula
