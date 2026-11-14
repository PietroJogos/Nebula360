// Versão no formato MAIOR.MENOR.CORREÇÃO (subconjunto de SemVer, sem sufixos).
#pragma once

#include <string>

namespace nebula {

struct Version {
  int major;
  int minor;
  int patch;
  Version() : major(0), minor(0), patch(0) {}
  Version(int a, int b, int c) : major(a), minor(b), patch(c) {}

  // Aceita "1", "1.2" e "1.2.3"; componentes de 0 a 9999.
  static bool parse(const std::string& text, Version& out);
  std::string toString() const;
  // <0, 0 ou >0.
  int compare(const Version& other) const;
  bool operator<(const Version& o) const { return compare(o) < 0; }
  bool operator==(const Version& o) const { return compare(o) == 0; }
};

}  // namespace nebula
