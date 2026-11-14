// Tela principal: grade de jogos com abas de categoria, pesquisa e favoritos.
//
// Controles: direcional/analógico = mover; LB/RB = categoria; LT/RT = página;
// A = detalhes; Y = favorito; X = pesquisar; B = limpar pesquisa;
// Back = ordenação; Start = menu.
#pragma once

#include <string>
#include <vector>

#include "ui/Screen.h"

namespace nebula {

class LibraryScreen : public Screen {
 public:
  static const char* const kTabAll;        // ""
  static const char* const kTabFavorites;  // "favorites"

  explicit LibraryScreen(IAppController& app);

  virtual const char* name() const NB_OVERRIDE { return "library"; }
  virtual void handleAction(Action action) NB_OVERRIDE;
  virtual void update(uint64_t nowMs) NB_OVERRIDE;
  virtual void render(UiContext& ctx) NB_OVERRIDE;

  void setSearch(const std::string& text);
  const std::string& search() const { return search_; }
  // Para testes e para outras telas.
  const std::vector<size_t>& visibleGames() const { return results_; }
  const std::vector<std::string>& tabs() const { return tabs_; }
  const std::string& currentTab() const;
  size_t selectedIndex() const { return selected_; }
  std::string selectedGameId() const;
  void invalidate() { dirty_ = true; }
  void refresh();
  // A biblioteca, a ordenação ou o filtro de ausentes mudaram desde o último refresh?
  bool stale() const;

 private:
  struct GridLayout {
    int columns;
    int tileW;
    int tileH;
    int spacing;
    int originX;
    int top;
    int height;
    int visibleRows;
  };
  GridLayout layout(const UiContext* ctx) const;
  void move(int dx, int dy);
  void changeTab(int delta);
  void ensureVisible();
  void renderHeader(UiContext& ctx, const GridLayout& g);
  void renderGrid(UiContext& ctx, const GridLayout& g);
  void renderFooter(UiContext& ctx);

  std::vector<std::string> tabs_;
  size_t tab_;
  std::string search_;
  std::vector<size_t> results_;
  size_t selected_;
  float scroll_;        // deslocamento atual (px)
  float scrollTarget_;  // deslocamento desejado (px)
  uint32_t seenRevision_;
  std::string seenSort_;
  bool seenShowMissing_;
  bool dirty_;
  uint64_t lastUpdateMs_;
};

}  // namespace nebula
