// Configuração persistida em <dados>/nebula.json.
#pragma once

#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Status.h"
#include "core/Types.h"
#include "library/Scanner.h"

namespace nebula {

class IFileSystem;

struct AppConfig {
  static const int kSchemaVersion = 1;

  std::vector<ScanRoot> scanRoots;
  std::string themeId;
  std::string language;
  std::string sortMode;
  std::string lastCategory;
  bool showMissing;
  bool exportLibrary;  // integrações: grava export/library.v1.json
  std::string logLevel;

  AppConfig();

  JsonValue toJson() const;
  // Campos inválidos usam o padrão e geram um aviso em "warnings".
  static AppConfig fromJson(const JsonValue& json, std::vector<std::string>& warnings);
};

// Padrões por plataforma. No Xbox 360 aponta para locais comuns de RGH.
AppConfig defaultConfig(PlatformKind platform);

// Se o arquivo não existir, usa o padrão e retorna OK. Se estiver ilegível,
// usa o padrão, preserva uma cópia ".corrompido" e retorna kParseError.
Status loadConfig(IFileSystem& fs, const std::string& path, PlatformKind platform,
                  AppConfig& out, std::vector<std::string>& warnings);
Status saveConfig(IFileSystem& fs, const std::string& path, const AppConfig& config);

}  // namespace nebula
