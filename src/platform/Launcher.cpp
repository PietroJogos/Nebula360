#include "platform/Launcher.h"

#include "core/Log.h"
#include "library/Game.h"

namespace nebula {

Status UnsupportedLauncher::launch(const Game& game) {
  NB_INFO("launcher", "pedido de lançamento ignorado no PC: " << game.path);
  return Status(kUnsupported, "lançar jogos só é possível no Xbox 360");
}

}  // namespace nebula
