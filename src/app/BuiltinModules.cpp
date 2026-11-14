#include "app/BuiltinModules.h"

#include "core/Log.h"
#include "core/Path.h"
#include "integrations/LibraryExport.h"
#include "plugins/ModuleRegistry.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

// Módulos de base: o trabalho real ainda é feito pelo App; aqui eles
// declaram identidade, dependências, permissões e orçamento de memória.
class MarkerModule : public IModule {
 public:
  virtual Status start(ModuleContext&) NB_OVERRIDE { return Status::Ok(); }
  virtual void stop() NB_OVERRIDE {}
};

// Exportação aberta da biblioteca (integrações, ex.: Neutro X).
class LibraryExportModule : public IModule {
 public:
  virtual Status start(ModuleContext& ctx) NB_OVERRIDE {
    if (ctx.integrations == NULL || ctx.fs == NULL || ctx.clock == NULL) {
      return Status(kInternal, "contexto sem integrações, sistema de arquivos ou relógio");
    }
    std::string exportPath = path::join(path::join(ctx.dataDir, "export", ctx.fs->separator()),
                                        "library.v1.json", ctx.fs->separator());
    ctx.integrations->add(new JsonExportIntegration(*ctx.fs, exportPath, *ctx.clock));
    return Status::Ok();
  }
  virtual void stop() NB_OVERRIDE {}
};

IModule* createMarker() { return new MarkerModule(); }
IModule* createLibraryExport() { return new LibraryExportModule(); }

struct Builtin {
  const char* manifest;
  ModuleFactory factory;
};

// Orçamentos de memória são ESTIMATIVAS documentadas em
// docs/analise/DEPENDENCIAS_NEBULA360.md (não medidas no console).
const Builtin kBuiltins[] = {
    {"{\"schema\":1,\"id\":\"nebula.core\",\"name\":\"Núcleo\",\"version\":\"0.3.0\","
     "\"apiVersion\":1,\"required\":true,\"permissions\":[\"fs.read\",\"fs.write\"],"
     "\"memoryBudgetKiB\":1024}",
     createMarker},
    {"{\"schema\":1,\"id\":\"nebula.ui\",\"name\":\"Interface\",\"version\":\"0.3.0\","
     "\"apiVersion\":1,\"required\":true,\"permissions\":[\"ui\",\"fs.read\"],"
     "\"dependencies\":[{\"id\":\"nebula.core\"}],\"memoryBudgetKiB\":24576}",
     createMarker},
    {"{\"schema\":1,\"id\":\"nebula.library\",\"name\":\"Biblioteca de jogos\",\"version\":\"0.3.0\","
     "\"apiVersion\":1,\"required\":true,\"permissions\":[\"fs.read\",\"fs.write\",\"launch\"],"
     "\"dependencies\":[{\"id\":\"nebula.core\"}],\"memoryBudgetKiB\":2048}",
     createMarker},
    {"{\"schema\":1,\"id\":\"nebula.export\",\"name\":\"Exportação da biblioteca\",\"version\":\"1.0.0\","
     "\"apiVersion\":1,\"required\":false,\"permissions\":[\"fs.write\"],"
     "\"dependencies\":[{\"id\":\"nebula.library\",\"minVersion\":\"0.3.0\"}],\"memoryBudgetKiB\":256}",
     createLibraryExport}};

}  // namespace

const std::vector<std::string>& builtinManifestTexts() {
  static std::vector<std::string> texts;
  if (texts.empty()) {
    for (size_t i = 0; i < sizeof(kBuiltins) / sizeof(kBuiltins[0]); ++i) {
      texts.push_back(kBuiltins[i].manifest);
    }
  }
  return texts;
}

Status registerBuiltinModules(ModuleRegistry& registry) {
  Status first;
  for (size_t i = 0; i < sizeof(kBuiltins) / sizeof(kBuiltins[0]); ++i) {
    ModuleManifest m;
    Status s = parseModuleManifestText(kBuiltins[i].manifest, m);
    if (s.ok()) s = registry.registerModule(m, kBuiltins[i].factory);
    if (!s.ok()) {
      NB_ERROR("modules", "módulo embutido inválido: " << s.toString());
      if (first.ok()) first = s;
    }
  }
  return first;
}

}  // namespace nebula
