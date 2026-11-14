// Manifesto de módulo do Nébula 360 (ver docs/arquitetura/SISTEMA_DE_PLUGINS.md).
//
// Nesta fase todo módulo é EMBUTIDO (compilado no próprio XEX). O campo
// "kind" reserva "script" para extensões interpretadas no futuro. Carregar
// executáveis nativos (DLLs/XEX) de terceiros NÃO é suportado, por decisão
// de segurança e estabilidade.
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Status.h"
#include "core/Types.h"
#include "core/Version.h"

namespace nebula {

enum ModuleKind {
  kModuleBuiltin = 0,  // código do próprio Nébula
  kModuleScript = 1    // reservado: extensão interpretada, com hash e assinatura
};

struct ModuleDependency {
  std::string id;
  Version minVersion;
  bool optional;  // se ausente, o módulo inicia mesmo assim
  ModuleDependency() : optional(false) {}
};

struct ModuleManifest {
  static const int kSchemaVersion = 1;

  std::string id;          // "nebula.ftp": minúsculas, dígitos, '.', '-', '_'
  std::string name;
  Version version;
  int apiVersion;          // contrato com o núcleo (ModuleRegistry::kApiVersion)
  ModuleKind kind;
  bool required;           // obrigatório: se falhar, o app não inicia normalmente
  bool enabledByDefault;
  std::vector<std::string> platforms;    // "host", "xbox360"; vazio = todas
  std::vector<ModuleDependency> dependencies;
  std::vector<std::string> conflicts;    // ids que não podem rodar junto
  std::vector<std::string> permissions;  // ver isKnownPermission
  std::vector<uint32_t> ports;           // portas TCP em que o módulo escuta
  uint32_t memoryBudgetKiB;              // orçamento declarado (0 = desprezível)
  std::string sha256;                    // integridade (obrigatório para scripts)

  ModuleManifest()
      : apiVersion(0), kind(kModuleBuiltin), required(false), enabledByDefault(true),
        memoryBudgetKiB(0) {}

  bool supportsPlatform(PlatformKind platform) const;
  bool hasPermission(const std::string& permission) const;
};

// Permissões conhecidas: "fs.read", "fs.write", "net.client", "net.server",
// "launch", "system.info", "ui".
bool isKnownPermission(const std::string& permission);
bool isValidModuleId(const std::string& id);

Status parseModuleManifest(const JsonValue& json, ModuleManifest& out);
Status parseModuleManifestText(const std::string& text, ModuleManifest& out);

}  // namespace nebula
