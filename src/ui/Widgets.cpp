#include "ui/Widgets.h"

#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/Game.h"
#include "theme/Theme.h"
#include "ui/Font.h"
#include "ui/Image.h"
#include "ui/Strings.h"
#include "ui/TextureCache.h"

namespace nebula {
namespace widgets {

namespace {

const Color kWhite = 0xFFFFFFFFu;

Color hsv(int hue, int sat, int val) {
  // hue 0..359, sat/val 0..255
  int region = (hue % 360) / 60;
  int rem = ((hue % 360) - region * 60) * 255 / 60;
  int p = (val * (255 - sat)) / 255;
  int q = (val * (255 - (sat * rem) / 255)) / 255;
  int t = (val * (255 - (sat * (255 - rem)) / 255)) / 255;
  int r = 0, g = 0, b = 0;
  switch (region) {
    case 0: r = val; g = t; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = t; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = t; g = p; b = val; break;
    default: r = val; g = p; b = q; break;
  }
  return makeColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

uint32_t seedFrom(const std::string& s) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (size_t i = 0; i < s.size(); ++i) {
    h ^= static_cast<unsigned char>(s[i]);
    h *= 16777619u;
  }
  return h;
}

std::string initials(const std::string& title) {
  std::vector<std::string> words = str::splitWhitespace(title);
  std::string out;
  for (size_t i = 0; i < words.size() && str::utf8Length(out) < 2; ++i) {
    size_t pos = 0;
    uint32_t cp = 0;
    if (str::decodeUtf8(words[i], pos, cp)) {
      if (cp < 0x80 && !((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
                         (cp >= '0' && cp <= '9'))) {
        continue;  // pula pontuação
      }
      if (cp >= 'a' && cp <= 'z') cp = cp - 'a' + 'A';
      str::appendUtf8(out, cp);
    }
  }
  return out.empty() ? "?" : out;
}

Rect centerCrop(int imgW, int imgH, int dstW, int dstH) {
  // Recorte central com a proporção do destino.
  int64_t lhs = static_cast<int64_t>(imgW) * dstH;
  int64_t rhs = static_cast<int64_t>(imgH) * dstW;
  if (lhs > rhs) {
    int w = static_cast<int>(rhs / dstH);
    return Rect((imgW - w) / 2, 0, w, imgH);
  }
  int h = static_cast<int>(lhs / (dstW > 0 ? dstW : 1));
  return Rect(0, (imgH - h) / 2, imgW, h);
}

void drawPlaceholder(UiContext& ctx, const Rect& rect, const Game& game, uint8_t alpha,
                     int radius, bool withInitials) {
  IRenderer& r = *ctx.renderer;
  uint32_t seed = seedFrom(game.id.empty() ? game.title : game.id);
  int hue = static_cast<int>(seed % 360);
  Color top = withAlpha(hsv(hue, 150, 120), alpha);
  Color bottom = withAlpha(hsv((hue + 40) % 360, 200, 45), alpha);
  r.fillRoundedVerticalGradient(rect, rect, radius, top, bottom);
  // "Órbita" decorativa, inteira dentro do bloco (não invade os cantos arredondados).
  int d = rect.w * 11 / 20;
  Rect orbit(rect.right() - d - rect.w / 12, rect.y + rect.w / 12, d, d);
  r.fillRoundedRect(orbit, d / 2, withAlpha(kWhite, static_cast<uint8_t>(alpha / 14)));
  if (!withInitials) return;
  FontSet& f = *ctx.fonts;
  int px = rect.w / 3;
  std::string ini = initials(game.title);
  text::drawAligned(r, f.bold, px, rect.x, rect.y + rect.h / 2 - px * 3 / 4, rect.w, ini,
                    withAlpha(kWhite, static_cast<uint8_t>(alpha * 9 / 10)), text::kAlignCenter);
}

}  // namespace

void drawBackground(UiContext& ctx) {
  const Theme& t = *ctx.theme;
  ctx.renderer->fillVerticalGradient(Rect(0, 0, ctx.renderer->width(), ctx.renderer->height()),
                                     t.colors.backgroundTop, t.colors.backgroundBottom);
}

int measureButtonBadge(UiContext& ctx, const char* button) {
  std::string b = button;
  int px = ctx.theme->metrics.fontSmall;
  if (b.size() == 1) return px + 10;
  return text::measure(ctx.fonts->bold, px - 3, b) + 16;
}

int drawButtonBadge(UiContext& ctx, int x, int y, const char* button) {
  IRenderer& r = *ctx.renderer;
  const Theme& t = *ctx.theme;
  std::string b = button;
  int px = t.metrics.fontSmall;
  int size = px + 10;
  if (b.size() == 1) {
    Color c = makeColor(0x6B, 0x72, 0x80);
    if (b == "A") c = makeColor(0x22, 0xC5, 0x5E);
    if (b == "B") c = makeColor(0xEF, 0x44, 0x44);
    if (b == "X") c = makeColor(0x3B, 0x82, 0xF6);
    if (b == "Y") c = makeColor(0xEA, 0xB3, 0x08);
    r.fillRoundedRect(Rect(x, y, size, size), size / 2, c);
    text::drawAligned(r, ctx.fonts->bold, px, x, y + (size - ctx.fonts->bold.lineHeight(px)) / 2 + 1,
                      size, b, kWhite, text::kAlignCenter);
    return size;
  }
  int w = measureButtonBadge(ctx, button);
  r.fillRoundedRect(Rect(x, y, w, size), 6, withAlpha(t.colors.text, 0x30));
  text::drawAligned(r, ctx.fonts->bold, px - 3, x, y + (size - ctx.fonts->bold.lineHeight(px - 3)) / 2,
                    w, b, t.colors.text, text::kAlignCenter);
  return w;
}

void drawButtonHintsAt(UiContext& ctx, const std::vector<ButtonHint>& hints, int rightX, int y) {
  const Theme& t = *ctx.theme;
  int px = t.metrics.fontSmall;
  int total = 0;
  for (size_t i = 0; i < hints.size(); ++i) {
    total += measureButtonBadge(ctx, hints[i].button) + 8 +
             text::measure(ctx.fonts->regular, px, hints[i].label);
    if (i + 1 < hints.size()) total += 22;
  }
  int x = rightX - total;
  int badgeH = px + 10;
  for (size_t i = 0; i < hints.size(); ++i) {
    x += drawButtonBadge(ctx, x, y, hints[i].button) + 8;
    text::draw(*ctx.renderer, ctx.fonts->regular, px, x,
               y + (badgeH - ctx.fonts->regular.lineHeight(px)) / 2, hints[i].label,
               t.colors.textMuted);
    x += text::measure(ctx.fonts->regular, px, hints[i].label) + 22;
  }
}

void drawButtonHints(UiContext& ctx, const std::vector<ButtonHint>& hints) {
  const ThemeMetrics& m = ctx.theme->metrics;
  int y = ctx.renderer->height() - m.safeMarginY - (m.fontSmall + 10);
  drawButtonHintsAt(ctx, hints, ctx.renderer->width() - m.safeMarginX, y);
}

void drawLogo(UiContext& ctx, int x, int y, int px) {
  const Theme& t = *ctx.theme;
  std::string name = "Nébula";
  text::draw(*ctx.renderer, ctx.fonts->bold, px, x, y, name, t.colors.text);
  int w = text::measure(ctx.fonts->bold, px, name);
  text::draw(*ctx.renderer, ctx.fonts->bold, px, x + w + px / 4, y, "360", t.colors.accent);
}

void drawCover(UiContext& ctx, const Rect& rect, const Game& game, uint8_t alpha, int radius,
               bool drawTitle) {
  IRenderer& r = *ctx.renderer;
  const Theme& t = *ctx.theme;
  const Image* img = NULL;
  if (!game.coverPath.empty()) {
    // Decodifica no tamanho do bloco padrão, ou maior se o destino for maior.
    int w = rect.w > t.metrics.tileWidth ? rect.w : t.metrics.tileWidth;
    int h = rect.h > t.metrics.tileHeight ? rect.h : t.metrics.tileHeight;
    img = ctx.textures->get(game.coverPath, w, h);
  }
  if (img != NULL && img->width >= rect.w / 2) {
    r.drawImage(*img, centerCrop(img->width, img->height, rect.w, rect.h), rect, alpha, radius);
  } else if (img != NULL) {
    // Ícone pequeno (ex.: miniatura 64x64 do contêiner): centraliza sobre o marcador.
    drawPlaceholder(ctx, rect, game, alpha, radius, false);
    int maxSide = rect.w * 3 / 5;
    int scale = maxSide / (img->width > 0 ? img->width : 1);
    if (scale < 1) scale = 1;
    int w = img->width * scale;
    int h = img->height * scale;
    if (w > maxSide) {
      h = h * maxSide / w;
      w = maxSide;
    }
    Rect dst(rect.x + (rect.w - w) / 2, rect.y + rect.h / 2 - h / 2 - rect.h / 10, w, h);
    r.drawImage(*img, Rect(0, 0, img->width, img->height), dst, alpha, 8);
  } else {
    drawPlaceholder(ctx, rect, game, alpha, radius, true);
  }

  if (!drawTitle) return;
  Rect shade(rect.x, rect.y + rect.h * 45 / 100, rect.w, rect.h - rect.h * 45 / 100);
  r.fillRoundedVerticalGradient(shade, rect, radius, 0x00000000u,
                                withAlpha(0xFF000000u, static_cast<uint8_t>(alpha * 220 / 255)));
  int px = t.metrics.fontSmall;
  std::vector<std::string> lines = text::wrap(ctx.fonts->bold, px, game.title, rect.w - 20, 2);
  int lh = ctx.fonts->bold.lineHeight(px);
  int y = rect.bottom() - 10 - static_cast<int>(lines.size()) * lh;
  for (size_t i = 0; i < lines.size(); ++i) {
    text::draw(r, ctx.fonts->bold, px, rect.x + 10, y + static_cast<int>(i) * lh, lines[i],
               withAlpha(kWhite, alpha));
  }
}

void drawPanel(UiContext& ctx, const Rect& rect) {
  const Theme& t = *ctx.theme;
  ctx.renderer->fillRoundedRect(rect, t.metrics.cornerRadius + 4,
                                lerpColor(t.colors.backgroundTop, t.colors.backgroundBottom, 128));
  ctx.renderer->fillRoundedRect(rect, t.metrics.cornerRadius + 4, t.colors.surface);
  ctx.renderer->strokeRoundedRect(rect, t.metrics.cornerRadius + 4, 1,
                                  withAlpha(t.colors.text, 0x22));
}

void dimBackground(UiContext& ctx) {
  ctx.renderer->fillRect(Rect(0, 0, ctx.renderer->width(), ctx.renderer->height()),
                         ctx.theme->colors.overlay);
}

}  // namespace widgets
}  // namespace nebula
