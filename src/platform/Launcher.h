// Lançamento de jogos/aplicativos. Cada plataforma tem a sua implementação.
#pragma once

#include <string>

#include "core/Status.h"

namespace nebula {

struct Game;

class ILauncher {
 public:
  virtual ~ILauncher() {}
  // No Xbox 360 uma chamada bem-sucedida não retorna (o título é trocado).
  virtual Status launch(const Game& game) = 0;
  virtual bool canLaunch(const Game& game) const = 0;
  // Sai do Nébula e volta ao dashboard padrão do sistema (no console com
  // DashLaunch, o item "Default" do launch.ini). Em sucesso não retorna.
  // kUnsupported = a plataforma não tem dashboard (PC): o frontend fecha.
  virtual Status returnToDashboard() = 0;
};

// Lançador para PC: não executa jogos de Xbox 360; só registra o pedido.
class UnsupportedLauncher : public ILauncher {
 public:
  virtual Status launch(const Game& game) NB_OVERRIDE;
  virtual bool canLaunch(const Game&) const NB_OVERRIDE { return false; }
  virtual Status returnToDashboard() NB_OVERRIDE {
    return Status(kUnsupported, "não há dashboard para onde voltar no PC");
  }
};

}  // namespace nebula
