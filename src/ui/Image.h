// Imagem RGBA na memória (pixels 0xAARRGGBB, alfa não pré-multiplicado).
#pragma once

#include <string>
#include <vector>

#include "core/Color.h"
#include "core/Status.h"

namespace nebula {

struct Image {
  int width;
  int height;
  std::vector<Color> pixels;
  Image() : width(0), height(0) {}
  Image(int w, int h, Color fill) : width(w), height(h), pixels(static_cast<size_t>(w) * h, fill) {}
  bool empty() const { return width <= 0 || height <= 0; }
  Color at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

const int kMaxImageDimension = 4096;

// Decodifica PNG/JPEG (stb_image). Rejeita imagens maiores que 4096x4096.
Status decodeImage(const std::string& bytes, Image& out);
// Redução por média de área (boa qualidade para miniaturas).
Image downscale(const Image& src, int width, int height);
// Reduz para cobrir w x h mantendo a proporção (só reduz, nunca amplia).
Image downscaleToCover(const Image& src, int width, int height);
// Codifica PNG (stb_image_write).
Status encodePng(const Image& image, std::string& out);

}  // namespace nebula
