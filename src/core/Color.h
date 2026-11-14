// Cor 32 bits no formato 0xAARRGGBB (o mesmo de D3DCOLOR no Xbox 360).
#pragma once

#include <string>

#include "core/Types.h"

namespace nebula {

typedef uint32_t Color;

inline Color makeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}
inline uint8_t colorA(Color c) { return static_cast<uint8_t>(c >> 24); }
inline uint8_t colorR(Color c) { return static_cast<uint8_t>(c >> 16); }
inline uint8_t colorG(Color c) { return static_cast<uint8_t>(c >> 8); }
inline uint8_t colorB(Color c) { return static_cast<uint8_t>(c); }
inline Color withAlpha(Color c, uint8_t a) {
  return (c & 0x00FFFFFFu) | (static_cast<uint32_t>(a) << 24);
}
// Interpola entre a e b (t de 0 a 255).
Color lerpColor(Color a, Color b, int t);

// Aceita "#RRGGBB" ou "#RRGGBBAA".
bool parseColor(const std::string& text, Color& out);
// Gera "#RRGGBB" (alfa 255) ou "#RRGGBBAA".
std::string colorToString(Color c);

}  // namespace nebula
