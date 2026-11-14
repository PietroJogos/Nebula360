// Renderizador por software sobre um framebuffer 0xAARRGGBB.
#pragma once

#include <vector>

#include "ui/Image.h"
#include "ui/Renderer.h"

namespace nebula {

class SoftwareRenderer : public IRenderer {
 public:
  SoftwareRenderer(int width = kScreenWidth, int height = kScreenHeight);

  const Image& framebuffer() const { return fb_; }
  Image& framebuffer() { return fb_; }

  virtual int width() const NB_OVERRIDE { return fb_.width; }
  virtual int height() const NB_OVERRIDE { return fb_.height; }
  virtual void clear(Color color) NB_OVERRIDE;
  virtual void fillRect(const Rect& rect, Color color) NB_OVERRIDE;
  virtual void fillRoundedRect(const Rect& rect, int radius, Color color) NB_OVERRIDE;
  virtual void strokeRoundedRect(const Rect& rect, int radius, int thickness,
                                 Color color) NB_OVERRIDE;
  virtual void fillVerticalGradient(const Rect& rect, Color top, Color bottom) NB_OVERRIDE;
  virtual void fillRoundedVerticalGradient(const Rect& rect, const Rect& shape, int radius,
                                           Color top, Color bottom) NB_OVERRIDE;
  virtual void drawImage(const Image& image, const Rect& src, const Rect& dst, uint8_t alpha,
                         int cornerRadius) NB_OVERRIDE;
  virtual void drawAlphaMask(const uint8_t* mask, int width, int height, int stride, int x,
                             int y, Color color) NB_OVERRIDE;
  virtual void pushClip(const Rect& rect) NB_OVERRIDE;
  virtual void popClip() NB_OVERRIDE;

 private:
  void blendPixel(int x, int y, Color color, uint32_t coverage);
  const Rect& clip() const { return clips_[clips_.size() - 1]; }

  Image fb_;
  std::vector<Rect> clips_;
};

// Cobertura (0..255) de um pixel dentro de um retângulo de cantos arredondados.
uint32_t roundedRectCoverage(const Rect& rect, int radius, int px, int py);

}  // namespace nebula
