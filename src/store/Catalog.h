// Catálogo da loja de homebrews autorizados (formato próprio do Nébula).
//
// Só aceita itens com licença livre reconhecida ou com autorização escrita
// do autor registrada no próprio catálogo. Cada arquivo precisa de tamanho e
// SHA-256, verificados antes da instalação. Ver docs/fases/FASE_2.md.
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

struct CatalogFile {
  std::string url;
  std::string installPath;  // relativo à pasta do item, separado por '/'
  uint64_t size;
  std::string sha256;       // 64 dígitos hex minúsculos
  CatalogFile() : size(0) {}
};

struct CatalogAuthorization {
  std::string type;       // "license" ou "written-permission"
  std::string reference;  // URL/arquivo da licença ou da autorização
  std::string grantedBy;  // obrigatório para "written-permission"
  std::string date;       // AAAA-MM-DD
};

struct CatalogItem {
  std::string id;
  std::string name;
  std::string version;
  std::string author;
  std::string category;   // ver store::isValidStoreCategory
  std::string description;
  std::string license;    // identificador SPDX
  std::string licenseUrl;
  std::string sourceUrl;  // obrigatório para licenças copyleft
  std::string homepage;
  CatalogAuthorization authorization;
  std::vector<CatalogFile> files;

  uint64_t totalSize() const;
};

struct CatalogIssue {
  std::string itemId;
  std::string reason;
};

struct Catalog {
  static const int kSchemaVersion = 1;
  std::string name;
  std::string updatedAt;
  std::vector<CatalogItem> items;
};

namespace store {
bool isValidStoreCategory(const std::string& id);
bool isValidItemId(const std::string& id);
}  // namespace store

// Lê o catálogo. Itens que violam a política vão para "rejected" com o motivo
// e ficam fora de catalog.items. Só falha se o documento inteiro for inválido.
Status parseCatalog(const JsonValue& json, Catalog& catalog,
                    std::vector<CatalogIssue>& rejected);

}  // namespace nebula
