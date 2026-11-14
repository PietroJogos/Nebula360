// Menus em lista (menu principal, pastas, temas, categorias), varredura e "Sobre".
#include "app/AppConfig.h"
#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/Library.h"
#include "library/Scanner.h"
#include "plugins/ModuleRegistry.h"
#include "theme/Theme.h"
#include "ui/AppController.h"
#include "ui/Font.h"
#include "ui/Strings.h"
#include "ui/Widgets.h"
#include "ui/screens/Screens.h"

namespace nebula {

namespace {

const char* const kCheck = "\xE2\x9C\x93";
const char* const kChevron = "\xE2\x80\xBA";

// ------------------------------------------------------------ ListMenuScreen

class ListMenuScreen : public Screen {
 public:
  ListMenuScreen(IAppController& app, IMenuModel* model, const char* name)
      : Screen(app), model_(model), name_(name), selected_(0), scrollTop_(0) {}
  virtual ~ListMenuScreen() { delete model_; }

  virtual const char* name() const NB_OVERRIDE { return name_; }
  virtual bool isOverlay() const NB_OVERRIDE { return true; }

  virtual void handleAction(Action action) NB_OVERRIDE {
    std::vector<MenuItem> items;
    model_->items(app_, items);
    if (selected_ >= items.size()) selected_ = items.empty() ? 0 : items.size() - 1;
    switch (action) {
      case kActionUp:
        if (!items.empty()) selected_ = selected_ == 0 ? items.size() - 1 : selected_ - 1;
        break;
      case kActionDown:
        if (!items.empty()) selected_ = (selected_ + 1) % items.size();
        break;
      case kActionAccept:
        if (!items.empty()) model_->activate(app_, selected_);
        break;
      case kActionX:
        if (!model_->xHint(app_).empty()) model_->onX(app_, selected_);
        break;
      case kActionBack: app_.popScreen(); break;
      default: break;
    }
  }

  virtual void render(UiContext& ctx) NB_OVERRIDE {
    const Theme& t = *ctx.theme;
    const Strings& s = *ctx.strings;
    IRenderer& r = *ctx.renderer;
    FontSet& f = *ctx.fonts;
    std::vector<MenuItem> items;
    model_->items(app_, items);
    if (selected_ >= items.size()) selected_ = items.empty() ? 0 : items.size() - 1;

    widgets::dimBackground(ctx);
    const int rowH = f.regular.lineHeight(t.metrics.fontBody) + 22;
    const int maxRows = 8;
    int visible = static_cast<int>(items.size()) < maxRows ? static_cast<int>(items.size()) : maxRows;
    if (visible < 1) visible = 1;
    Rect panel(kScreenWidth / 2 - 360, 0, 720, 120 + visible * (rowH + 6) + 60);
    panel.y = (kScreenHeight - panel.h) / 2;
    widgets::drawPanel(ctx, panel);
    text::draw(r, f.bold, t.metrics.fontTitle - 4, panel.x + 32, panel.y + 26, model_->title(app_),
               t.colors.text);

    if (static_cast<int>(selected_) < scrollTop_) scrollTop_ = static_cast<int>(selected_);
    if (static_cast<int>(selected_) >= scrollTop_ + maxRows) scrollTop_ = static_cast<int>(selected_) - maxRows + 1;
    int y = panel.y + 90;
    for (int i = scrollTop_; i < static_cast<int>(items.size()) && i < scrollTop_ + maxRows; ++i) {
      const MenuItem& it = items[static_cast<size_t>(i)];
      Rect row(panel.x + 20, y, panel.w - 40, rowH);
      bool sel = static_cast<size_t>(i) == selected_;
      if (sel) r.fillRoundedRect(row, 12, t.colors.accent);
      Color fg = sel ? t.colors.accentText : (it.enabled ? t.colors.text : t.colors.textMuted);
      int valueW = it.value.empty() ? 0 : text::measure(f.regular, t.metrics.fontBody, it.value) + 16;
      text::draw(r, sel ? f.bold : f.regular, t.metrics.fontBody, row.x + 18, row.y + 11,
                 text::ellipsize(f.regular, t.metrics.fontBody, it.label, row.w - 36 - valueW), fg);
      if (!it.value.empty()) {
        text::drawAligned(r, f.regular, t.metrics.fontBody, row.x, row.y + 11, row.w - 18, it.value,
                          sel ? t.colors.accentText : t.colors.textMuted, text::kAlignRight);
      }
      y += rowH + 6;
    }

    std::vector<ButtonHint> hints;
    hints.push_back(ButtonHint("A", s.get("hint.select")));
    std::string x = model_->xHint(app_);
    if (!x.empty()) hints.push_back(ButtonHint("X", x));
    hints.push_back(ButtonHint("B", s.get("hint.back")));
    widgets::drawButtonHintsAt(ctx, hints, panel.right() - 28, panel.bottom() - 46);
  }

 private:
  IMenuModel* model_;
  const char* name_;
  size_t selected_;
  int scrollTop_;
};

// ------------------------------------------------------------ modelos

class ThemePickerModel : public IMenuModel {
 public:
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("theme.title");
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    out.clear();
    const std::vector<Theme>& themes = app.themes().themes();
    for (size_t i = 0; i < themes.size(); ++i) {
      out.push_back(MenuItem(themes[i].name, themes[i].id == app.themes().current().id ? kCheck : ""));
    }
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    const std::vector<Theme>& themes = app.themes().themes();
    if (index < themes.size()) app.applyTheme(themes[index].id);
  }
};

class PathCategoryModel : public IMenuModel {
 public:
  explicit PathCategoryModel(size_t pathIndex) : pathIndex_(pathIndex) {}
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("paths.category");
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    out.clear();
    if (pathIndex_ >= app.config().scanRoots.size()) return;
    const std::string& current = app.config().scanRoots[pathIndex_].category;
    out.push_back(MenuItem(app.strings().get("category.auto"), current == category::kAuto ? kCheck : ""));
    const std::vector<std::string>& cats = category::all();
    for (size_t i = 0; i < cats.size(); ++i) {
      out.push_back(MenuItem(app.strings().get(category::labelKey(cats[i])),
                             current == cats[i] ? kCheck : ""));
    }
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    const std::vector<std::string>& cats = category::all();
    std::string chosen = index == 0 || index > cats.size() ? std::string(category::kAuto) : cats[index - 1];
    app.setScanPathCategory(pathIndex_, chosen);
    app.popScreen();
  }

 private:
  size_t pathIndex_;
};

class PathOptionsModel : public IMenuModel {
 public:
  explicit PathOptionsModel(size_t pathIndex) : pathIndex_(pathIndex) {}
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    if (pathIndex_ >= app.config().scanRoots.size()) return "";
    return app.config().scanRoots[pathIndex_].path;
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    out.clear();
    if (pathIndex_ >= app.config().scanRoots.size()) return;
    const ScanRoot& root = app.config().scanRoots[pathIndex_];
    const Strings& s = app.strings();
    out.push_back(MenuItem(s.get("paths.toggle"), s.get(root.enabled ? "paths.enabled" : "paths.disabled")));
    std::string cat = root.category == category::kAuto ? s.get("category.auto")
                                                       : s.get(category::labelKey(root.category));
    out.push_back(MenuItem(s.get("paths.category"), cat + " " + kChevron));
    out.push_back(MenuItem(s.get("paths.remove"), ""));
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    if (pathIndex_ >= app.config().scanRoots.size()) {
      app.popScreen();
      return;
    }
    if (index == 0) app.toggleScanPath(pathIndex_);
    if (index == 1) app.pushScreen(createListMenuScreen(app, new PathCategoryModel(pathIndex_), "path-category"));
    if (index == 2) {
      app.removeScanPath(pathIndex_);
      app.popScreen();
    }
  }

 private:
  size_t pathIndex_;
};

class PathsModel : public IMenuModel {
 public:
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("paths.title");
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    out.clear();
    const std::vector<ScanRoot>& roots = app.config().scanRoots;
    const Strings& s = app.strings();
    if (roots.empty()) {
      out.push_back(MenuItem(s.get("paths.empty"), "", false));
      return;
    }
    for (size_t i = 0; i < roots.size(); ++i) {
      std::string cat = roots[i].category == category::kAuto ? s.get("category.auto")
                                                             : s.get(category::labelKey(roots[i].category));
      std::string state = roots[i].enabled ? cat : s.get("paths.disabled");
      out.push_back(MenuItem(roots[i].path, state, roots[i].enabled));
    }
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    if (app.config().scanRoots.empty()) {
      onX(app, index);
      return;
    }
    app.pushScreen(createListMenuScreen(app, new PathOptionsModel(index), "path-options"));
  }
  virtual std::string xHint(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("hint.add");
  }
  virtual void onX(IAppController& app, size_t) NB_OVERRIDE {
    app.openKeyboard(kKeyboardAddPath, app.strings().get("paths.addPrompt"),
                     app.platform() == kPlatformXbox360 ? "Hdd1:\\" : "");
  }
};

enum MainMenuItem {
  kMenuPaths = 0,
  kMenuRescan,
  kMenuTheme,
  kMenuShowMissing,
  kMenuRemoveMissing,
  kMenuStore,
  kMenuModules,
  kMenuAbout,
  kMenuExit,
  kMenuCount
};

class MainMenuModel : public IMenuModel {
 public:
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("menu.title");
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    const Strings& s = app.strings();
    out.clear();
    out.push_back(MenuItem(s.get("menu.paths"),
                           str::toString(static_cast<uint64_t>(app.config().scanRoots.size())) + " " + kChevron));
    out.push_back(MenuItem(s.get("menu.rescan"), ""));
    out.push_back(MenuItem(s.get("menu.theme"), app.themes().current().name + " " + kChevron));
    out.push_back(MenuItem(s.get("menu.showMissing"), s.get(app.config().showMissing ? "menu.on" : "menu.off")));
    out.push_back(MenuItem(s.get("menu.removeMissing"), ""));
    out.push_back(MenuItem(s.get("menu.store"), s.get("menu.soon"), false));
    out.push_back(MenuItem(s.get("menu.modules"),
                           app.safeMode() ? s.get("menu.safemode") : std::string(kChevron)));
    out.push_back(MenuItem(s.get("menu.about"), kChevron));
    out.push_back(MenuItem(s.get("menu.exit"), ""));
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    const Strings& s = app.strings();
    switch (index) {
      case kMenuPaths: app.pushScreen(createListMenuScreen(app, new PathsModel(), "paths")); break;
      case kMenuRescan:
        app.popScreen();
        app.startScan();
        break;
      case kMenuTheme: app.pushScreen(createListMenuScreen(app, new ThemePickerModel(), "themes")); break;
      case kMenuShowMissing: app.setShowMissing(!app.config().showMissing); break;
      case kMenuRemoveMissing: {
        size_t n = app.removeMissingGames();
        app.showToast(s.format("toast.missing.removed", str::toString(static_cast<uint64_t>(n))), false);
        break;
      }
      case kMenuStore: app.showToast(s.get("toast.store.soon"), false); break;
      case kMenuModules: {
        std::vector<std::string> lines;
        const ModuleRegistry& reg = app.modules();
        for (size_t i = 0; i < reg.size(); ++i) {
          const ModuleRecord& r = reg.at(i);
          std::string line = r.manifest.name + " (" + r.manifest.id + " " + r.manifest.version.toString() +
                             "): " + s.get(std::string("module.state.") + moduleStateName(r.state));
          if (!r.reason.empty() && r.state != kModuleRunning) line += " \xE2\x80\x94 " + r.reason;
          lines.push_back(line);
        }
        if (app.safeMode()) lines.push_back(s.get("modules.safemode"));
        app.pushScreen(createMessageScreen(app, s.get("modules.title"), lines));
        break;
      }
      case kMenuExit: app.exitToDashboard(); break;
      case kMenuAbout: {
        std::vector<std::string> lines;
        lines.push_back(s.get("about.line1"));
        lines.push_back(s.get("about.line2"));
        lines.push_back(s.get("about.line3"));
        lines.push_back(s.get("about.line4"));
        app.pushScreen(createMessageScreen(app, s.get("about.title"), lines));
        break;
      }
      default: break;
    }
  }
};

// ------------------------------------------------------------ ScanScreen

class ScanScreen : public Screen {
 public:
  explicit ScanScreen(IAppController& app) : Screen(app) {}
  virtual const char* name() const NB_OVERRIDE { return "scan"; }
  virtual bool isOverlay() const NB_OVERRIDE { return true; }
  virtual void handleAction(Action action) NB_OVERRIDE {
    if (action == kActionBack) app_.cancelScan();
  }
  virtual void render(UiContext& ctx) NB_OVERRIDE {
    const Theme& t = *ctx.theme;
    const Strings& s = *ctx.strings;
    IRenderer& r = *ctx.renderer;
    FontSet& f = *ctx.fonts;
    const Scanner& sc = app_.scanner();
    widgets::dimBackground(ctx);
    Rect panel(kScreenWidth / 2 - 340, kScreenHeight / 2 - 130, 680, 260);
    widgets::drawPanel(ctx, panel);
    // Indicador de atividade: 8 pontos girando.
    int active = static_cast<int>((ctx.nowMs / 110) % 8);
    static const int kDx[8] = {0, 11, 16, 11, 0, -11, -16, -11};
    static const int kDy[8] = {-16, -11, 0, 11, 16, 11, 0, -11};
    for (int i = 0; i < 8; ++i) {
      int dist = (active - i + 8) % 8;
      uint8_t a = static_cast<uint8_t>(255 - dist * 28);
      r.fillRoundedRect(Rect(panel.x + 56 + kDx[i] - 4, panel.y + 52 + kDy[i] - 4, 8, 8), 4,
                        withAlpha(t.colors.accent, a));
    }
    text::draw(r, f.bold, t.metrics.fontTitle - 4, panel.x + 96, panel.y + 34, s.get("scan.title"), t.colors.text);
    text::draw(r, f.regular, t.metrics.fontBody, panel.x + 40, panel.y + 104,
               s.format("scan.found", str::toString(sc.stats().gamesFound)), t.colors.text);
    text::draw(r, f.regular, t.metrics.fontSmall, panel.x + 40, panel.y + 140,
               s.format("scan.folders", str::toString(sc.stats().directoriesVisited)), t.colors.textMuted);
    text::draw(r, f.regular, t.metrics.fontSmall, panel.x + 40, panel.y + 166,
               text::ellipsize(f.regular, t.metrics.fontSmall, sc.currentPath(), panel.w - 80),
               t.colors.textMuted);
    std::vector<ButtonHint> hints;
    hints.push_back(ButtonHint("B", s.get("hint.cancel")));
    widgets::drawButtonHintsAt(ctx, hints, panel.right() - 28, panel.bottom() - 46);
  }
};

// ------------------------------------------------------------ MessageScreen

class MessageScreen : public Screen {
 public:
  MessageScreen(IAppController& app, const std::string& title, const std::vector<std::string>& lines)
      : Screen(app), title_(title), lines_(lines) {}
  virtual const char* name() const NB_OVERRIDE { return "message"; }
  virtual bool isOverlay() const NB_OVERRIDE { return true; }
  virtual void handleAction(Action action) NB_OVERRIDE {
    if (action == kActionBack || action == kActionAccept) app_.popScreen();
  }
  virtual void render(UiContext& ctx) NB_OVERRIDE {
    const Theme& t = *ctx.theme;
    const Strings& s = *ctx.strings;
    FontSet& f = *ctx.fonts;
    widgets::dimBackground(ctx);
    std::vector<std::string> wrapped;
    for (size_t i = 0; i < lines_.size(); ++i) {
      std::vector<std::string> w = text::wrap(f.regular, t.metrics.fontBody, lines_[i], 760, 3);
      wrapped.insert(wrapped.end(), w.begin(), w.end());
      wrapped.push_back("");
    }
    int lh = f.regular.lineHeight(t.metrics.fontBody);
    Rect panel(kScreenWidth / 2 - 420, 0, 840, 150 + static_cast<int>(wrapped.size()) * lh);
    panel.y = (kScreenHeight - panel.h) / 2;
    widgets::drawPanel(ctx, panel);
    text::draw(*ctx.renderer, f.bold, t.metrics.fontTitle - 4, panel.x + 40, panel.y + 28, title_,
               t.colors.text);
    int y = panel.y + 84;
    for (size_t i = 0; i < wrapped.size(); ++i) {
      text::draw(*ctx.renderer, f.regular, t.metrics.fontBody, panel.x + 40, y, wrapped[i], t.colors.text);
      y += lh;
    }
    std::vector<ButtonHint> hints;
    hints.push_back(ButtonHint("B", s.get("hint.back")));
    widgets::drawButtonHintsAt(ctx, hints, panel.right() - 28, panel.bottom() - 46);
  }

 private:
  std::string title_;
  std::vector<std::string> lines_;
};

}  // namespace

Screen* createListMenuScreen(IAppController& app, IMenuModel* model, const char* name) {
  return new ListMenuScreen(app, model, name);
}

Screen* createMainMenuScreen(IAppController& app) {
  return new ListMenuScreen(app, new MainMenuModel(), "menu");
}

Screen* createScanScreen(IAppController& app) { return new ScanScreen(app); }

Screen* createMessageScreen(IAppController& app, const std::string& title,
                            const std::vector<std::string>& lines) {
  return new MessageScreen(app, title, lines);
}

}  // namespace nebula
