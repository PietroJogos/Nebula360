#include "integrations/Integration.h"

#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
#include "integrations/LibraryExport.h"
#include "library/Library.h"
#include "platform/Clock.h"
#include "platform/FileSystem.h"

namespace nebula {

IntegrationRegistry::~IntegrationRegistry() {
  for (size_t i = 0; i < items_.size(); ++i) delete items_[i];
}

void IntegrationRegistry::add(IIntegration* integration) {
  if (integration != NULL) items_.push_back(integration);
}

size_t IntegrationRegistry::notifyLibrarySaved(const Library& library) {
  size_t ok = 0;
  for (size_t i = 0; i < items_.size(); ++i) {
    Status s = items_[i]->onLibrarySaved(library);
    if (s.ok()) {
      ++ok;
    } else {
      NB_WARN("integration", items_[i]->id() << ": " << s.toString());
    }
  }
  return ok;
}

JsonValue buildLibraryExport(const Library& library, int64_t generatedAt) {
  JsonValue root = JsonValue::object();
  root.set("format", "nebula360.library");
  root.set("version", 1);
  root.set("generatedAt", generatedAt);
  JsonValue games = JsonValue::array();
  for (size_t i = 0; i < library.size(); ++i) {
    const Game& g = library.at(i);
    if (g.missing) continue;
    JsonValue o = JsonValue::object();
    o.set("id", g.id);
    o.set("title", g.title);
    o.set("titleId", str::toHex32(g.titleId));
    o.set("category", g.effectiveCategory());
    o.set("format", g.format);
    o.set("path", g.path);
    o.set("cover", g.coverPath);
    o.set("favorite", g.favorite);
    o.set("lastPlayedAt", g.lastPlayedAt);
    o.set("playCount", g.playCount);
    games.push(o);
  }
  root.set("games", games);
  return root;
}

JsonExportIntegration::JsonExportIntegration(IFileSystem& fs, const std::string& exportPath,
                                             IClock& clock)
    : fs_(fs), path_(exportPath), clock_(clock) {}

Status JsonExportIntegration::onLibrarySaved(const Library& library) {
  std::string dir = path::parent(path_, fs_.separator());
  if (!dir.empty()) {
    Status s = fs_.createDirectories(dir);
    if (!s.ok()) return s;
  }
  JsonValue json = buildLibraryExport(library, clock_.unixTimeSeconds());
  return fs_.writeFileAtomic(path_, json.serialize(true));
}

}  // namespace nebula
