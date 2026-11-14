#include "core/Version.h"

#include "core/StringUtil.h"

namespace nebula {

bool Version::parse(const std::string& text, Version& out) {
  std::vector<std::string> parts = str::split(str::trim(text), '.');
  if (parts.empty() || parts.size() > 3) return false;
  int values[3] = {0, 0, 0};
  for (size_t i = 0; i < parts.size(); ++i) {
    const std::string& p = parts[i];
    if (p.empty() || p.size() > 4) return false;
    int v = 0;
    for (size_t k = 0; k < p.size(); ++k) {
      if (p[k] < '0' || p[k] > '9') return false;
      v = v * 10 + (p[k] - '0');
    }
    values[i] = v;
  }
  out = Version(values[0], values[1], values[2]);
  return true;
}

std::string Version::toString() const {
  return str::toString(major) + "." + str::toString(minor) + "." + str::toString(patch);
}

int Version::compare(const Version& o) const {
  if (major != o.major) return major < o.major ? -1 : 1;
  if (minor != o.minor) return minor < o.minor ? -1 : 1;
  if (patch != o.patch) return patch < o.patch ? -1 : 1;
  return 0;
}

}  // namespace nebula
