// Tela de detalhes de um jogo: capa, metadados e ações.
#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/Library.h"
#include "theme/Theme.h"
#include "ui/AppController.h"
#include "ui/Font.h"
#include "ui/Strings.h"
#include "ui/Widgets.h"
#include "ui/screens/Screens.h"

namespace nebula {

namespace {

enum DetailAction { kDetailPlay = 0, kDetailFavorite, kDetailCategory, kDetailBack, kDetailCount };

class CategoryPickerModel : public IMenuModel {
 public:
  explicit CategoryPickerModel(const std::string& gameId) : gameId_(gameId) {}
  virtual std::string title(IAppController& app) const NB_OVERRIDE {
    return app.strings().get("categoryPicker.title");
  }
  virtual void items(IAppController& app, std::vector<MenuItem>& out) const NB_OVERRIDE {
    out.clear();
    const Game* g = app.library().find(gameId_);
    if (g == NULL) return;
    const Strings& s = app.strings();
    std::string check = "\xE2\x9C\x93";
    out.push_back(MenuItem(s.format("categoryPicker.detected", s.get(category::labelKey(g->category))),
                           g->categoryOverride.empty() ? check : ""));
    const std::vector<std::string>& cats = category::all();
    for (size_t i = 0; i < cats.size(); ++i) {
      out.push_back(MenuItem(s.get(category::labelKey(cats[i])),
                             g->categoryOverride == cats[i] ? check : ""));
    }
  }
  virtual void activate(IAppController& app, size_t index) NB_OVERRIDE {
    const std::vector<std::string>& cats = category::all();
    std::string chosen = index == 0 || index > cats.size() ? std::string() : cats[index - 1];
    app.setGameCategory(gameId_, chosen);
    app.popScreen();
  }

 private:
  std::string gameId_;
};

class GameDetailsScreen : public Screen {
 public:
  GameDetailsScreen(IAppController& app, const std::string& gameId)
      : Screen(app), gameId_(gameId), selected_(0) {}

  virtual const char* name() const NB_OVERRIDE { return "details"; }

  virtual void handleAction(Action action) NB_OVERRIDE {
    const Game* g = app_.library().find(gameId_);
    if (g == NULL) {
      app_.popScreen();
      return;
    }
    switch (action) {
      case kActionUp:
        if (selected_ > 0) --selected_;
        break;
      case kActionDown:
        if (selected_ + 1 < kDetailCount) ++selected_;
        break;
      case kActionBack: app_.popScreen(); break;
      case kActionY: app_.toggleFavorite(gameId_); break;
      case kActionAccept: activate(); break;
      default: break;
    }
  }

  virtual void render(UiContext& ctx) NB_OVERRIDE {
    const Game* g = app_.library().find(gameId_);
    widgets::drawBackground(ctx);
    if (g == NULL) return;
    const Theme& t = *ctx.theme;
    const Strings& s = *ctx.strings;
    IRenderer& r = *ctx.renderer;
    FontSet& f = *ctx.fonts;
    const int x = t.metrics.safeMarginX;
    const int y = t.metrics.safeMarginY;

    widgets::drawLogo(ctx, x, y + 6, t.metrics.fontBody + 2);

    Rect cover(x + 16, y + 70, 300, 420);
    r.fillRoundedRect(Rect(cover.x - 8, cover.y - 8, cover.w + 16, cover.h + 16),
                      t.metrics.cornerRadius + 8, withAlpha(t.colors.accent, 0x30));
    widgets::drawCover(ctx, cover, *g, 255, t.metrics.cornerRadius + 2, false);

    const int cx = cover.right() + 56;
    const int cw = kScreenWidth - t.metrics.safeMarginX - cx;
    int cy = cover.y - 6;
    std::vector<std::string> titleLines = text::wrap(f.bold, t.metrics.fontTitle + 6, g->title, cw, 2);
    for (size_t i = 0; i < titleLines.size(); ++i) {
      text::draw(r, f.bold, t.metrics.fontTitle + 6, cx, cy, titleLines[i], t.colors.text);
      cy += f.bold.lineHeight(t.metrics.fontTitle + 6);
    }
    if (g->favorite) {
      text::draw(r, f.bold, t.metrics.fontBody, cx, cy + 2,
                 "\xE2\x98\x85 " + s.get("tab.favorites"), t.colors.favorite);
      cy += f.bold.lineHeight(t.metrics.fontBody) + 4;
    }
    cy += 10;

    std::vector<std::pair<std::string, std::string> > rows;
    rows.push_back(std::make_pair(s.get("details.categoryLabel"),
                                  s.get(category::labelKey(g->effectiveCategory()))));
    rows.push_back(std::make_pair(s.get("details.format"), s.get("format." + g->format)));
    if (g->titleId != 0) rows.push_back(std::make_pair(s.get("details.titleId"), str::toHex32(g->titleId)));
    rows.push_back(std::make_pair(s.get("details.plays"), str::toString(g->playCount)));
    rows.push_back(std::make_pair(s.get("details.lastPlayed"),
                                  g->lastPlayedAt > 0 ? str::formatDate(g->lastPlayedAt)
                                                      : s.get("details.never")));
    rows.push_back(std::make_pair(s.get("details.path"), g->path));
    int px = t.metrics.fontBody - 2;
    int labelW = 170;
    for (size_t i = 0; i < rows.size(); ++i) {
      text::draw(r, f.regular, px, cx, cy, rows[i].first, t.colors.textMuted);
      text::draw(r, f.regular, px, cx + labelW, cy,
                 text::ellipsize(f.regular, px, rows[i].second, cw - labelW), t.colors.text);
      cy += f.regular.lineHeight(px) + 6;
    }
    if (g->missing) {
      cy += 4;
      text::draw(r, f.bold, px, cx, cy, s.get("details.missing"), t.colors.danger);
      cy += f.bold.lineHeight(px) + 6;
    }

    // Ações.
    cy += 14;
    const char* labels[kDetailCount] = {"details.play",
                                        g->favorite ? "details.favorite.remove" : "details.favorite.add",
                                        "details.category", "details.back"};
    int bh = f.bold.lineHeight(t.metrics.fontBody) + 18;
    for (int i = 0; i < kDetailCount; ++i) {
      Rect b(cx, cy, 380, bh);
      bool sel = static_cast<size_t>(i) == selected_;
      r.fillRoundedRect(b, bh / 2, sel ? t.colors.accent : t.colors.surface);
      std::string label = s.get(labels[i]);
      if (i == kDetailPlay && !app_.canLaunch(*g)) label += "  \xE2\x80\x94  Xbox 360";
      text::draw(r, sel ? f.bold : f.regular, t.metrics.fontBody, b.x + 24, b.y + 9, label,
                 sel ? t.colors.accentText : t.colors.text);
      cy += bh + 10;
    }

    std::vector<ButtonHint> hints;
    hints.push_back(ButtonHint("A", s.get("hint.select")));
    hints.push_back(ButtonHint("Y", s.get("hint.favorite")));
    hints.push_back(ButtonHint("B", s.get("hint.back")));
    widgets::drawButtonHints(ctx, hints);
  }

 private:
  void activate() {
    switch (selected_) {
      case kDetailPlay: app_.launchGame(gameId_); break;
      case kDetailFavorite: app_.toggleFavorite(gameId_); break;
      case kDetailCategory:
        app_.pushScreen(createListMenuScreen(app_, new CategoryPickerModel(gameId_), "category-picker"));
        break;
      case kDetailBack: app_.popScreen(); break;
      default: break;
    }
  }

  std::string gameId_;
  size_t selected_;
};

}  // namespace

Screen* createGameDetailsScreen(IAppController& app, const std::string& gameId) {
  return new GameDetailsScreen(app, gameId);
}

}  // namespace nebula
