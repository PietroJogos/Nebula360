#include "library/LibraryQuery.h"

#include <algorithm>

#include "core/StringUtil.h"
#include "library/Library.h"

namespace nebula {

const char* sortModeId(SortMode mode) {
  switch (mode) {
    case kSortTitle: return "title";
    case kSortRecentlyAdded: return "added";
    case kSortLastPlayed: return "played";
    case kSortMostPlayed: return "playcount";
    case kSortModeCount: break;
  }
  return "title";
}

bool parseSortMode(const std::string& id, SortMode& out) {
  for (int i = 0; i < kSortModeCount; ++i) {
    if (id == sortModeId(static_cast<SortMode>(i))) {
      out = static_cast<SortMode>(i);
      return true;
    }
  }
  return false;
}

bool matchesSearch(const Game& game, const std::vector<std::string>& terms) {
  if (terms.empty()) return true;
  std::string title = str::foldForSearch(game.title);
  std::string titleId = str::toLowerAscii(str::toHex32(game.titleId));
  for (size_t i = 0; i < terms.size(); ++i) {
    const std::string& t = terms[i];
    if (title.find(t) != std::string::npos) continue;
    if (game.titleId != 0 && t.size() >= 4 && str::startsWith(titleId, t)) continue;
    return false;
  }
  return true;
}

namespace {

struct Entry {
  size_t index;
  std::string key;  // título normalizado
  const Game* game;
};

// Comparador em escopo de namespace (C++03 não aceita tipos locais em templates).
struct EntryLess {
  SortMode mode;
  explicit EntryLess(SortMode m) : mode(m) {}
  bool byTitle(const Entry& a, const Entry& b) const {
    if (a.key != b.key) return a.key < b.key;
    return a.game->id < b.game->id;
  }
  bool operator()(const Entry& a, const Entry& b) const {
    const Game& ga = *a.game;
    const Game& gb = *b.game;
    switch (mode) {
      case kSortRecentlyAdded:
        if (ga.addedAt != gb.addedAt) return ga.addedAt > gb.addedAt;
        break;
      case kSortLastPlayed:
        if (ga.lastPlayedAt != gb.lastPlayedAt) return ga.lastPlayedAt > gb.lastPlayedAt;
        break;
      case kSortMostPlayed:
        if (ga.playCount != gb.playCount) return ga.playCount > gb.playCount;
        break;
      case kSortTitle:
      case kSortModeCount:
        break;
    }
    return byTitle(a, b);
  }
};

}  // namespace

std::vector<size_t> runQuery(const Library& library, const LibraryQuery& q) {
  std::vector<std::string> terms = str::splitWhitespace(str::foldForSearch(q.search));
  std::vector<Entry> entries;
  for (size_t i = 0; i < library.size(); ++i) {
    const Game& g = library.at(i);
    if (g.missing && !q.includeMissing) continue;
    if (q.favoritesOnly && !g.favorite) continue;
    if (!q.category.empty() && g.effectiveCategory() != q.category) continue;
    if (!matchesSearch(g, terms)) continue;
    Entry e;
    e.index = i;
    e.key = str::foldForSearch(g.title);
    e.game = &g;
    entries.push_back(e);
  }
  std::sort(entries.begin(), entries.end(), EntryLess(q.sort));
  std::vector<size_t> out;
  out.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) out.push_back(entries[i].index);
  return out;
}

}  // namespace nebula
