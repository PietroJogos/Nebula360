#include "core/StringUtil.h"

#include <sstream>

namespace nebula {
namespace str {

bool decodeUtf8(const std::string& s, size_t& pos, uint32_t& cp) {
  if (pos >= s.size()) return false;
  const unsigned char c0 = static_cast<unsigned char>(s[pos]);
  if (c0 < 0x80) {
    cp = c0;
    pos += 1;
    return true;
  }
  int extra = 0;
  uint32_t value = 0;
  uint32_t minValue = 0;
  if ((c0 & 0xE0) == 0xC0) { extra = 1; value = c0 & 0x1F; minValue = 0x80; }
  else if ((c0 & 0xF0) == 0xE0) { extra = 2; value = c0 & 0x0F; minValue = 0x800; }
  else if ((c0 & 0xF8) == 0xF0) { extra = 3; value = c0 & 0x07; minValue = 0x10000; }
  else {
    cp = 0xFFFD;
    pos += 1;
    return true;
  }
  for (int i = 1; i <= extra; ++i) {
    size_t p = pos + static_cast<size_t>(i);
    if (p >= s.size()) {
      cp = 0xFFFD;
      pos += 1;
      return true;
    }
    const unsigned char c = static_cast<unsigned char>(s[p]);
    if ((c & 0xC0) != 0x80) {
      cp = 0xFFFD;
      pos += 1;
      return true;
    }
    value = (value << 6) | (c & 0x3F);
  }
  if (value < minValue || value > 0x10FFFF ||
      (value >= 0xD800 && value <= 0xDFFF)) {
    cp = 0xFFFD;
    pos += 1;
    return true;
  }
  cp = value;
  pos += static_cast<size_t>(extra) + 1;
  return true;
}

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

size_t utf8Length(const std::string& s) {
  size_t pos = 0;
  size_t n = 0;
  uint32_t cp = 0;
  while (decodeUtf8(s, pos, cp)) ++n;
  return n;
}

std::string utf16beToUtf8(const uint8_t* data, size_t byteLength) {
  std::string out;
  size_t i = 0;
  while (i + 1 < byteLength) {
    uint32_t u = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
    i += 2;
    if (u == 0) break;
    if (u >= 0xD800 && u <= 0xDBFF) {
      if (i + 1 < byteLength) {
        uint32_t lo = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
        if (lo >= 0xDC00 && lo <= 0xDFFF) {
          i += 2;
          appendUtf8(out, 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00));
          continue;
        }
      }
      appendUtf8(out, 0xFFFD);
    } else if (u >= 0xDC00 && u <= 0xDFFF) {
      appendUtf8(out, 0xFFFD);
    } else {
      appendUtf8(out, u);
    }
  }
  return out;
}

namespace {

// U+00C0..U+00FF já em minúsculas e sem acento. "" = separador.
const char* const kLatin1Fold[64] = {
    "a", "a", "a", "a", "a", "a", "ae", "c",   // C0-C7
    "e", "e", "e", "e", "i", "i", "i", "i",    // C8-CF
    "d", "n", "o", "o", "o", "o", "o", "",     // D0-D7 (D7 = ×)
    "o", "u", "u", "u", "u", "y", "th", "ss",  // D8-DF
    "a", "a", "a", "a", "a", "a", "ae", "c",   // E0-E7
    "e", "e", "e", "e", "i", "i", "i", "i",    // E8-EF
    "d", "n", "o", "o", "o", "o", "o", "",     // F0-F7 (F7 = ÷)
    "o", "u", "u", "u", "u", "y", "th", "y"};  // F8-FF

struct FoldRange {
  uint32_t first;
  uint32_t last;
  const char* text;
};

// Latin Extended-A (U+0100..U+017F), agrupado por letra base.
const FoldRange kLatinExtAFold[] = {
    {0x100, 0x105, "a"}, {0x106, 0x10D, "c"}, {0x10E, 0x111, "d"},
    {0x112, 0x11B, "e"}, {0x11C, 0x123, "g"}, {0x124, 0x127, "h"},
    {0x128, 0x131, "i"}, {0x132, 0x133, "ij"}, {0x134, 0x135, "j"},
    {0x136, 0x138, "k"}, {0x139, 0x142, "l"}, {0x143, 0x14B, "n"},
    {0x14C, 0x151, "o"}, {0x152, 0x153, "oe"}, {0x154, 0x159, "r"},
    {0x15A, 0x161, "s"}, {0x162, 0x167, "t"}, {0x168, 0x173, "u"},
    {0x174, 0x175, "w"}, {0x176, 0x178, "y"}, {0x179, 0x17E, "z"},
    {0x17F, 0x17F, "s"}};

bool isAsciiAlnum(uint32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

void appendFolded(std::string& out, const char* text, bool& lastWasSpace) {
  if (text[0] == '\0') {
    if (!lastWasSpace && !out.empty()) {
      out += ' ';
      lastWasSpace = true;
    }
    return;
  }
  out += text;
  lastWasSpace = false;
}

}  // namespace

std::string foldForSearch(const std::string& utf8) {
  std::string out;
  out.reserve(utf8.size());
  size_t pos = 0;
  uint32_t cp = 0;
  bool lastWasSpace = true;
  while (decodeUtf8(utf8, pos, cp)) {
    if (cp < 0x80) {
      if (isAsciiAlnum(cp)) {
        char c = static_cast<char>(cp);
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        out += c;
        lastWasSpace = false;
      } else {
        appendFolded(out, "", lastWasSpace);
      }
    } else if (cp >= 0xC0 && cp <= 0xFF) {
      appendFolded(out, kLatin1Fold[cp - 0xC0], lastWasSpace);
    } else if (cp >= 0x100 && cp <= 0x17F) {
      const char* text = "";
      for (size_t i = 0; i < sizeof(kLatinExtAFold) / sizeof(kLatinExtAFold[0]);
           ++i) {
        if (cp >= kLatinExtAFold[i].first && cp <= kLatinExtAFold[i].last) {
          text = kLatinExtAFold[i].text;
          break;
        }
      }
      appendFolded(out, text, lastWasSpace);
    } else if (cp < 0xC0 || (cp >= 0x2000 && cp <= 0x206F) || cp == 0x3000 ||
               cp == 0xFEFF) {
      // Pontuação Latin-1 (¡, ¿...), pontuação geral (travessões, aspas
      // tipográficas, reticências...) e espaços especiais viram separador.
      appendFolded(out, "", lastWasSpace);
    } else {
      // Outros alfabetos são mantidos como estão.
      appendUtf8(out, cp);
      lastWasSpace = false;
    }
  }
  while (!out.empty() && out[out.size() - 1] == ' ') out.erase(out.size() - 1);
  return out;
}

std::string toLowerAscii(const std::string& s) {
  std::string out(s);
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] >= 'A' && out[i] <= 'Z') out[i] = static_cast<char>(out[i] - 'A' + 'a');
  }
  return out;
}

std::string toUpperAscii(const std::string& s) {
  std::string out(s);
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] >= 'a' && out[i] <= 'z') out[i] = static_cast<char>(out[i] - 'a' + 'A');
  }
  return out;
}

namespace {
bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
         c == '\v';
}
}  // namespace

std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && isSpace(s[b])) ++b;
  while (e > b && isSpace(s[e - 1])) --e;
  return s.substr(b, e - b);
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += s[i];
    }
  }
  out.push_back(cur);
  return out;
}

std::vector<std::string> splitWhitespace(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (size_t i = 0; i < s.size(); ++i) {
    if (isSpace(s[i])) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += s[i];
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

bool startsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool iequalsAscii(const std::string& a, const std::string& b) {
  return a.size() == b.size() && toLowerAscii(a) == toLowerAscii(b);
}

bool istartsWithAscii(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() &&
         toLowerAscii(s.substr(0, prefix.size())) == toLowerAscii(prefix);
}

std::string replaceAll(const std::string& s, const std::string& from,
                       const std::string& to) {
  if (from.empty()) return s;
  std::string out;
  size_t pos = 0;
  while (true) {
    size_t f = s.find(from, pos);
    if (f == std::string::npos) break;
    out.append(s, pos, f - pos);
    out += to;
    pos = f + from.size();
  }
  out.append(s, pos, std::string::npos);
  return out;
}

std::string toString(int64_t value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

std::string toString(uint64_t value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

std::string toString(int value) { return toString(static_cast<int64_t>(value)); }
std::string toString(uint32_t value) {
  return toString(static_cast<uint64_t>(value));
}

std::string toHex32(uint32_t value) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string out(8, '0');
  for (int i = 7; i >= 0; --i) {
    out[static_cast<size_t>(i)] = kDigits[value & 0xF];
    value >>= 4;
  }
  return out;
}

std::string toHexBytes(const uint8_t* data, size_t length) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    out += kDigits[data[i] >> 4];
    out += kDigits[data[i] & 0xF];
  }
  return out;
}

bool isHexString(const std::string& s) {
  if (s.empty()) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F');
    if (!ok) return false;
  }
  return true;
}

bool parseHex32(const std::string& text, uint32_t& out) {
  std::string s = trim(text);
  if (startsWith(s, "0x") || startsWith(s, "0X")) s = s.substr(2);
  if (s.empty() || s.size() > 8 || !isHexString(s)) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    uint32_t d = 0;
    if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
    else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
    else d = static_cast<uint32_t>(c - 'A' + 10);
    v = (v << 4) | d;
  }
  out = v;
  return true;
}

bool parseInt64(const std::string& text, int64_t& out) {
  std::string s = trim(text);
  if (s.empty()) return false;
  bool negative = false;
  size_t i = 0;
  if (s[0] == '-' || s[0] == '+') {
    negative = s[0] == '-';
    i = 1;
  }
  if (i >= s.size()) return false;
  // INT64_MAX sem depender de __STDC_LIMIT_MACROS (C++03).
  const uint64_t kMax = 0x7FFFFFFFFFFFFFFFULL;
  uint64_t v = 0;
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    uint64_t d = static_cast<uint64_t>(s[i] - '0');
    if (v > (kMax - d) / 10) return false;
    v = v * 10 + d;
  }
  out = negative ? -static_cast<int64_t>(v) : static_cast<int64_t>(v);
  return true;
}

std::string formatDate(int64_t unixSeconds) {
  // Algoritmo "civil from days" (Howard Hinnant), domínio público.
  int64_t days = unixSeconds >= 0 ? unixSeconds / 86400 : (unixSeconds - 86399) / 86400;
  days += 719468;
  int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  int64_t doe = days - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153;
  int64_t d = doy - (153 * mp + 2) / 5 + 1;
  int64_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
  std::string dd = toString(d);
  std::string mm = toString(m);
  if (dd.size() < 2) dd = "0" + dd;
  if (mm.size() < 2) mm = "0" + mm;
  return dd + "/" + mm + "/" + toString(y);
}

std::string formatBytes(uint64_t bytes) {
  static const char* const kUnits[] = {"B", "KB", "MB", "GB", "TB"};
  if (bytes < 1024) return toString(bytes) + " B";
  uint64_t whole = bytes;
  uint64_t rem = 0;
  size_t unit = 0;
  while (whole >= 1024 && unit < 4) {
    rem = whole % 1024;
    whole /= 1024;
    ++unit;
  }
  uint64_t tenth = (rem * 10) / 1024;
  std::string out = toString(whole);
  if (whole < 100) out += "," + toString(tenth);
  return out + " " + kUnits[unit];
}

}  // namespace str
}  // namespace nebula
