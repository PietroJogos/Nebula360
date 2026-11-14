// Raiz de composição do Nébula 360: junta configuração, biblioteca, scanner,
// temas, telas, salvamento e integrações. Independente de plataforma: o
// frontend (PC ou Xbox 360) só fornece sistema de arquivos, relógio,
// lançador, entrada e um IRenderer.
#pragma once

#include <string>
#include <vector>

#include "app/AppConfig.h"
#include "core/Log.h"
#include "integrations/Integration.h"
#include "library/CoverResolver.h"
#include "library/Library.h"
#include "library/Scanner.h"
#include "theme/Theme.h"
#include "ui/AppController.h"
#include "ui/Font.h"
#include "ui/Input.h"
#include "ui/Screen.h"
#include "ui/Strings.h"
#include "ui/TextureCache.h"

namespace nebula {

class IClock;
class IFileSystem;
class ILauncher;
class IRenderer;
class LibraryScreen;

struct AppPaths {
  std::string dataDir;    // gravável: nebula.json, library.json, logs, covers, cache
  std::string assetsDir;  // somente leitura: fonts, themes, lang
};

struct Toast {
  std::string message;
  bool error;
  uint64_t expiresAt;
};

class App : public IAppController {
 public:
  App(IFileSystem& fs, IClock& clock, ILauncher& launcher, PlatformKind platform);
  virtual ~App();

  // Erros recuperáveis viram avisos na tela; só falha sem fontes (não há
  // como desenhar a interface).
  Status init(const AppPaths& paths);
  void tick(const InputState& input);
  void render(IRenderer& renderer);
  // Salva o que estiver pendente (chamar ao sair, quando houver saída).
  void shutdown();

  // Para testes e frontends.
  std::string activeScreenName() const;
  size_t screenCount() const { return screens_.size(); }
  LibraryScreen* libraryScreen() const { return libraryScreen_; }
  const std::vector<Toast>& toasts() const { return toasts_; }
  const MemoryLogSink& recentLog() const { return memoryLog_; }
  bool scanning() const { return scanner_.running(); }
  const std::string& dataDir() const { return paths_.dataDir; }

  // IAppController
  virtual const Library& library() const NB_OVERRIDE { return library_; }
  virtual const AppConfig& config() const NB_OVERRIDE { return config_; }
  virtual const ThemeManager& themes() const NB_OVERRIDE { return themes_; }
  virtual const Strings& strings() const NB_OVERRIDE { return strings_; }
  virtual const Scanner& scanner() const NB_OVERRIDE { return scanner_; }
  virtual PlatformKind platform() const NB_OVERRIDE { return platform_; }
  virtual bool canLaunch(const Game& game) const NB_OVERRIDE;
  virtual void pushScreen(Screen* screen) NB_OVERRIDE;
  virtual void popScreen() NB_OVERRIDE;
  virtual void showToast(const std::string& message, bool isError) NB_OVERRIDE;
  virtual void toggleFavorite(const std::string& gameId) NB_OVERRIDE;
  virtual void setGameCategory(const std::string& gameId, const std::string& category) NB_OVERRIDE;
  virtual void launchGame(const std::string& gameId) NB_OVERRIDE;
  virtual void startScan() NB_OVERRIDE;
  virtual void cancelScan() NB_OVERRIDE;
  virtual size_t removeMissingGames() NB_OVERRIDE;
  virtual void setSortMode(SortMode mode) NB_OVERRIDE;
  virtual void setLastCategory(const std::string& category) NB_OVERRIDE;
  virtual void setShowMissing(bool show) NB_OVERRIDE;
  virtual void applyTheme(const std::string& themeId) NB_OVERRIDE;
  virtual void addScanPath(const std::string& path) NB_OVERRIDE;
  virtual void removeScanPath(size_t index) NB_OVERRIDE;
  virtual void toggleScanPath(size_t index) NB_OVERRIDE;
  virtual void setScanPathCategory(size_t index, const std::string& category) NB_OVERRIDE;
  virtual void openKeyboard(int requestId, const std::string& title,
                            const std::string& initialText) NB_OVERRIDE;
  virtual void onKeyboardText(int requestId, const std::string& text, bool finished,
                              bool accepted) NB_OVERRIDE;

 private:
  std::string dataPath(const std::string& name) const;
  std::string assetPath(const std::string& name) const;
  void finishScan();
  void resolveCovers();
  void saveConfigNow();
  bool saveLibraryNow();
  void renderToasts(UiContext& ctx);
  void markConfigDirty();

  IFileSystem& fs_;
  IClock& clock_;
  ILauncher& launcher_;
  PlatformKind platform_;
  AppPaths paths_;

  AppConfig config_;
  Library library_;
  Scanner scanner_;
  ThemeManager themes_;
  Strings strings_;
  FontSet fonts_;
  TextureCache textures_;
  CoverResolver* covers_;
  IntegrationRegistry integrations_;

  ScreenStack screens_;
  LibraryScreen* libraryScreen_;
  Screen* scanScreen_;
  InputMapper input_;
  std::vector<Action> actions_;
  std::vector<Toast> toasts_;

  MemoryLogSink memoryLog_;
  FileLogSink* fileLog_;
  uint32_t savedRevision_;
  uint32_t seenRevision_;
  uint64_t lastChangeMs_;
  uint64_t lastSaveFailureMs_;
  bool configDirty_;
  uint64_t configChangedMs_;
  bool initialized_;
  NB_NONCOPYABLE(App);
};

}  // namespace nebula
