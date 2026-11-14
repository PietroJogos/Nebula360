// Fontes TrueType/OpenType via stb_truetype, com cache de glifos.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Color.h"
#include "core/Status.h"

namespace nebula {

class IRenderer;
class IFileSystem;

struct Glyph {
  int width;
  int height;
  int xoff;
  int yoff;      // em relação à linha de base
  int advance;   // em 1/64 de pixel
  int index;     // índice do glifo na fonte (kerning)
  std::vector<uint8_t> bitmap;
  Glyph() : width(0), height(0), xoff(0), yoff(0), advance(0), index(0) {}
};

class Font {
 public:
  Font();
  ~Font();

  Status loadFromMemory(const std::string& data);
  Status loadFromFile(IFileSystem& fs, const std::string& path);
  bool loaded() const { return info_ != NULL; }

  bool hasGlyph(uint32_t codepoint) const;
  const Glyph& glyph(uint32_t codepoint, int pixelHeight);
  int ascent(int pixelHeight) const;
  int lineHeight(int pixelHeight) const;
  // Kerning entre dois glifos, em 1/64 de pixel.
  int kerning(const Glyph& a, const Glyph& b, int pixelHeight) const;
  size_t cachedGlyphs() const { return cache_.size(); }

 private:
  std::string data_;
  void* info_;  // stbtt_fontinfo (opaco para não vazar o cabeçalho C)
  std::map<uint32_t, Glyph> cache_;
  NB_NONCOPYABLE(Font);
};

struct FontSet {
  Font regular;
  Font bold;
};

namespace text {

int measure(Font& font, int px, const std::string& utf8);
// y = topo da linha de texto.
void draw(IRenderer& r, Font& font, int px, int x, int y, const std::string& utf8, Color color);
// Corta com "…" para caber em maxWidth.
std::string ellipsize(Font& font, int px, const std::string& utf8, int maxWidth);
// Quebra em linhas por palavras; a última linha recebe "…" se sobrar texto.
std::vector<std::string> wrap(Font& font, int px, const std::string& utf8, int maxWidth,
                              size_t maxLines);

enum Align { kAlignLeft, kAlignCenter, kAlignRight };
void drawAligned(IRenderer& r, Font& font, int px, int x, int y, int width,
                 const std::string& utf8, Color color, Align align);

}  // namespace text

}  // namespace nebula
