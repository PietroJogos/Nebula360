// Leitura do cabeçalho de metadados de pacotes STFS/SVOD ("CON ", "LIVE",
// "PIRS"), usados por jogos Games on Demand (GOD) e Xbox Live Arcade.
//
// Offsets conforme a documentação pública do Free60 (STFS). O cabeçalho de
// metadados é texto claro e assinado; nada aqui verifica ou contorna a
// assinatura - só lê nome, Title ID, tipo de conteúdo e a miniatura PNG.
//
// ATENÇÃO: validado só com dados sintéticos gerados nos testes. Precisa ser
// confirmado com pacotes reais do usuário (ver docs/fases/FASE_2.md).
#pragma once

#include <string>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class IFileSystem;

// Tipos de conteúdo relevantes para a biblioteca.
enum StfsContentType {
  kContentSavedGame = 0x00000001,
  kContentMarketplace = 0x00000002,  // DLC
  kContentInstalledGame = 0x00004000,
  kContentXboxOriginalGame = 0x00005000,
  kContentGamesOnDemand = 0x00007000,
  kContentGameDemo = 0x00080000,
  kContentInstaller = 0x000B0000,  // title updates
  kContentArcadeTitle = 0x000D0000,
  kContentCommunityGame = 0x02000000
};

struct StfsInfo {
  std::string magic;  // "CON ", "LIVE" ou "PIRS"
  uint32_t contentType;
  uint32_t metadataVersion;
  uint64_t contentSize;
  uint32_t mediaId;
  uint32_t version;
  uint32_t baseVersion;
  uint32_t titleId;
  uint8_t discNumber;
  uint8_t discCount;
  uint32_t descriptorType;  // 0 = STFS, 1 = SVOD
  uint32_t dataFileCount;
  std::string displayName;  // idioma 0 (inglês), UTF-8
  std::string description;
  std::string publisherName;
  std::string titleName;
  std::string thumbnailPng;       // vazio se ausente ou não for PNG
  std::string titleThumbnailPng;

  StfsInfo();
  // Nome mais adequado para exibir.
  std::string bestName() const;
};

// Tamanho de leitura suficiente para metadados + miniaturas.
const size_t kStfsHeaderReadBytes = 0x971A;
// Mínimo para ter nome e Title ID.
const size_t kStfsMinimumBytes = 0x1712;

bool hasStfsMagic(const uint8_t* data, size_t size);
Status parseStfsHeader(const uint8_t* data, size_t size, StfsInfo& out);
Status readStfsInfo(IFileSystem& fs, const std::string& path, StfsInfo& out);

const char* stfsContentTypeName(uint32_t contentType);
// Tipo de conteúdo que representa algo jogável na biblioteca?
bool isLaunchableContentType(uint32_t contentType);

}  // namespace nebula
