// Filtros, pesquisa e ordenação da biblioteca.
#pragma once

#include <string>
#include <vector>

namespace nebula {

class Library;
struct Game;

enum SortMode {
  kSortTitle = 0,
  kSortRecentlyAdded,
  kSortLastPlayed,
  kSortMostPlayed,
  kSortModeCount
};

const char* sortModeId(SortMode mode);           // "title", "added"...
bool parseSortMode(const std::string& id, SortMode& out);

struct LibraryQuery {
  std::string category;   // "" = todas
  bool favoritesOnly;
  std::string search;     // termos separados por espaço (E lógico)
  SortMode sort;
  bool includeMissing;
  LibraryQuery()
      : favoritesOnly(false), sort(kSortTitle), includeMissing(false) {}
};

// Retorna índices na biblioteca, já filtrados e ordenados.
std::vector<size_t> runQuery(const Library& library, const LibraryQuery& query);

// Verifica um jogo contra os termos de pesquisa (já normalizados com
// str::foldForSearch). Casa com o título, sem acentos e sem diferenciar
// maiúsculas, ou com o prefixo do Title ID em hexadecimal.
bool matchesSearch(const Game& game, const std::vector<std::string>& foldedTerms);

}  // namespace nebula
