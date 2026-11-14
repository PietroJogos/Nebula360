// Componentes visuais reutilizáveis (todos desenhados pelo próprio Nébula).
#pragma once

#include <string>
#include <vector>

#include "core/Color.h"
#include "ui/Renderer.h"
#include "ui/Screen.h"

namespace nebula {

struct Game;

struct ButtonHint {
  const char* button;  // "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "START", "BACK"
  std::string label;
  ButtonHint(const char* b, const std::string& l) : button(b), label(l) {}
};

namespace widgets {

void drawBackground(UiContext& ctx);
// Desenha o selo de um botão do controle; retorna a largura usada.
int drawButtonBadge(UiContext& ctx, int x, int y, const char* button);
int measureButtonBadge(UiContext& ctx, const char* button);
// Dicas de botões alinhadas à direita na base da área segura.
void drawButtonHints(UiContext& ctx, const std::vector<ButtonHint>& hints);
void drawButtonHintsAt(UiContext& ctx, const std::vector<ButtonHint>& hints, int rightX, int y);
// "Nébula 360" no canto superior esquerdo.
void drawLogo(UiContext& ctx, int x, int y, int px);
// Capa do jogo (ou arte de marcador gerada) com cantos arredondados.
void drawCover(UiContext& ctx, const Rect& rect, const Game& game, uint8_t alpha, int radius,
               bool drawTitle);
void drawPanel(UiContext& ctx, const Rect& rect);
void dimBackground(UiContext& ctx);

}  // namespace widgets
}  // namespace nebula
