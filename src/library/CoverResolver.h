// Localiza a capa de um jogo entre arquivos locais do usuário.
//
// Ordem de busca:
//   1. <dados>/covers/<TitleID>.png|jpg   (capas colocadas pelo usuário)
//   2. <dados>/covers/<id do jogo>.png|jpg
//   3. na pasta do jogo: cover.*, boxart.*, folder.*
//   4. <dados>/cache/icons/<id>.png  (miniatura extraída do contêiner GOD/XBLA)
//
// O Nébula NÃO inclui capas no pacote: a arte pertence às editoras. Uma
// fonte online configurável pelo usuário pode ser plugada via ICoverProvider.
#pragma once

#include <string>

#include "core/Status.h"

namespace nebula {

class IFileSystem;
struct Game;

class ICoverProvider {
 public:
  virtual ~ICoverProvider() {}
  virtual const char* name() const = 0;
  // Baixa (ou copia) a capa e grava em destPath. Implementações futuras.
  virtual Status fetchCover(const Game& game, const std::string& destPath) = 0;
};

class CoverResolver {
 public:
  CoverResolver(IFileSystem& fs, const std::string& dataDir);

  std::string resolve(const Game& game);
  // Grava a miniatura PNG extraída do contêiner no cache (se ainda não houver).
  Status storeIcon(const std::string& gameId, const std::string& png,
                   std::string& outPath);

  std::string coversDir() const;
  std::string iconCacheDir() const;

 private:
  std::string firstExisting(const std::string& dir, const std::string& baseName);

  IFileSystem& fs_;
  std::string dataDir_;
};

}  // namespace nebula
