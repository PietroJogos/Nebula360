#include "ui/SoftwareRenderer.h"

#include <cmath>

namespace nebula {

Rect Rect::intersect(const Rect& o) const {
  int x0 = x > o.x ? x : o.x;
  int y0 = y > o.y ? y : o.y;
  int x1 = right() < o.right() ? right() : o.right();
  int y1 = bottom() < o.bottom() ? bottom() : o.bottom();
  if (x1 <= x0 || y1 <= y0) return Rect(x0, y0, 0, 0);
  return Rect(x0, y0, x1 - x0, y1 - y0);
}

namespace {

inline uint32_t mix(uint32_t s, uint32_t d, uint32_t a) {
  return (s * a + d * (255 - a) + 127) / 255;
}

inline Color blend(Color dst, Color src, uint32_t a) {
  if (a == 0) return dst;
  if (a >= 255) return src | 0xFF000000u;
  return 0xFF000000u | (mix(colorR(src), colorR(dst), a) << 16) |
         (mix(colorG(src), colorG(dst), a) << 8) | mix(colorB(src), colorB(dst), a);
}

int clampRadius(const Rect& r, int radius) {
  int m = (r.w < r.h ? r.w : r.h) / 2;
  if (radius > m) radius = m;
  return radius < 0 ? 0 : radius;
}

}  // namespace

uint32_t roundedRectCoverage(const Rect& rect, int radius, int px, int py) {
  if (!rect.contains(px, py)) return 0;
  int r = clampRadius(rect, radius);
  if (r == 0) return 255;
  float cx = 0, cy = 0;
  bool cornerX = false, cornerY = false;
  if (px < rect.x + r) {
    cx = static_cast<float>(rect.x + r);
    cornerX = true;
  } else if (px >= rect.right() - r) {
    cx = static_cast<float>(rect.right() - r);
    cornerX = true;
  }
  if (py < rect.y + r) {
    cy = static_cast<float>(rect.y + r);
    cornerY = true;
  } else if (py >= rect.bottom() - r) {
    cy = static_cast<float>(rect.bottom() - r);
    cornerY = true;
  }
  if (!cornerX || !cornerY) return 255;
  float dx = (px + 0.5f) - cx;
  float dy = (py + 0.5f) - cy;
  float cov = static_cast<float>(r) - std::sqrt(dx * dx + dy * dy) + 0.5f;
  if (cov <= 0.0f) return 0;
  if (cov >= 1.0f) return 255;
  return static_cast<uint32_t>(cov * 255.0f);
}

SoftwareRenderer::SoftwareRenderer(int width, int height)
    : fb_(width, height, 0xFF000000u) {
  clips_.push_back(Rect(0, 0, width, height));
}

void SoftwareRenderer::blendPixel(int x, int y, Color color, uint32_t coverage) {
  uint32_t a = colorA(color) * coverage / 255;
  Color& dst = fb_.pixels[static_cast<size_t>(y) * fb_.width + x];
  dst = blend(dst, color, a);
}

void SoftwareRenderer::pushClip(const Rect& rect) { clips_.push_back(clip().intersect(rect)); }

void SoftwareRenderer::popClip() {
  if (clips_.size() > 1) clips_.pop_back();
}

void SoftwareRenderer::clear(Color color) {
  Color c = color | 0xFF000000u;
  for (size_t i = 0; i < fb_.pixels.size(); ++i) fb_.pixels[i] = c;
}

void SoftwareRenderer::fillRect(const Rect& rect, Color color) {
  Rect r = rect.intersect(clip());
  if (r.empty() || colorA(color) == 0) return;
  uint32_t a = colorA(color);
  for (int y = r.y; y < r.bottom(); ++y) {
    Color* row = &fb_.pixels[static_cast<size_t>(y) * fb_.width];
    if (a == 255) {
      for (int x = r.x; x < r.right(); ++x) row[x] = color;
    } else {
      for (int x = r.x; x < r.right(); ++x) row[x] = blend(row[x], color, a);
    }
  }
}

void SoftwareRenderer::fillRoundedRect(const Rect& rect, int radius, Color color) {
  int rad = clampRadius(rect, radius);
  if (rad == 0) {
    fillRect(rect, color);
    return;
  }
  Rect r = rect.intersect(clip());
  if (r.empty() || colorA(color) == 0) return;
  for (int y = r.y; y < r.bottom(); ++y) {
    bool middle = y >= rect.y + rad && y < rect.bottom() - rad;
    if (middle) {
      fillRect(Rect(r.x, y, r.w, 1), color);
      continue;
    }
    for (int x = r.x; x < r.right(); ++x) {
      uint32_t cov = roundedRectCoverage(rect, rad, x, y);
      if (cov) blendPixel(x, y, color, cov);
    }
  }
}

void SoftwareRenderer::strokeRoundedRect(const Rect& rect, int radius, int thickness,
                                         Color color) {
  if (thickness <= 0) return;
  int rad = clampRadius(rect, radius);
  Rect inner = rect.inset(thickness);
  int innerRad = rad - thickness < 0 ? 0 : rad - thickness;
  Rect r = rect.intersect(clip());
  if (r.empty()) return;
  for (int y = r.y; y < r.bottom(); ++y) {
    bool innerMiddle = !inner.empty() && y >= inner.y + innerRad && y < inner.bottom() - innerRad;
    int leftEnd = innerMiddle ? inner.x + innerRad : r.right();
    int rightStart = innerMiddle ? inner.right() - innerRad : r.right();
    for (int x = r.x; x < r.right(); ++x) {
      if (innerMiddle && x >= leftEnd && x < rightStart) {
        x = rightStart - 1;
        continue;
      }
      uint32_t outer = roundedRectCoverage(rect, rad, x, y);
      uint32_t in = inner.empty() ? 0 : roundedRectCoverage(inner, innerRad, x, y);
      if (outer > in) blendPixel(x, y, color, outer - in);
    }
  }
}

void SoftwareRenderer::fillVerticalGradient(const Rect& rect, Color top, Color bottom) {
  Rect r = rect.intersect(clip());
  if (r.empty() || rect.h <= 0) return;
  for (int y = r.y; y < r.bottom(); ++y) {
    int t = rect.h > 1 ? (y - rect.y) * 255 / (rect.h - 1) : 0;
    fillRect(Rect(r.x, y, r.w, 1), lerpColor(top, bottom, t));
  }
}

void SoftwareRenderer::fillRoundedVerticalGradient(const Rect& rect, const Rect& shape,
                                                   int radius, Color top, Color bottom) {
  Rect r = rect.intersect(clip()).intersect(shape);
  if (r.empty() || rect.h <= 0) return;
  int rad = clampRadius(shape, radius);
  for (int y = r.y; y < r.bottom(); ++y) {
    int t = rect.h > 1 ? (y - rect.y) * 255 / (rect.h - 1) : 0;
    Color c = lerpColor(top, bottom, t);
    bool cornerRow = rad > 0 && (y < shape.y + rad || y >= shape.bottom() - rad);
    if (!cornerRow) {
      fillRect(Rect(r.x, y, r.w, 1), c);
      continue;
    }
    for (int x = r.x; x < r.right(); ++x) {
      uint32_t cov = roundedRectCoverage(shape, rad, x, y);
      if (cov) blendPixel(x, y, c, cov);
    }
  }
}

void SoftwareRenderer::drawImage(const Image& image, const Rect& srcIn, const Rect& dst,
                                 uint8_t alpha, int cornerRadius) {
  if (image.empty() || dst.empty() || alpha == 0) return;
  Rect src = srcIn.intersect(Rect(0, 0, image.width, image.height));
  if (src.empty()) return;
  Rect r = dst.intersect(clip());
  if (r.empty()) return;
  int rad = clampRadius(dst, cornerRadius);
  // Passo em ponto fixo 16.16.
  int64_t stepX = (static_cast<int64_t>(src.w) << 16) / dst.w;
  int64_t stepY = (static_cast<int64_t>(src.h) << 16) / dst.h;
  for (int y = r.y; y < r.bottom(); ++y) {
    int sy = src.y + static_cast<int>(((y - dst.y) * stepY) >> 16);
    if (sy >= src.bottom()) sy = src.bottom() - 1;
    const Color* srow = &image.pixels[static_cast<size_t>(sy) * image.width];
    Color* drow = &fb_.pixels[static_cast<size_t>(y) * fb_.width];
    bool cornerRow = rad > 0 && (y < dst.y + rad || y >= dst.bottom() - rad);
    for (int x = r.x; x < r.right(); ++x) {
      int sx = src.x + static_cast<int>(((x - dst.x) * stepX) >> 16);
      if (sx >= src.right()) sx = src.right() - 1;
      Color c = srow[sx];
      uint32_t a = colorA(c) * alpha / 255;
      if (cornerRow) a = a * roundedRectCoverage(dst, rad, x, y) / 255;
      drow[x] = blend(drow[x], c, a);
    }
  }
}

void SoftwareRenderer::drawAlphaMask(const uint8_t* mask, int width, int height, int stride,
                                     int x, int y, Color color) {
  if (mask == NULL || width <= 0 || height <= 0) return;
  Rect r = Rect(x, y, width, height).intersect(clip());
  if (r.empty()) return;
  uint32_t ca = colorA(color);
  for (int py = r.y; py < r.bottom(); ++py) {
    const uint8_t* mrow = mask + static_cast<size_t>(py - y) * stride;
    Color* drow = &fb_.pixels[static_cast<size_t>(py) * fb_.width];
    for (int px = r.x; px < r.right(); ++px) {
      uint32_t m = mrow[px - x];
      if (m == 0) continue;
      drow[px] = blend(drow[px], color, m * ca / 255);
    }
  }
}

}  // namespace nebula
