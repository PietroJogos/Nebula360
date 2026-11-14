// Entrada da biblioteca.
#pragma once

#include <string>

#include "core/Types.h"

namespace nebula {

struct Game {
  std::string id;          // estável: derivado do caminho de lançamento
  std::string title;
  std::string path;        // caminho de lançamento (default.xex, contêiner GOD...)
  std::string folder;      // pasta do jogo
  std::string root;        // pasta de varredura em que foi encontrado
  std::string format;      // gameformat::*
  std::string category;    // detectada
  std::string categoryOverride;  // escolhida pelo usuário ("" = usar detectada)
  std::string coverPath;   // imagem local resolvida ("" = sem capa)
  uint32_t titleId;
  uint32_t mediaId;
  bool favorite;
  bool missing;            // não encontrado na última varredura da sua pasta
  int64_t addedAt;         // unix (s)
  int64_t lastPlayedAt;    // unix (s), 0 = nunca
  uint32_t playCount;

  Game()
      : titleId(0), mediaId(0), favorite(false), missing(false), addedAt(0),
        lastPlayedAt(0), playCount(0) {}

  const std::string& effectiveCategory() const {
    return categoryOverride.empty() ? category : categoryOverride;
  }
};

// ID estável: 16 primeiros dígitos hex do SHA-256 do caminho em minúsculas
// (o FATX não diferencia maiúsculas).
std::string makeGameId(const std::string& launchPath);

}  // namespace nebula
