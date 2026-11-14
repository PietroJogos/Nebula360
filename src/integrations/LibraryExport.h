// Exportação aberta da biblioteca: <dados>/export/library.v1.json
//
// Formato estável e versionado ("format": "nebula360.library", "version": 1),
// para que outros dashboards/ferramentas leiam a biblioteca sem depender do
// formato interno. Especificação em docs/formatos/library-export-v1.md.
#pragma once

#include <string>

#include "core/Json.h"
#include "integrations/Integration.h"

namespace nebula {

class IClock;
class IFileSystem;

JsonValue buildLibraryExport(const Library& library, int64_t generatedAt);

class JsonExportIntegration : public IIntegration {
 public:
  JsonExportIntegration(IFileSystem& fs, const std::string& exportPath, IClock& clock);
  virtual const char* id() const NB_OVERRIDE { return "library-export-v1"; }
  virtual Status onLibrarySaved(const Library& library) NB_OVERRIDE;

 private:
  IFileSystem& fs_;
  std::string path_;
  IClock& clock_;
};

}  // namespace nebula
