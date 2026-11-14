// Interface de desenho 2D. A interface do Nébula trabalha sempre em
// coordenadas lógicas de 1280x720 (720p, resolução nativa do dashboard).
//
// Implementações:
//  - SoftwareRenderer: desenha num framebuffer de memória (PC, testes e
//    primeira versão no Xbox 360, onde o framebuffer é enviado à GPU).
//  - Futuro: renderizador Direct3D 9 do Xbox 360 (ver platform/xbox360).
#pragma once

#include "core/Color.h"
#include "core/Types.h"

namespace nebula {

struct Image;

const int kScreenWidth = 1280;
const int kScreenHeight = 720;

struct Rect {
  int x, y, w, h;
  Rect() : x(0), y(0), w(0), h(0) {}
  Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
  int right() const { return x + w; }
  int bottom() const { return y + h; }
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(int px, int py) const { return px >= x && py >= y && px < right() && py < bottom(); }
  Rect intersect(const Rect& o) const;
  Rect inset(int d) const { return Rect(x + d, y + d, w - 2 * d, h - 2 * d); }
  Rect offset(int dx, int dy) const { return Rect(x + dx, y + dy, w, h); }
};

class IRenderer {
 public:
  virtual ~IRenderer() {}
  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual void clear(Color color) = 0;
  virtual void fillRect(const Rect& rect, Color color) = 0;
  virtual void fillRoundedRect(const Rect& rect, int radius, Color color) = 0;
  virtual void strokeRoundedRect(const Rect& rect, int radius, int thickness, Color color) = 0;
  virtual void fillVerticalGradient(const Rect& rect, Color top, Color bottom) = 0;
  // Gradiente vertical recortado por cantos arredondados (do retângulo "shape").
  virtual void fillRoundedVerticalGradient(const Rect& rect, const Rect& shape, int radius,
                                           Color top, Color bottom) = 0;
  // Desenha "src" da imagem em "dst" (escala), com alfa global e cantos arredondados.
  virtual void drawImage(const Image& image, const Rect& src, const Rect& dst, uint8_t alpha,
                         int cornerRadius) = 0;
  // Máscara de cobertura 8 bits (glifos de texto) pintada com "color".
  virtual void drawAlphaMask(const uint8_t* mask, int width, int height, int stride, int x,
                             int y, Color color) = 0;

  virtual void pushClip(const Rect& rect) = 0;
  virtual void popClip() = 0;
};

}  // namespace nebula
