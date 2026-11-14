#include "ui/screens/LibraryScreen.h"

#include "app/AppConfig.h"
#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/Library.h"
#include "library/LibraryQuery.h"
#include "theme/Theme.h"
#include "ui/AppController.h"
#include "ui/Font.h"
#include "ui/Strings.h"
#include "ui/Widgets.h"
#include "ui/screens/Screens.h"

namespace nebula {

const char* const LibraryScreen::kTabAll = "";
const char* const LibraryScreen::kTabFavorites = "favorites";

namespace {
const int kTabsHeight = 44;
const int kFooterHeight = 44;
}  // namespace

LibraryScreen::LibraryScreen(IAppController& app)
    : Screen(app), tab_(0), selected_(0), scroll_(0), scrollTarget_(0), seenRevision_(0),
      seenShowMissing_(false), dirty_(true), lastUpdateMs_(0) {
  tabs_.push_back(kTabAll);
  const std::string& last = app.config().lastCategory;
  if (!last.empty()) {
    tabs_.push_back(last);
    tab_ = 1;
  }
}

const std::string& LibraryScreen::currentTab() const { return tabs_[tab_]; }

std::string LibraryScreen::selectedGameId() const {
  if (selected_ >= results_.size()) return "";
  return app_.library().at(results_[selected_]).id;
}

void LibraryScreen::setSearch(const std::string& text) {
  if (text == search_) return;
  search_ = text;
  selected_ = 0;
  scrollTarget_ = 0;
  dirty_ = true;
  refresh();
}

void LibraryScreen::refresh() {
  const Library& lib = app_.library();
  const AppConfig& cfg = app_.config();
  std::string keepId = selectedGameId();
  std::string keepTab = tabs_.empty() ? std::string() : tabs_[tab_];

  // Abas: Todos, Favoritos e as categorias que têm jogos.
  std::vector<std::string> tabs;
  tabs.push_back(kTabAll);
  tabs.push_back(kTabFavorites);
  const std::vector<std::string>& cats = category::all();
  for (size_t c = 0; c < cats.size(); ++c) {
    bool present = cats[c] == keepTab;
    for (size_t i = 0; i < lib.size() && !present; ++i) {
      const Game& g = lib.at(i);
      if ((!g.missing || cfg.showMissing) && g.effectiveCategory() == cats[c]) present = true;
    }
    if (present) tabs.push_back(cats[c]);
  }
  tabs_.swap(tabs);
  tab_ = 0;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i] == keepTab) tab_ = i;
  }

  LibraryQuery q;
  q.search = search_;
  q.includeMissing = cfg.showMissing;
  SortMode mode = kSortTitle;
  parseSortMode(cfg.sortMode, mode);
  q.sort = mode;
  if (tabs_[tab_] == kTabFavorites) {
    q.favoritesOnly = true;
  } else {
    q.category = tabs_[tab_];
  }
  results_ = runQuery(lib, q);

  selected_ = results_.empty() ? 0 : (selected_ < results_.size() ? selected_ : results_.size() - 1);
  for (size_t i = 0; i < results_.size() && !keepId.empty(); ++i) {
    if (lib.at(results_[i]).id == keepId) {
      selected_ = i;
      break;
    }
  }
  seenRevision_ = lib.revision();
  seenSort_ = cfg.sortMode;
  seenShowMissing_ = cfg.showMissing;
  dirty_ = false;
  ensureVisible();
}

LibraryScreen::GridLayout LibraryScreen::layout(const UiContext* ctx) const {
  const ThemeMetrics& m = ctx ? ctx->theme->metrics : app_.themes().current().metrics;
  GridLayout g;
  g.columns = m.columns;
  g.tileW = m.tileWidth;
  g.tileH = m.tileHeight;
  g.spacing = m.tileSpacing;
  int contentW = kScreenWidth - 2 * m.safeMarginX;
  // Se o tema pedir colunas demais para a largura, reduz.
  while (g.columns > 1 && g.columns * g.tileW + (g.columns - 1) * g.spacing > contentW) --g.columns;
  int gridW = g.columns * g.tileW + (g.columns - 1) * g.spacing;
  g.originX = m.safeMarginX + (contentW - gridW) / 2;
  g.top = m.safeMarginY + m.headerHeight + kTabsHeight + 8;
  int bottom = kScreenHeight - m.safeMarginY - kFooterHeight;
  g.height = bottom - g.top;
  g.visibleRows = (g.height + g.spacing) / (g.tileH + g.spacing);
  if (g.visibleRows < 1) g.visibleRows = 1;
  return g;
}

void LibraryScreen::ensureVisible() {
  GridLayout g = layout(NULL);
  if (results_.empty()) {
    scrollTarget_ = 0;
    return;
  }
  int row = static_cast<int>(selected_) / g.columns;
  int rowH = g.tileH + g.spacing;
  float rowTop = static_cast<float>(row * rowH);
  float rowBottom = rowTop + g.tileH;
  // Margem para o efeito de foco (bloco ampliado) não ser cortado.
  const float pad = 10.0f;
  if (rowTop - pad < scrollTarget_) scrollTarget_ = rowTop - pad;
  if (rowBottom + pad > scrollTarget_ + g.height) scrollTarget_ = rowBottom + pad - g.height;
  if (scrollTarget_ < 0) scrollTarget_ = 0;
}

void LibraryScreen::move(int dx, int dy) {
  if (results_.empty()) return;
  GridLayout g = layout(NULL);
  int count = static_cast<int>(results_.size());
  int cur = static_cast<int>(selected_);
  int col = cur % g.columns;
  int next = cur;
  if (dx != 0) {
    int c = col + dx;
    if (c >= 0 && c < g.columns) next = cur + dx;
  }
  if (dy != 0) next = cur + dy * g.columns;
  if (next < 0) next = dy < 0 ? (dy < -1 ? 0 : cur) : cur;
  if (next >= count) {
    // Descer na última linha incompleta leva ao último item.
    int lastRow = (count - 1) / g.columns;
    next = (cur / g.columns < lastRow) ? count - 1 : cur;
  }
  selected_ = static_cast<size_t>(next);
  ensureVisible();
}

void LibraryScreen::changeTab(int delta) {
  if (tabs_.size() < 2) return;
  int n = static_cast<int>(tabs_.size());
  tab_ = static_cast<size_t>((static_cast<int>(tab_) + delta + n) % n);
  app_.setLastCategory(tabs_[tab_]);
  selected_ = 0;
  scroll_ = scrollTarget_ = 0;
  dirty_ = true;
  refresh();
}

bool LibraryScreen::stale() const {
  const AppConfig& cfg = app_.config();
  return dirty_ || seenRevision_ != app_.library().revision() || seenSort_ != cfg.sortMode ||
         seenShowMissing_ != cfg.showMissing;
}

void LibraryScreen::handleAction(Action action) {
  // Nunca age sobre uma lista antiga (ex.: varredura terminou neste quadro).
  if (stale()) refresh();
  GridLayout g = layout(NULL);
  switch (action) {
    case kActionUp: move(0, -1); break;
    case kActionDown: move(0, 1); break;
    case kActionLeft: move(-1, 0); break;
    case kActionRight: move(1, 0); break;
    case kActionPageUp: move(0, -(g.visibleRows > 1 ? g.visibleRows : 1) * 2); break;
    case kActionPageDown: move(0, (g.visibleRows > 1 ? g.visibleRows : 1) * 2); break;
    case kActionPrevTab: changeTab(-1); break;
    case kActionNextTab: changeTab(1); break;
    case kActionAccept:
      if (!results_.empty()) app_.pushScreen(createGameDetailsScreen(app_, selectedGameId()));
      break;
    case kActionY:
      if (!results_.empty()) app_.toggleFavorite(selectedGameId());
      break;
    case kActionX:
      app_.openKeyboard(kKeyboardSearch, app_.strings().get("keyboard.search"), search_);
      break;
    case kActionBack:
      if (!search_.empty()) setSearch("");
      break;
    case kActionView: {
      SortMode mode = kSortTitle;
      parseSortMode(app_.config().sortMode, mode);
      SortMode next = static_cast<SortMode>((mode + 1) % kSortModeCount);
      app_.setSortMode(next);
      app_.showToast(app_.strings().format("toast.sort",
                                           app_.strings().get(std::string("sort.") + sortModeId(next))),
                     false);
      selected_ = 0;
      scrollTarget_ = 0;
      dirty_ = true;
      refresh();
      break;
    }
    case kActionMenu: app_.pushScreen(createMainMenuScreen(app_)); break;
    case kActionNone: break;
  }
}

void LibraryScreen::update(uint64_t nowMs) {
  if (stale()) refresh();
  // Rolagem suave (aproximação exponencial, independente de FPS).
  uint64_t dt = lastUpdateMs_ == 0 ? 16 : nowMs - lastUpdateMs_;
  lastUpdateMs_ = nowMs;
  float k = dt >= 120 ? 1.0f : static_cast<float>(dt) / 120.0f;
  scroll_ += (scrollTarget_ - scroll_) * k;
  float diff = scroll_ - scrollTarget_;
  if (diff < 0.5f && diff > -0.5f) scroll_ = scrollTarget_;
}

void LibraryScreen::renderHeader(UiContext& ctx, const GridLayout& g) {
  const Theme& t = *ctx.theme;
  const Strings& s = *ctx.strings;
  IRenderer& r = *ctx.renderer;
  FontSet& f = *ctx.fonts;
  const int x = t.metrics.safeMarginX;
  const int right = kScreenWidth - t.metrics.safeMarginX;
  const int y = t.metrics.safeMarginY;

  widgets::drawLogo(ctx, x, y + 6, t.metrics.fontTitle);
  std::string count = s.format("library.count", str::toString(static_cast<uint64_t>(results_.size())));
  SortMode mode = kSortTitle;
  parseSortMode(app_.config().sortMode, mode);
  std::string sortLabel = s.get(std::string("sort.") + sortModeId(mode));
  text::drawAligned(r, f.bold, t.metrics.fontBody, x, y + 4, right - x, count, t.colors.text,
                    text::kAlignRight);
  text::drawAligned(r, f.regular, t.metrics.fontSmall, x, y + 8 + f.bold.lineHeight(t.metrics.fontBody),
                    right - x, sortLabel, t.colors.textMuted, text::kAlignRight);

  // Abas.
  int ty = y + t.metrics.headerHeight;
  int px = t.metrics.fontBody - 2;
  int pillH = f.regular.lineHeight(px) + 12;
  int cx = x;
  cx += widgets::drawButtonBadge(ctx, cx, ty + (pillH - (t.metrics.fontSmall + 10)) / 2, "LB") + 10;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    std::string label;
    if (tabs_[i] == kTabAll) label = s.get("tab.all");
    else if (tabs_[i] == kTabFavorites) label = s.get("tab.favorites");
    else label = s.get(category::labelKey(tabs_[i]));
    int w = text::measure(f.regular, px, label) + 28;
    bool sel = i == tab_;
    if (sel) r.fillRoundedRect(Rect(cx, ty, w, pillH), pillH / 2, t.colors.accent);
    text::drawAligned(r, sel ? f.bold : f.regular, px, cx, ty + 6, w, label,
                      sel ? t.colors.accentText : t.colors.textMuted, text::kAlignCenter);
    cx += w + 6;
  }
  cx += widgets::drawButtonBadge(ctx, cx + 4, ty + (pillH - (t.metrics.fontSmall + 10)) / 2, "RB") + 4;

  if (!search_.empty()) {
    std::string label = s.format("library.search", search_);
    // Usa só o espaço que sobrou à direita das abas.
    int maxW = right - cx - 24 - 24;
    if (maxW > 360) maxW = 360;
    if (maxW < 80) maxW = 80;
    label = text::ellipsize(f.regular, t.metrics.fontSmall, label, maxW);
    int w = text::measure(f.regular, t.metrics.fontSmall, label) + 24;
    Rect chip(right - w, ty + 4, w, pillH - 8);
    r.fillRoundedRect(chip, chip.h / 2, t.colors.surfaceFocused);
    text::drawAligned(r, f.regular, t.metrics.fontSmall, chip.x, chip.y + 4, chip.w, label,
                      t.colors.text, text::kAlignCenter);
  }
  (void)g;
}

void LibraryScreen::renderGrid(UiContext& ctx, const GridLayout& g) {
  const Theme& t = *ctx.theme;
  const Strings& s = *ctx.strings;
  IRenderer& r = *ctx.renderer;
  FontSet& f = *ctx.fonts;
  const Library& lib = app_.library();

  if (results_.empty()) {
    bool filtered = !search_.empty() || tab_ != 0;
    int cy = g.top + g.height / 2 - 40;
    text::drawAligned(r, f.bold, t.metrics.fontTitle - 4, 0, cy, kScreenWidth,
                      filtered ? s.get("library.empty.filtered") : s.get("library.empty.title"),
                      t.colors.text, text::kAlignCenter);
    if (!filtered) {
      text::drawAligned(r, f.regular, t.metrics.fontBody, 0, cy + 48, kScreenWidth,
                        s.get("library.empty.body"), t.colors.textMuted, text::kAlignCenter);
    }
    return;
  }

  Rect area(0, g.top - 12, kScreenWidth, g.height + 24);
  r.pushClip(area);
  int rowH = g.tileH + g.spacing;
  int scroll = static_cast<int>(scroll_ + 0.5f);
  int firstRow = scroll / rowH;
  int lastRow = (scroll + g.height) / rowH + 1;
  for (int row = firstRow; row <= lastRow; ++row) {
    for (int col = 0; col < g.columns; ++col) {
      size_t i = static_cast<size_t>(row * g.columns + col);
      if (i >= results_.size() || i == selected_) continue;
      const Game& game = lib.at(results_[i]);
      Rect tile(g.originX + col * (g.tileW + g.spacing), g.top + row * rowH - scroll, g.tileW, g.tileH);
      uint8_t alpha = game.missing ? 110 : 255;
      widgets::drawCover(ctx, tile, game, alpha, t.metrics.cornerRadius, true);
      if (game.favorite) {
        Rect badge(tile.right() - 34, tile.y + 8, 26, 26);
        r.fillRoundedRect(badge, 13, makeColor(0, 0, 0, 0x90));
        text::drawAligned(r, f.bold, 17, badge.x, badge.y + 2, badge.w, "\xE2\x98\x85",
                          t.colors.favorite, text::kAlignCenter);
      }
      if (game.missing) {
        std::string label = s.get("library.missing");
        int w = text::measure(f.bold, t.metrics.fontSmall - 2, label) + 16;
        Rect pill(tile.x + 8, tile.y + 8, w, 24);
        r.fillRoundedRect(pill, 12, t.colors.danger);
        text::drawAligned(r, f.bold, t.metrics.fontSmall - 2, pill.x, pill.y + 3, pill.w, label,
                          0xFFFFFFFFu, text::kAlignCenter);
      }
    }
  }
  // Bloco em foco por último (ampliado, por cima dos vizinhos).
  if (selected_ < results_.size()) {
    int row = static_cast<int>(selected_) / g.columns;
    int col = static_cast<int>(selected_) % g.columns;
    const Game& game = lib.at(results_[selected_]);
    Rect base(g.originX + col * (g.tileW + g.spacing), g.top + row * rowH - scroll, g.tileW, g.tileH);
    int gx = g.tileW * 5 / 100 / 2;
    int gy = g.tileH * 5 / 100 / 2;
    Rect tile(base.x - gx, base.y - gy, base.w + 2 * gx, base.h + 2 * gy);
    r.fillRoundedRect(Rect(tile.x - 6, tile.y - 6, tile.w + 12, tile.h + 12),
                      t.metrics.cornerRadius + 6, withAlpha(t.colors.accent, 0x40));
    widgets::drawCover(ctx, tile, game, game.missing ? 150 : 255, t.metrics.cornerRadius, true);
    r.strokeRoundedRect(Rect(tile.x - 3, tile.y - 3, tile.w + 6, tile.h + 6),
                        t.metrics.cornerRadius + 3, 3, t.colors.accent);
    if (game.favorite) {
      Rect badge(tile.right() - 34, tile.y + 8, 26, 26);
      r.fillRoundedRect(badge, 13, makeColor(0, 0, 0, 0x90));
      text::drawAligned(r, f.bold, 17, badge.x, badge.y + 2, badge.w, "\xE2\x98\x85",
                        t.colors.favorite, text::kAlignCenter);
    }
  }
  r.popClip();

  // Indicador de rolagem.
  int rows = (static_cast<int>(results_.size()) + g.columns - 1) / g.columns;
  int contentH = rows * rowH - g.spacing;
  if (contentH > g.height) {
    int trackX = kScreenWidth - t.metrics.safeMarginX + 18;
    Rect track(trackX, g.top, 4, g.height);
    r.fillRoundedRect(track, 2, withAlpha(t.colors.text, 0x22));
    int thumbH = g.height * g.height / contentH;
    if (thumbH < 24) thumbH = 24;
    int maxScroll = contentH - g.height;
    int thumbY = g.top + (g.height - thumbH) * (scroll > maxScroll ? maxScroll : scroll) / maxScroll;
    r.fillRoundedRect(Rect(trackX, thumbY, 4, thumbH), 2, t.colors.accent);
  }
}

void LibraryScreen::renderFooter(UiContext& ctx) {
  const Theme& t = *ctx.theme;
  const Strings& s = *ctx.strings;
  FontSet& f = *ctx.fonts;
  int y = kScreenHeight - t.metrics.safeMarginY - (t.metrics.fontSmall + 10);
  if (selected_ < results_.size()) {
    const Game& game = app_.library().at(results_[selected_]);
    std::string info = s.get(category::labelKey(game.effectiveCategory()));
    if (game.titleId != 0) info += "  \xE2\x80\xA2  " + str::toHex32(game.titleId);
    text::draw(*ctx.renderer, f.regular, t.metrics.fontSmall, t.metrics.safeMarginX,
               y + (t.metrics.fontSmall + 10 - f.regular.lineHeight(t.metrics.fontSmall)) / 2,
               text::ellipsize(f.regular, t.metrics.fontSmall, info, 300), t.colors.textMuted);
  }
  std::vector<ButtonHint> hints;
  hints.push_back(ButtonHint("A", s.get("hint.open")));
  hints.push_back(ButtonHint("Y", s.get("hint.favorite")));
  hints.push_back(ButtonHint("X", s.get("hint.search")));
  if (!search_.empty()) hints.push_back(ButtonHint("B", s.get("hint.clearSearch")));
  hints.push_back(ButtonHint("BACK", s.get("hint.sort")));
  hints.push_back(ButtonHint("START", s.get("hint.menu")));
  widgets::drawButtonHints(ctx, hints);
}

void LibraryScreen::render(UiContext& ctx) {
  if (stale()) refresh();
  GridLayout g = layout(&ctx);
  widgets::drawBackground(ctx);
  renderHeader(ctx, g);
  renderGrid(ctx, g);
  renderFooter(ctx);
}

}  // namespace nebula
