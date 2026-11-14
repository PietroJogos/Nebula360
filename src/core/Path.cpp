#include "core/Path.h"

#include "core/StringUtil.h"

namespace nebula {
namespace path {

namespace {

bool isDeviceRoot(const std::string& p, char sep) {
  // "Hdd1:" ou "Hdd1:\"
  if (p.empty()) return false;
  size_t colon = p.find(':');
  if (colon == std::string::npos) return false;
  return colon == p.size() - 1 || (colon == p.size() - 2 && p[p.size() - 1] == sep);
}

}  // namespace

std::string stripTrailingSeparator(const std::string& p, char sep) {
  std::string out(p);
  while (out.size() > 1 && out[out.size() - 1] == sep) {
    if (isDeviceRoot(out, sep) && out.find(':') == out.size() - 2) break;
    out.erase(out.size() - 1);
  }
  return out;
}

std::string join(const std::string& base, const std::string& name, char sep) {
  if (base.empty()) return name;
  if (name.empty()) return base;
  std::string b(base);
  if (b[b.size() - 1] != sep) b += sep;
  size_t start = 0;
  while (start < name.size() && name[start] == sep) ++start;
  return b + name.substr(start);
}

std::string parent(const std::string& p, char sep) {
  std::string s = stripTrailingSeparator(p, sep);
  size_t pos = s.rfind(sep);
  if (pos == std::string::npos) {
    return "";
  }
  if (pos == 0) return s.substr(0, 1);  // "/x" -> "/"
  std::string head = s.substr(0, pos);
  if (!head.empty() && head[head.size() - 1] == ':') return head;  // "Hdd1:"
  return head;
}

std::string fileName(const std::string& p, char sep) {
  std::string s = stripTrailingSeparator(p, sep);
  size_t pos = s.rfind(sep);
  if (pos == std::string::npos) {
    size_t colon = s.find(':');
    if (colon != std::string::npos && colon == s.size() - 1) return "";
    return s;
  }
  return s.substr(pos + 1);
}

std::string extensionLower(const std::string& p, char sep) {
  std::string name = fileName(p, sep);
  size_t dot = name.rfind('.');
  if (dot == std::string::npos || dot == 0) return "";
  return str::toLowerAscii(name.substr(dot + 1));
}

std::string stem(const std::string& p, char sep) {
  std::string name = fileName(p, sep);
  size_t dot = name.rfind('.');
  if (dot == std::string::npos || dot == 0) return name;
  return name.substr(0, dot);
}

bool isWithin(const std::string& p, const std::string& root, char sep) {
  std::string a = str::toLowerAscii(stripTrailingSeparator(p, sep));
  std::string r = str::toLowerAscii(stripTrailingSeparator(root, sep));
  if (r.empty()) return false;
  if (a == r) return true;
  if (!str::startsWith(a, r)) return false;
  if (r[r.size() - 1] == sep) return true;
  return a.size() > r.size() && a[r.size()] == sep;
}

}  // namespace path
}  // namespace nebula
