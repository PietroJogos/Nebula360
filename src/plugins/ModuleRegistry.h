// Registro de módulos: valida manifestos, resolve dependências, detecta
// conflitos (inclusive de portas), respeita orçamento de memória e modo
// seguro, e isola falhas (um módulo opcional que falha não derruba o app).
//
// Os módulos são objetos C++ criados por fábricas registradas no código. Não
// há carregamento dinâmico de executáveis.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"
#include "plugins/ModuleManifest.h"

namespace nebula {

class IFileSystem;
class IClock;
class IntegrationRegistry;

// Serviços que o núcleo oferece aos módulos.
struct ModuleContext {
  PlatformKind platform;
  IFileSystem* fs;
  IClock* clock;
  std::string dataDir;
  IntegrationRegistry* integrations;
  ModuleContext() : platform(kPlatformHost), fs(NULL), clock(NULL), integrations(NULL) {}
};

class IModule {
 public:
  virtual ~IModule() {}
  virtual Status start(ModuleContext& context) = 0;
  virtual void stop() = 0;
};

typedef IModule* (*ModuleFactory)();

enum ModuleState {
  kModuleRegistered = 0,  // ainda não avaliado
  kModuleDisabled,        // desligado pelo usuário ou pelo modo seguro
  kModuleIncompatible,    // API, plataforma ou dependência em versão incompatível
  kModuleSkipped,         // dependência ausente/falha, conflito ou memória insuficiente
  kModuleFailed,          // start() retornou erro (ou ciclo de dependências)
  kModuleRunning,
  kModuleStopped
};
const char* moduleStateName(ModuleState state);

struct ModuleRecord {
  ModuleManifest manifest;
  ModuleFactory factory;
  IModule* instance;
  ModuleState state;
  std::string reason;
  bool userEnabled;
  ModuleRecord() : factory(NULL), instance(NULL), state(kModuleRegistered), userEnabled(true) {}
};

class ModuleRegistry {
 public:
  static const int kApiVersion = 1;

  explicit ModuleRegistry(PlatformKind platform);
  ~ModuleRegistry();

  // Falha (sem lançar nada) se o id já existir ou o manifesto for incoerente.
  Status registerModule(const ModuleManifest& manifest, ModuleFactory factory);
  // Preferência do usuário; módulos obrigatórios ignoram "false".
  bool setEnabled(const std::string& id, bool enabled);
  void setSafeMode(bool safeMode) { safeMode_ = safeMode; }
  bool safeMode() const { return safeMode_; }
  // 0 = sem limite.
  void setMemoryLimitKiB(uint32_t limit) { memoryLimitKiB_ = limit; }

  // Avalia e inicia tudo em ordem de dependência. Retorna erro somente se um
  // módulo obrigatório não puder rodar.
  Status startAll(ModuleContext& context);
  // Para na ordem inversa da inicialização.
  void stopAll();

  size_t size() const { return records_.size(); }
  const ModuleRecord& at(size_t index) const { return records_[index]; }
  const ModuleRecord* find(const std::string& id) const;
  bool isRunning(const std::string& id) const;
  const std::vector<std::string>& startOrder() const { return startOrder_; }
  uint32_t memoryInUseKiB() const { return memoryInUseKiB_; }

 private:
  ModuleRecord* findMutable(const std::string& id);
  void evaluateCandidates();
  void resolveOrder(std::vector<size_t>& order);
  bool dependenciesReady(ModuleRecord& r);
  bool conflictsWithRunning(ModuleRecord& r);

  PlatformKind platform_;
  std::vector<ModuleRecord> records_;
  std::vector<std::string> startOrder_;
  bool safeMode_;
  uint32_t memoryLimitKiB_;
  uint32_t memoryInUseKiB_;
  bool started_;
  NB_NONCOPYABLE(ModuleRegistry);
};

}  // namespace nebula
