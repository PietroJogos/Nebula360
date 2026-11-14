// Pontos de integração com outros aplicativos (ex.: futuro suporte ao Neutro X).
//
// O Nébula NÃO incorpora código de terceiros aqui. A integração acontece por
// contratos abertos:
//   1. Exportação da biblioteca em JSON documentado (LibraryExport.h).
//   2. IIntegration: adaptadores notificados quando a biblioteca muda.
// Um adaptador específico (Neutro X ou outro) só deve ser escrito quando a
// especificação pública desse projeto estiver disponível.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class Library;

class IIntegration {
 public:
  virtual ~IIntegration() {}
  virtual const char* id() const = 0;
  // Chamado depois que a biblioteca foi salva com sucesso.
  virtual Status onLibrarySaved(const Library& library) = 0;
};

class IntegrationRegistry {
 public:
  IntegrationRegistry() {}
  ~IntegrationRegistry();

  // Assume a posse do objeto.
  void add(IIntegration* integration);
  size_t size() const { return items_.size(); }
  // Falhas de um adaptador são registradas no log e não afetam os outros.
  size_t notifyLibrarySaved(const Library& library);

 private:
  std::vector<IIntegration*> items_;
  NB_NONCOPYABLE(IntegrationRegistry);
};

}  // namespace nebula
