// Fábricas das telas (evita expor detalhes de cada tela no resto do código).
#pragma once

#include <string>
#include <vector>

#include "ui/Screen.h"

namespace nebula {

class IAppController;

Screen* createGameDetailsScreen(IAppController& app, const std::string& gameId);
Screen* createMainMenuScreen(IAppController& app);
Screen* createScanScreen(IAppController& app);
Screen* createMessageScreen(IAppController& app, const std::string& title,
                            const std::vector<std::string>& lines);

enum KeyboardMode {
  kKeyboardModeSearch = 0,  // letras, números, pontuação básica
  kKeyboardModePath = 1     // inclui maiúsculas por padrão e símbolos de caminho
};
Screen* createKeyboardScreen(IAppController& app, int requestId, const std::string& title,
                             const std::string& initialText, KeyboardMode mode);

// Item genérico de menu.
struct MenuItem {
  std::string label;
  std::string value;  // texto à direita (estado atual, "›" etc.)
  bool enabled;
  MenuItem() : enabled(true) {}
  MenuItem(const std::string& l, const std::string& v, bool e = true)
      : label(l), value(v), enabled(e) {}
};

// Modelo de um menu em lista. O ListMenuScreen é dono do modelo.
class IMenuModel {
 public:
  virtual ~IMenuModel() {}
  virtual std::string title(IAppController& app) const = 0;
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const = 0;
  virtual void activate(IAppController& app, size_t index) = 0;
  // Ação opcional do botão X (ex.: adicionar pasta). Retorna o rótulo ou "".
  virtual std::string xHint(IAppController& app) const {
    (void)app;
    return "";
  }
  virtual void onX(IAppController& app, size_t index) {
    (void)app;
    (void)index;
  }
};

Screen* createListMenuScreen(IAppController& app, IMenuModel* model, const char* name);

}  // namespace nebula
