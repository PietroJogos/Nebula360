#include "plugins/ModuleRegistry.h"

#include "core/Log.h"
#include "core/StringUtil.h"

namespace nebula {

namespace {
const char* const kTag = "modules";
}  // namespace

const char* moduleStateName(ModuleState s) {
  switch (s) {
    case kModuleRegistered: return "registered";
    case kModuleDisabled: return "disabled";
    case kModuleIncompatible: return "incompatible";
    case kModuleSkipped: return "skipped";
    case kModuleFailed: return "failed";
    case kModuleRunning: return "running";
    case kModuleStopped: return "stopped";
  }
  return "?";
}

ModuleRegistry::ModuleRegistry(PlatformKind platform)
    : platform_(platform), safeMode_(false), memoryLimitKiB_(0), memoryInUseKiB_(0),
      started_(false) {}

ModuleRegistry::~ModuleRegistry() { stopAll(); }

ModuleRecord* ModuleRegistry::findMutable(const std::string& id) {
  for (size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].manifest.id == id) return &records_[i];
  }
  return NULL;
}

const ModuleRecord* ModuleRegistry::find(const std::string& id) const {
  for (size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].manifest.id == id) return &records_[i];
  }
  return NULL;
}

bool ModuleRegistry::isRunning(const std::string& id) const {
  const ModuleRecord* r = find(id);
  return r != NULL && r->state == kModuleRunning;
}

Status ModuleRegistry::registerModule(const ModuleManifest& manifest, ModuleFactory factory) {
  if (started_) return Status(kInvalidArgument, "registro já iniciado");
  if (!isValidModuleId(manifest.id)) return Status(kInvalidArgument, "id inválido: " + manifest.id);
  if (findMutable(manifest.id) != NULL) {
    return Status(kInvalidArgument, "módulo já registrado: " + manifest.id);
  }
  if (factory == NULL && manifest.kind == kModuleBuiltin) {
    return Status(kInvalidArgument, manifest.id + ": módulo embutido sem fábrica");
  }
  ModuleRecord r;
  r.manifest = manifest;
  r.factory = factory;
  r.userEnabled = manifest.enabledByDefault;
  records_.push_back(r);
  return Status::Ok();
}

bool ModuleRegistry::setEnabled(const std::string& id, bool enabled) {
  ModuleRecord* r = findMutable(id);
  if (r == NULL) return false;
  r->userEnabled = enabled;
  return true;
}

void ModuleRegistry::evaluateCandidates() {
  for (size_t i = 0; i < records_.size(); ++i) {
    ModuleRecord& r = records_[i];
    const ModuleManifest& m = r.manifest;
    r.state = kModuleRegistered;
    r.reason.clear();
    if (!m.required && (!r.userEnabled || safeMode_)) {
      r.state = kModuleDisabled;
      r.reason = safeMode_ ? "modo seguro" : "desativado pelo usuário";
    } else if (m.apiVersion != kApiVersion) {
      r.state = kModuleIncompatible;
      r.reason = "API " + str::toString(m.apiVersion) + " (núcleo usa " + str::toString(kApiVersion) + ")";
    } else if (!m.supportsPlatform(platform_)) {
      r.state = kModuleIncompatible;
      r.reason = "plataforma não suportada";
    } else if (m.kind == kModuleScript) {
      // Reservado: ainda não há interpretador; nunca executa nada.
      r.state = kModuleIncompatible;
      r.reason = "módulos de script ainda não são suportados";
    }
  }
}

void ModuleRegistry::resolveOrder(std::vector<size_t>& order) {
  // Ordenação topológica por busca em profundidade (pós-ordem): dependências
  // sempre antes dos dependentes. As buscas começam pelos módulos
  // obrigatórios, para que eles (e o que precisam) fiquem com prioridade de
  // porta e de memória sobre os opcionais.
  order.clear();
  std::vector<int> visit(records_.size(), 0);  // 0 novo, 1 em pilha, 2 pronto
  std::vector<size_t> roots;
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t i = 0; i < records_.size(); ++i) {
      if (records_[i].manifest.required == (pass == 0)) roots.push_back(i);
    }
  }
  // Iterativo para não depender de recursão profunda.
  for (size_t r = 0; r < roots.size(); ++r) {
    size_t start = roots[r];
    if (visit[start] != 0 || records_[start].state != kModuleRegistered) continue;
    std::vector<size_t> stack;
    std::vector<size_t> nextDep;
    stack.push_back(start);
    nextDep.push_back(0);
    visit[start] = 1;
    while (!stack.empty()) {
      size_t cur = stack[stack.size() - 1];
      size_t& di = nextDep[nextDep.size() - 1];
      const std::vector<ModuleDependency>& deps = records_[cur].manifest.dependencies;
      if (di < deps.size()) {
        const ModuleDependency& d = deps[di++];
        size_t depIndex = records_.size();
        for (size_t k = 0; k < records_.size(); ++k) {
          if (records_[k].manifest.id == d.id) depIndex = k;
        }
        if (depIndex == records_.size() || records_[depIndex].state != kModuleRegistered) continue;
        if (visit[depIndex] == 1) {
          // Ciclo: todos os módulos na pilha a partir da dependência falham.
          bool mark = false;
          for (size_t s = 0; s < stack.size(); ++s) {
            if (stack[s] == depIndex) mark = true;
            if (mark) {
              records_[stack[s]].state = kModuleFailed;
              records_[stack[s]].reason = "ciclo de dependências";
            }
          }
          continue;
        }
        if (visit[depIndex] == 0) {
          visit[depIndex] = 1;
          stack.push_back(depIndex);
          nextDep.push_back(0);
        }
        continue;
      }
      visit[cur] = 2;
      order.push_back(cur);
      stack.pop_back();
      nextDep.pop_back();
    }
  }
}

bool ModuleRegistry::dependenciesReady(ModuleRecord& r) {
  for (size_t i = 0; i < r.manifest.dependencies.size(); ++i) {
    const ModuleDependency& d = r.manifest.dependencies[i];
    const ModuleRecord* dep = find(d.id);
    if (dep == NULL || dep->state != kModuleRunning) {
      if (d.optional) continue;
      r.state = kModuleSkipped;
      r.reason = dep == NULL ? "dependência ausente: " + d.id
                             : "dependência indisponível: " + d.id + " (" + moduleStateName(dep->state) + ")";
      return false;
    }
    if (dep->manifest.version < d.minVersion) {
      if (d.optional) continue;
      r.state = kModuleIncompatible;
      r.reason = "requer " + d.id + " >= " + d.minVersion.toString() + ", encontrado " +
                 dep->manifest.version.toString();
      return false;
    }
  }
  return true;
}

bool ModuleRegistry::conflictsWithRunning(ModuleRecord& r) {
  for (size_t i = 0; i < records_.size(); ++i) {
    const ModuleRecord& other = records_[i];
    if (&other == &r || other.state != kModuleRunning) continue;
    for (size_t c = 0; c < r.manifest.conflicts.size(); ++c) {
      if (r.manifest.conflicts[c] == other.manifest.id) {
        r.state = kModuleSkipped;
        r.reason = "conflito declarado com " + other.manifest.id;
        return true;
      }
    }
    for (size_t c = 0; c < other.manifest.conflicts.size(); ++c) {
      if (other.manifest.conflicts[c] == r.manifest.id) {
        r.state = kModuleSkipped;
        r.reason = "conflito declarado por " + other.manifest.id;
        return true;
      }
    }
    for (size_t p = 0; p < r.manifest.ports.size(); ++p) {
      for (size_t q = 0; q < other.manifest.ports.size(); ++q) {
        if (r.manifest.ports[p] == other.manifest.ports[q]) {
          r.state = kModuleSkipped;
          r.reason = "porta " + str::toString(r.manifest.ports[p]) + " já usada por " + other.manifest.id;
          return true;
        }
      }
    }
  }
  return false;
}

Status ModuleRegistry::startAll(ModuleContext& context) {
  if (started_) return Status(kInvalidArgument, "módulos já iniciados");
  started_ = true;
  startOrder_.clear();
  memoryInUseKiB_ = 0;
  evaluateCandidates();
  std::vector<size_t> order;
  resolveOrder(order);

  for (size_t i = 0; i < order.size(); ++i) {
    ModuleRecord& r = records_[order[i]];
    if (r.state != kModuleRegistered) continue;
    if (!dependenciesReady(r)) continue;
    if (conflictsWithRunning(r)) continue;
    if (memoryLimitKiB_ != 0 && memoryInUseKiB_ + r.manifest.memoryBudgetKiB > memoryLimitKiB_) {
      r.state = kModuleSkipped;
      r.reason = "memória insuficiente (" + str::toString(r.manifest.memoryBudgetKiB) + " KiB pedidos, " +
                 str::toString(memoryLimitKiB_ - memoryInUseKiB_) + " KiB livres)";
      continue;
    }
    r.instance = r.factory ? r.factory() : NULL;
    if (r.instance == NULL) {
      r.state = kModuleFailed;
      r.reason = "fábrica não criou o módulo";
      continue;
    }
    Status s = r.instance->start(context);
    if (!s.ok()) {
      r.state = kModuleFailed;
      r.reason = s.toString();
      delete r.instance;
      r.instance = NULL;
      continue;
    }
    r.state = kModuleRunning;
    memoryInUseKiB_ += r.manifest.memoryBudgetKiB;
    startOrder_.push_back(r.manifest.id);
  }

  Status result;
  for (size_t i = 0; i < records_.size(); ++i) {
    const ModuleRecord& r = records_[i];
    if (r.state == kModuleRunning) {
      NB_INFO(kTag, r.manifest.id << " " << r.manifest.version.toString() << ": rodando");
    } else {
      NB_LOG(r.manifest.required ? kLogError : (r.state == kModuleDisabled ? kLogInfo : kLogWarning),
             kTag, r.manifest.id << ": " << moduleStateName(r.state) << " (" << r.reason << ")");
    }
    if (r.manifest.required && r.state != kModuleRunning && result.ok()) {
      result = Status(kInternal, "módulo obrigatório indisponível: " + r.manifest.id + " (" + r.reason + ")");
    }
  }
  return result;
}

void ModuleRegistry::stopAll() {
  for (size_t i = startOrder_.size(); i > 0; --i) {
    ModuleRecord* r = findMutable(startOrder_[i - 1]);
    if (r == NULL || r->instance == NULL) continue;
    r->instance->stop();
    delete r->instance;
    r->instance = NULL;
    r->state = kModuleStopped;
  }
  startOrder_.clear();
  memoryInUseKiB_ = 0;
}

}  // namespace nebula
