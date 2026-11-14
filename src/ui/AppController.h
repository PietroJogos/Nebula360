// Contrato entre as telas e o aplicativo. As telas não acessam sistema de
// arquivos, scanner ou lançador diretamente: pedem ao controlador.
#pragma once

#include <string>

#include "core/Types.h"
#include "library/LibraryQuery.h"

namespace nebula {

class Library;
class ModuleRegistry;
class Scanner;
class Screen;
class Strings;
class ThemeManager;
struct AppConfig;
struct Game;

enum KeyboardRequest {
  kKeyboardSearch = 1,
  kKeyboardAddPath = 2
};

class IAppController {
 public:
  virtual ~IAppController() {}

  virtual const Library& library() const = 0;
  virtual const AppConfig& config() const = 0;
  virtual const ThemeManager& themes() const = 0;
  virtual const Strings& strings() const = 0;
  virtual const Scanner& scanner() const = 0;
  virtual PlatformKind platform() const = 0;
  virtual bool canLaunch(const Game& game) const = 0;
  virtual const ModuleRegistry& modules() const = 0;
  virtual bool safeMode() const = 0;

  virtual void pushScreen(Screen* screen) = 0;
  virtual void popScreen() = 0;
  virtual void showToast(const std::string& message, bool isError) = 0;

  virtual void toggleFavorite(const std::string& gameId) = 0;
  virtual void setGameCategory(const std::string& gameId, const std::string& category) = 0;
  virtual void launchGame(const std::string& gameId) = 0;
  virtual void startScan() = 0;
  virtual void cancelScan() = 0;
  virtual size_t removeMissingGames() = 0;
  // Salva tudo, marca encerramento limpo e volta ao dashboard (no PC, fecha).
  virtual void exitToDashboard() = 0;

  virtual void setSortMode(SortMode mode) = 0;
  virtual void setLastCategory(const std::string& category) = 0;
  virtual void setShowMissing(bool show) = 0;
  virtual void applyTheme(const std::string& themeId) = 0;

  virtual void addScanPath(const std::string& path) = 0;
  virtual void removeScanPath(size_t index) = 0;
  virtual void toggleScanPath(size_t index) = 0;
  virtual void setScanPathCategory(size_t index, const std::string& category) = 0;

  virtual void openKeyboard(int requestId, const std::string& title,
                            const std::string& initialText) = 0;
  // Chamado pelo teclado virtual: a cada alteração (finished=false) e ao
  // concluir/cancelar (finished=true).
  virtual void onKeyboardText(int requestId, const std::string& text, bool finished,
                              bool accepted) = 0;
};

}  // namespace nebula
