// Biblioteca local de jogos: mesclagem de varreduras, favoritos, histórico
// e persistência em JSON.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Status.h"
#include "library/Game.h"

namespace nebula {

class IFileSystem;
struct DetectedGame;

struct MergeStats {
  uint32_t added;
  uint32_t updated;
  uint32_t markedMissing;
  uint32_t restored;
  MergeStats() : added(0), updated(0), markedMissing(0), restored(0) {}
};

class Library {
 public:
  static const int kSchemaVersion = 1;

  Library();

  size_t size() const { return games_.size(); }
  const Game& at(size_t index) const { return games_[index]; }
  const Game* find(const std::string& id) const;
  Game* findMutable(const std::string& id);

  // Mescla o resultado de uma varredura. Jogos já conhecidos que estavam em
  // uma das pastas varridas com sucesso e não foram encontrados ficam
  // marcados como ausentes (não são apagados: favoritos e histórico ficam).
  MergeStats mergeScan(const std::vector<DetectedGame>& found,
                       const std::vector<std::string>& scannedRoots,
                       int64_t now, char separator);

  bool setFavorite(const std::string& id, bool favorite);
  bool toggleFavorite(const std::string& id);
  bool setCategoryOverride(const std::string& id, const std::string& category);
  bool setCoverPath(const std::string& id, const std::string& coverPath);
  bool recordLaunch(const std::string& id, int64_t now);
  size_t removeMissing();
  void clear();

  // Incrementa a cada mudança (a interface usa para saber quando atualizar).
  uint32_t revision() const { return revision_; }

  JsonValue toJson() const;
  // Entradas inválidas são ignoradas e contadas em "skipped".
  Status fromJson(const JsonValue& json, size_t& skipped);

 private:
  void reindex();
  void touch() { ++revision_; }

  std::vector<Game> games_;
  std::map<std::string, size_t> index_;
  uint32_t revision_;
};

// Carrega do disco. Se o arquivo não existir, a biblioteca fica vazia (OK).
// Se estiver corrompido, é renomeado para "<arquivo>.corrompido" e a
// biblioteca começa vazia; o Status retornado é kParseError para avisar.
Status loadLibrary(IFileSystem& fs, const std::string& path, Library& library);
Status saveLibrary(IFileSystem& fs, const std::string& path,
                   const Library& library);

}  // namespace nebula
