#include "app/App.h"

#include "app/BuiltinModules.h"

#include "core/Path.h"
#include "core/StringUtil.h"
#include "integrations/LibraryExport.h"
#include "library/Category.h"
#include "platform/Clock.h"
#include "platform/FileSystem.h"
#include "platform/Launcher.h"
#include "ui/Renderer.h"
#include "ui/Widgets.h"
#include "ui/screens/LibraryScreen.h"
#include "ui/screens/Screens.h"

namespace nebula {

namespace {

const char* const kTag = "app";
const uint32_t kScanDirectoriesPerTick = 6;
const uint64_t kSaveDebounceMs = 1000;
const uint64_t kSaveRetryMs = 5000;
const uint64_t kToastMs = 3200;
const size_t kMaxToasts = 3;
const uint64_t kMaxLogBytes = 512 * 1024;

IClock* gLogClock = NULL;
uint64_t logTime() { return gLogClock ? gLogClock->monotonicMs() : 0; }

}  // namespace

App::App(IFileSystem& fs, IClock& clock, ILauncher& launcher, PlatformKind platform)
    : fs_(fs), clock_(clock), launcher_(launcher), platform_(platform), scanner_(fs),
      themes_(fs), textures_(fs, 96, 2), covers_(NULL), modules_(platform), bootGuard_(NULL),
      quitRequested_(false), libraryScreen_(NULL), scanScreen_(NULL),
      fileLog_(NULL), savedRevision_(0), seenRevision_(0), lastChangeMs_(0),
      lastSaveFailureMs_(0), configDirty_(false), configChangedMs_(0), initialized_(false) {
  gLogClock = &clock_;
  Log::setTimeSource(logTime);
  Log::addSink(&memoryLog_);
}

App::~App() {
  shutdown();
  screens_.clear();
  screens_.collectGarbage();
  Log::removeSink(&memoryLog_);
  if (fileLog_ != NULL) {
    Log::removeSink(fileLog_);
    delete fileLog_;
  }
  if (gLogClock == &clock_) {
    gLogClock = NULL;
    Log::setTimeSource(NULL);
  }
  delete covers_;
  delete bootGuard_;
}

std::string App::dataPath(const std::string& name) const {
  std::string out = paths_.dataDir;
  std::vector<std::string> parts = str::split(name, '/');
  for (size_t i = 0; i < parts.size(); ++i) out = path::join(out, parts[i], fs_.separator());
  return out;
}

std::string App::assetPath(const std::string& name) const {
  std::string out = paths_.assetsDir;
  std::vector<std::string> parts = str::split(name, '/');
  for (size_t i = 0; i < parts.size(); ++i) out = path::join(out, parts[i], fs_.separator());
  return out;
}

Status App::init(const AppPaths& paths) {
  paths_ = paths;
  Status s = fs_.createDirectories(paths_.dataDir);
  if (s.ok()) s = fs_.createDirectories(dataPath("logs"));
  if (s.ok()) {
    fileLog_ = new FileLogSink(fs_, dataPath("logs/nebula.log"), kMaxLogBytes);
    Log::addSink(fileLog_);
  } else {
    NB_ERROR(kTag, "pasta de dados indisponível: " << s.toString());
  }
  NB_INFO(kTag, "Nébula 360 iniciando (plataforma: "
                    << (platform_ == kPlatformXbox360 ? "Xbox 360" : "PC") << ")");

  // Configuração.
  std::vector<std::string> warnings;
  bool configExisted = fs_.exists(dataPath("nebula.json"));
  Status cs = loadConfig(fs_, dataPath("nebula.json"), platform_, config_, warnings);
  LogLevel level;
  if (parseLogLevel(config_.logLevel, level)) Log::setLevel(level);

  // Textos.
  Status ls = strings_.loadFromFile(fs_, assetPath("lang/" + config_.language + ".json"));
  if (!ls.ok() && config_.language != "pt-BR") {
    NB_WARN(kTag, ls.toString());
    ls = strings_.loadFromFile(fs_, assetPath("lang/pt-BR.json"));
  }
  if (!ls.ok()) {
    NB_ERROR(kTag, ls.toString());
    showToast(strings_.get("toast.lang.missing"), true);
  }
  if (!cs.ok()) showToast(strings_.get("toast.config.corrupt"), true);
  if (!configExisted || !cs.ok()) saveConfigNow();

  // Fontes (obrigatórias).
  s = fonts_.regular.loadFromFile(fs_, assetPath("fonts/Inter-Regular.otf"));
  if (!s.ok()) {
    NB_ERROR(kTag, "fonte obrigatória ausente: " << s.toString());
    return s;
  }
  s = fonts_.bold.loadFromFile(fs_, assetPath("fonts/Inter-SemiBold.otf"));
  if (!s.ok()) {
    NB_WARN(kTag, "fonte em negrito ausente; usando a regular: " << s.toString());
    s = fonts_.bold.loadFromFile(fs_, assetPath("fonts/Inter-Regular.otf"));
    if (!s.ok()) return s;
  }

  // Temas: embutidos em assets e, opcionalmente, do usuário em <dados>/themes.
  std::vector<std::string> themeWarnings;
  themes_.loadDirectory(assetPath("themes"), themeWarnings);
  if (fs_.isDirectory(dataPath("themes"))) themes_.loadDirectory(dataPath("themes"), themeWarnings);
  if (!themes_.select(config_.themeId)) {
    showToast(strings_.format("toast.theme.missing", config_.themeId), true);
    config_.themeId = themes_.current().id;
    markConfigDirty();
  }

  // Biblioteca.
  covers_ = new CoverResolver(fs_, paths_.dataDir);
  Status lib = loadLibrary(fs_, dataPath("library.json"), library_);
  if (!lib.ok()) showToast(strings_.get("toast.library.corrupt"), true);
  savedRevision_ = seenRevision_ = library_.revision();

  // Módulos: obrigatórios sempre; opcionais conforme a configuração, exceto
  // em modo seguro (ativado após encerramentos inesperados seguidos).
  bootGuard_ = new BootGuard(fs_, dataPath("session.json"));
  bootGuard_->begin();
  if (bootGuard_->previousSessionCrashed()) showToast(strings_.get("toast.crash"), true);
  modules_.setSafeMode(bootGuard_->safeModeRecommended());
  registerBuiltinModules(modules_);
  modules_.setEnabled("nebula.export", config_.exportLibrary);
  ModuleContext moduleContext;
  moduleContext.platform = platform_;
  moduleContext.fs = &fs_;
  moduleContext.clock = &clock_;
  moduleContext.dataDir = paths_.dataDir;
  moduleContext.integrations = &integrations_;
  Status ms = modules_.startAll(moduleContext);
  if (!ms.ok()) showToast(strings_.format("toast.module.required", ms.message()), true);
  if (modules_.safeMode()) showToast(strings_.get("toast.safemode"), true);

  libraryScreen_ = new LibraryScreen(*this);
  screens_.push(libraryScreen_);
  initialized_ = true;

  bool anyEnabled = false;
  for (size_t i = 0; i < config_.scanRoots.size(); ++i) anyEnabled = anyEnabled || config_.scanRoots[i].enabled;
  if (library_.size() == 0 && anyEnabled) startScan();
  NB_INFO(kTag, "inicialização concluída");
  return Status::Ok();
}

void App::shutdown() {
  if (!initialized_) return;
  if (scanner_.running()) scanner_.cancel();
  if (library_.revision() != savedRevision_) saveLibraryNow();
  if (configDirty_) saveConfigNow();
  modules_.stopAll();
  if (bootGuard_ != NULL) bootGuard_->markCleanExit();
  NB_INFO(kTag, "encerrando");
  if (fileLog_ != NULL) fileLog_->flush();
  initialized_ = false;
}

std::string App::activeScreenName() const {
  Screen* top = screens_.top();
  return top ? top->name() : "";
}

void App::tick(const InputState& input) {
  if (!initialized_) return;
  uint64_t now = clock_.monotonicMs();
  textures_.beginFrame();
  input_.update(input, now, actions_);
  for (size_t i = 0; i < actions_.size(); ++i) {
    Screen* top = screens_.top();
    if (top != NULL) top->handleAction(actions_[i]);
  }
  screens_.collectGarbage();
  // Varredura antes de atualizar as telas: o resultado aparece no mesmo quadro.
  if (scanner_.running()) {
    scanner_.step(kScanDirectoriesPerTick);
    if (!scanner_.running()) finishScan();
    screens_.collectGarbage();
  }
  if (screens_.top() != NULL) screens_.top()->update(now);
  if (libraryScreen_ != NULL && screens_.top() != libraryScreen_) libraryScreen_->update(now);

  // Avisos expirados.
  for (size_t i = toasts_.size(); i > 0; --i) {
    if (toasts_[i - 1].expiresAt <= now) toasts_.erase(toasts_.begin() + static_cast<std::ptrdiff_t>(i - 1));
  }

  // Salvamento com atraso (agrupa várias mudanças seguidas).
  if (library_.revision() != seenRevision_) {
    seenRevision_ = library_.revision();
    lastChangeMs_ = now;
  }
  if (library_.revision() != savedRevision_ && now - lastChangeMs_ >= kSaveDebounceMs &&
      (lastSaveFailureMs_ == 0 || now - lastSaveFailureMs_ >= kSaveRetryMs)) {
    saveLibraryNow();
  }
  if (configDirty_ && now - configChangedMs_ >= kSaveDebounceMs) saveConfigNow();
}

void App::render(IRenderer& renderer) {
  if (!initialized_) return;
  UiContext ctx;
  ctx.renderer = &renderer;
  ctx.fonts = &fonts_;
  ctx.theme = &themes_.current();
  ctx.strings = &strings_;
  ctx.textures = &textures_;
  ctx.nowMs = clock_.monotonicMs();
  screens_.render(ctx);
  renderToasts(ctx);
}

void App::renderToasts(UiContext& ctx) {
  if (toasts_.empty()) return;
  const Theme& t = *ctx.theme;
  IRenderer& r = *ctx.renderer;
  int px = t.metrics.fontBody - 2;
  int h = fonts_.regular.lineHeight(px) + 22;
  int y = kScreenHeight - t.metrics.safeMarginY - 60 - h;
  for (size_t i = toasts_.size(); i > 0; --i) {
    const Toast& toast = toasts_[i - 1];
    std::string msg = text::ellipsize(fonts_.regular, px, toast.message, 900);
    int w = text::measure(fonts_.regular, px, msg) + 48;
    Rect box((kScreenWidth - w) / 2, y, w, h);
    uint64_t left = toast.expiresAt > ctx.nowMs ? toast.expiresAt - ctx.nowMs : 0;
    uint8_t a = left >= 300 ? 255 : static_cast<uint8_t>(left * 255 / 300);
    r.fillRoundedRect(box, h / 2, withAlpha(makeColor(0x10, 0x10, 0x18), static_cast<uint8_t>(a * 235 / 255)));
    if (toast.error) r.fillRoundedRect(Rect(box.x + 10, box.y + 10, 6, box.h - 20), 3, withAlpha(t.colors.danger, a));
    text::draw(r, fonts_.regular, px, box.x + 26, box.y + 11, msg, withAlpha(0xFFFFFFFFu, a));
    y -= h + 10;
  }
}

void App::showToast(const std::string& message, bool isError) {
  if (isError) NB_WARN(kTag, "aviso exibido: " << message);
  Toast t;
  t.message = message;
  t.error = isError;
  t.expiresAt = clock_.monotonicMs() + kToastMs;
  toasts_.push_back(t);
  while (toasts_.size() > kMaxToasts) toasts_.erase(toasts_.begin());
}

void App::pushScreen(Screen* screen) { screens_.push(screen); }

void App::popScreen() {
  // A tela da biblioteca é a base e nunca sai da pilha.
  if (screens_.size() <= 1) return;
  if (screens_.top() == scanScreen_) scanScreen_ = NULL;
  screens_.pop();
}

bool App::canLaunch(const Game& game) const { return launcher_.canLaunch(game); }

// ------------------------------------------------------------ biblioteca

void App::toggleFavorite(const std::string& gameId) {
  if (!library_.toggleFavorite(gameId)) return;
  const Game* g = library_.find(gameId);
  showToast(strings_.get(g && g->favorite ? "toast.favorite.on" : "toast.favorite.off"), false);
}

void App::setGameCategory(const std::string& gameId, const std::string& cat) {
  if (!library_.setCategoryOverride(gameId, cat)) return;
  const Game* g = library_.find(gameId);
  if (g) showToast(strings_.format("toast.category", strings_.get(category::labelKey(g->effectiveCategory()))), false);
}

void App::launchGame(const std::string& gameId) {
  const Game* g = library_.find(gameId);
  if (g == NULL) return;
  if (g->missing) {
    showToast(strings_.get("toast.game.missing"), true);
    return;
  }
  if (!launcher_.canLaunch(*g)) {
    showToast(strings_.get("toast.launch.unsupported"), false);
    return;
  }
  // Salva antes: no Xbox 360 o lançamento bem-sucedido não retorna.
  library_.recordLaunch(gameId, clock_.unixTimeSeconds());
  saveLibraryNow();
  if (configDirty_) saveConfigNow();
  // O lançamento troca o título: a sessão termina de forma limpa.
  if (bootGuard_ != NULL) bootGuard_->markCleanExit();
  if (fileLog_ != NULL) fileLog_->flush();
  Status s = launcher_.launch(*g);
  if (!s.ok()) {
    if (bootGuard_ != NULL) bootGuard_->markRunning();
    NB_ERROR(kTag, "falha ao iniciar " << g->path << ": " << s.toString());
    showToast(strings_.format("toast.launch.error", s.message()), true);
  }
}

void App::exitToDashboard() {
  saveLibraryNow();
  if (configDirty_) saveConfigNow();
  if (bootGuard_ != NULL) bootGuard_->markCleanExit();
  NB_INFO(kTag, "saindo para o dashboard");
  if (fileLog_ != NULL) fileLog_->flush();
  Status s = launcher_.returnToDashboard();
  if (s.code() == kUnsupported) {
    quitRequested_ = true;  // PC: o frontend fecha a janela
    return;
  }
  if (!s.ok()) {
    if (bootGuard_ != NULL) bootGuard_->markRunning();
    NB_ERROR(kTag, "falha ao voltar ao dashboard: " << s.toString());
    showToast(strings_.format("toast.exit.error", s.message()), true);
  }
}

void App::startScan() {
  if (scanner_.running()) return;
  bool anyEnabled = false;
  for (size_t i = 0; i < config_.scanRoots.size(); ++i) anyEnabled = anyEnabled || config_.scanRoots[i].enabled;
  if (!anyEnabled) {
    showToast(strings_.get("toast.paths.none"), false);
    return;
  }
  scanner_.start(config_.scanRoots);
  scanScreen_ = createScanScreen(*this);
  screens_.push(scanScreen_);
}

void App::cancelScan() {
  if (!scanner_.running()) return;
  scanner_.cancel();
  finishScan();
}

void App::finishScan() {
  if (scanScreen_ != NULL && screens_.top() == scanScreen_) popScreen();
  scanScreen_ = NULL;
  if (scanner_.cancelled()) {
    showToast(strings_.get("toast.scan.cancelled"), false);
    return;
  }
  MergeStats st = library_.mergeScan(scanner_.results(), scanner_.accessibleRoots(),
                                     clock_.unixTimeSeconds(), fs_.separator());
  const std::vector<DetectedGame>& found = scanner_.results();
  for (size_t i = 0; i < found.size(); ++i) {
    if (found[i].iconPng.empty()) continue;
    std::string out;
    Status s = covers_->storeIcon(makeGameId(found[i].launchPath), found[i].iconPng, out);
    if (!s.ok()) NB_WARN(kTag, "miniatura não gravada: " << s.toString());
  }
  resolveCovers();
  textures_.clear();
  saveLibraryNow();
  showToast(strings_.format("toast.scan.done", str::toString(st.added), str::toString(st.markedMissing)), false);
  if (scanner_.stats().errors > 0) {
    showToast(strings_.format("toast.scan.errors", str::toString(scanner_.stats().errors)), true);
  }
}

void App::resolveCovers() {
  for (size_t i = 0; i < library_.size(); ++i) {
    const Game& g = library_.at(i);
    library_.setCoverPath(g.id, covers_->resolve(g));
  }
}

size_t App::removeMissingGames() {
  size_t n = library_.removeMissing();
  if (n > 0) saveLibraryNow();
  return n;
}

bool App::saveLibraryNow() {
  Status s = saveLibrary(fs_, dataPath("library.json"), library_);
  if (!s.ok()) {
    lastSaveFailureMs_ = clock_.monotonicMs();
    showToast(strings_.format("toast.save.error", s.message()), true);
    return false;
  }
  lastSaveFailureMs_ = 0;
  savedRevision_ = seenRevision_ = library_.revision();
  integrations_.notifyLibrarySaved(library_);
  return true;
}

// ------------------------------------------------------------ preferências

void App::markConfigDirty() {
  configDirty_ = true;
  configChangedMs_ = clock_.monotonicMs();
}

void App::saveConfigNow() {
  Status s = saveConfig(fs_, dataPath("nebula.json"), config_);
  if (!s.ok()) {
    NB_ERROR(kTag, "falha ao salvar configuração: " << s.toString());
    showToast(strings_.format("toast.save.error", s.message()), true);
    configChangedMs_ = clock_.monotonicMs() + kSaveRetryMs;
    return;
  }
  configDirty_ = false;
}

void App::setSortMode(SortMode mode) {
  config_.sortMode = sortModeId(mode);
  markConfigDirty();
}

void App::setLastCategory(const std::string& cat) {
  if (config_.lastCategory == cat) return;
  config_.lastCategory = cat;
  markConfigDirty();
}

void App::setShowMissing(bool show) {
  config_.showMissing = show;
  markConfigDirty();
}

void App::applyTheme(const std::string& themeId) {
  if (!themes_.select(themeId)) {
    showToast(strings_.format("toast.theme.missing", themeId), true);
    return;
  }
  config_.themeId = themeId;
  markConfigDirty();
  showToast(strings_.format("toast.theme", themes_.current().name), false);
}

void App::addScanPath(const std::string& rawPath) {
  std::string p = path::stripTrailingSeparator(str::trim(rawPath), fs_.separator());
  if (p.empty() || p.size() > 240 || (platform_ == kPlatformXbox360 && p.find(':') == std::string::npos)) {
    showToast(strings_.get("toast.path.invalid"), true);
    return;
  }
  for (size_t i = 0; i < config_.scanRoots.size(); ++i) {
    if (str::iequalsAscii(config_.scanRoots[i].path, p)) {
      showToast(strings_.get("toast.path.duplicate"), false);
      return;
    }
  }
  ScanRoot root;
  root.path = p;
  config_.scanRoots.push_back(root);
  markConfigDirty();
  showToast(strings_.format("toast.path.added", p), false);
}

void App::removeScanPath(size_t index) {
  if (index >= config_.scanRoots.size()) return;
  config_.scanRoots.erase(config_.scanRoots.begin() + static_cast<std::ptrdiff_t>(index));
  markConfigDirty();
  showToast(strings_.get("toast.path.removed"), false);
}

void App::toggleScanPath(size_t index) {
  if (index >= config_.scanRoots.size()) return;
  config_.scanRoots[index].enabled = !config_.scanRoots[index].enabled;
  markConfigDirty();
}

void App::setScanPathCategory(size_t index, const std::string& cat) {
  if (index >= config_.scanRoots.size()) return;
  if (cat != category::kAuto && !category::isValid(cat)) return;
  config_.scanRoots[index].category = cat;
  markConfigDirty();
}

void App::openKeyboard(int requestId, const std::string& title, const std::string& initialText) {
  KeyboardMode mode = requestId == kKeyboardAddPath ? kKeyboardModePath : kKeyboardModeSearch;
  screens_.push(createKeyboardScreen(*this, requestId, title, initialText, mode));
}

void App::onKeyboardText(int requestId, const std::string& text, bool finished, bool accepted) {
  if (requestId == kKeyboardSearch && libraryScreen_ != NULL) {
    libraryScreen_->setSearch(text);
  } else if (requestId == kKeyboardAddPath && finished && accepted) {
    addScanPath(text);
  }
}

}  // namespace nebula
