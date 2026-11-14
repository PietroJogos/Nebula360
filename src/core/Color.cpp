#include "core/Color.h"

#include "core/StringUtil.h"

namespace nebula {

Color lerpColor(Color a, Color b, int t) {
  if (t <= 0) return a;
  if (t >= 255) return b;
  int inv = 255 - t;
  uint32_t ca = (colorA(a) * inv + colorA(b) * t) / 255;
  uint32_t cr = (colorR(a) * inv + colorR(b) * t) / 255;
  uint32_t cg = (colorG(a) * inv + colorG(b) * t) / 255;
  uint32_t cb = (colorB(a) * inv + colorB(b) * t) / 255;
  return (ca << 24) | (cr << 16) | (cg << 8) | cb;
}

bool parseColor(const std::string& text, Color& out) {
  std::string s = str::trim(text);
  if (s.empty() || s[0] != '#') return false;
  s = s.substr(1);
  if ((s.size() != 6 && s.size() != 8) || !str::isHexString(s)) return false;
  uint32_t v = 0;
  if (!str::parseHex32(s, v)) return false;
  if (s.size() == 6) {
    out = 0xFF000000u | v;
  } else {
    // RRGGBBAA -> AARRGGBB
    out = ((v & 0xFFu) << 24) | (v >> 8);
  }
  return true;
}

std::string colorToString(Color c) {
  std::string rgb = str::toHex32(c & 0x00FFFFFFu).substr(2);
  if (colorA(c) == 255) return "#" + rgb;
  std::string a = str::toHex32(colorA(c)).substr(6);
  return "#" + rgb + a;
}

}  // namespace nebula
