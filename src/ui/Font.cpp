#include "ui/Font.h"

#include "core/StringUtil.h"
#include "platform/FileSystem.h"
#include "ui/Renderer.h"

extern "C" {
#include "stb_truetype.h"
}

namespace nebula {

namespace {

const uint32_t kReplacement = '?';
const uint32_t kEllipsis = 0x2026;

stbtt_fontinfo* asInfo(void* p) { return static_cast<stbtt_fontinfo*>(p); }

uint32_t cacheKey(uint32_t cp, int px) {
  return (static_cast<uint32_t>(px & 0x7FF) << 21) | (cp & 0x1FFFFF);
}

}  // namespace

Font::Font() : info_(NULL) {}

Font::~Font() { delete asInfo(info_); }

Status Font::loadFromMemory(const std::string& data) {
  delete asInfo(info_);
  info_ = NULL;
  cache_.clear();
  data_ = data;
  if (data_.size() < 12) return Status(kParseError, "arquivo de fonte inválido");
  const unsigned char* p = reinterpret_cast<const unsigned char*>(data_.data());
  int offset = stbtt_GetFontOffsetForIndex(p, 0);
  if (offset < 0) return Status(kParseError, "fonte sem face utilizável");
  stbtt_fontinfo* info = new stbtt_fontinfo();
  if (!stbtt_InitFont(info, p, offset)) {
    delete info;
    return Status(kParseError, "stb_truetype não conseguiu ler a fonte");
  }
  info_ = info;
  return Status::Ok();
}

Status Font::loadFromFile(IFileSystem& fs, const std::string& path) {
  std::string data;
  Status s = fs.readFile(path, data, 8 * 1024 * 1024);
  if (!s.ok()) return s;
  return loadFromMemory(data).withContext(path);
}

bool Font::hasGlyph(uint32_t cp) const {
  return info_ != NULL && stbtt_FindGlyphIndex(asInfo(info_), static_cast<int>(cp)) != 0;
}

const Glyph& Font::glyph(uint32_t cp, int px) {
  static const Glyph kEmpty;
  if (info_ == NULL || px <= 0) return kEmpty;
  uint32_t key = cacheKey(cp, px);
  std::map<uint32_t, Glyph>::iterator it = cache_.find(key);
  if (it != cache_.end()) return it->second;

  stbtt_fontinfo* info = asInfo(info_);
  int index = stbtt_FindGlyphIndex(info, static_cast<int>(cp));
  if (index == 0 && cp != kReplacement && cp > 0x20) {
    index = stbtt_FindGlyphIndex(info, static_cast<int>(kReplacement));
  }
  float scale = stbtt_ScaleForPixelHeight(info, static_cast<float>(px));
  Glyph g;
  g.index = index;
  int advance = 0, lsb = 0;
  stbtt_GetGlyphHMetrics(info, index, &advance, &lsb);
  g.advance = static_cast<int>(advance * scale * 64.0f + 0.5f);
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  stbtt_GetGlyphBitmapBox(info, index, scale, scale, &x0, &y0, &x1, &y1);
  g.width = x1 - x0;
  g.height = y1 - y0;
  g.xoff = x0;
  g.yoff = y0;
  if (g.width > 0 && g.height > 0 && g.width < 1024 && g.height < 1024) {
    g.bitmap.resize(static_cast<size_t>(g.width) * g.height);
    stbtt_MakeGlyphBitmap(info, &g.bitmap[0], g.width, g.height, g.width, scale, scale, index);
  } else {
    g.width = g.height = 0;
  }
  return cache_.insert(std::make_pair(key, g)).first->second;
}

int Font::ascent(int px) const {
  if (info_ == NULL) return 0;
  int a = 0, d = 0, gap = 0;
  stbtt_GetFontVMetrics(asInfo(info_), &a, &d, &gap);
  return static_cast<int>(a * stbtt_ScaleForPixelHeight(asInfo(info_), static_cast<float>(px)) + 0.5f);
}

int Font::lineHeight(int px) const {
  if (info_ == NULL) return px;
  int a = 0, d = 0, gap = 0;
  stbtt_GetFontVMetrics(asInfo(info_), &a, &d, &gap);
  float scale = stbtt_ScaleForPixelHeight(asInfo(info_), static_cast<float>(px));
  return static_cast<int>((a - d + gap) * scale + 0.5f);
}

int Font::kerning(const Glyph& a, const Glyph& b, int px) const {
  if (info_ == NULL) return 0;
  int k = stbtt_GetGlyphKernAdvance(asInfo(info_), a.index, b.index);
  if (k == 0) return 0;
  return static_cast<int>(k * stbtt_ScaleForPixelHeight(asInfo(info_), static_cast<float>(px)) * 64.0f);
}

namespace text {

int measure(Font& font, int px, const std::string& s) {
  int pen = 0;
  size_t pos = 0;
  uint32_t cp = 0;
  const Glyph* prev = NULL;
  while (str::decodeUtf8(s, pos, cp)) {
    const Glyph& g = font.glyph(cp, px);
    if (prev) pen += font.kerning(*prev, g, px);
    pen += g.advance;
    prev = &g;
  }
  return (pen + 63) / 64;
}

void draw(IRenderer& r, Font& font, int px, int x, int y, const std::string& s, Color color) {
  int baseline = y + font.ascent(px);
  int pen = x * 64;
  size_t pos = 0;
  uint32_t cp = 0;
  const Glyph* prev = NULL;
  while (str::decodeUtf8(s, pos, cp)) {
    const Glyph& g = font.glyph(cp, px);
    if (prev) pen += font.kerning(*prev, g, px);
    if (!g.bitmap.empty()) {
      r.drawAlphaMask(&g.bitmap[0], g.width, g.height, g.width, (pen + 32) / 64 + g.xoff,
                      baseline + g.yoff, color);
    }
    pen += g.advance;
    prev = &g;
  }
}

std::string ellipsize(Font& font, int px, const std::string& s, int maxWidth) {
  if (measure(font, px, s) <= maxWidth) return s;
  std::string ell;
  str::appendUtf8(ell, kEllipsis);
  int ellWidth = measure(font, px, ell);
  std::string out;
  size_t pos = 0;
  uint32_t cp = 0;
  while (str::decodeUtf8(s, pos, cp)) {
    std::string next = out;
    str::appendUtf8(next, cp);
    if (measure(font, px, next) + ellWidth > maxWidth) break;
    out = next;
  }
  return str::trim(out) + ell;
}

std::vector<std::string> wrap(Font& font, int px, const std::string& s, int maxWidth,
                              size_t maxLines) {
  std::vector<std::string> lines;
  std::vector<std::string> words = str::splitWhitespace(s);
  std::string line;
  size_t i = 0;
  for (; i < words.size(); ++i) {
    std::string candidate = line.empty() ? words[i] : line + " " + words[i];
    if (line.empty() || measure(font, px, candidate) <= maxWidth) {
      line = candidate;
      continue;
    }
    lines.push_back(line);
    line = words[i];
    if (maxLines > 0 && lines.size() == maxLines) {
      line.clear();  // a palavra i não cabe mais
      break;
    }
  }
  bool truncated = maxLines > 0 && i < words.size();
  if (!line.empty()) {
    if (maxLines == 0 || lines.size() < maxLines) {
      lines.push_back(line);
    } else {
      truncated = true;
    }
  }
  for (size_t k = 0; k < lines.size(); ++k) lines[k] = ellipsize(font, px, lines[k], maxWidth);
  if (truncated && !lines.empty()) {
    std::string ell;
    str::appendUtf8(ell, kEllipsis);
    std::string& last = lines[lines.size() - 1];
    if (!str::endsWith(last, ell)) {
      // Remove caracteres do fim até "linha…" caber.
      std::string base = last;
      while (!base.empty() && measure(font, px, base + ell) > maxWidth) {
        size_t cut = base.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(base[cut]) & 0xC0) == 0x80) --cut;
        base.erase(cut);
      }
      last = str::trim(base) + ell;
    }
  }
  return lines;
}

void drawAligned(IRenderer& r, Font& font, int px, int x, int y, int width,
                 const std::string& s, Color color, Align align) {
  int w = measure(font, px, s);
  int dx = x;
  if (align == kAlignCenter) dx = x + (width - w) / 2;
  if (align == kAlignRight) dx = x + width - w;
  draw(r, font, px, dx, y, s, color);
}

}  // namespace text
}  // namespace nebula
