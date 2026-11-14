// Textos da interface (i18n). Carregados de assets/lang/<idioma>.json.
// Chave ausente: mostra a própria chave (fácil de notar em testes).
#pragma once

#include <map>
#include <string>

#include "core/Status.h"

namespace nebula {

class IFileSystem;

class Strings {
 public:
  Status loadFromJson(const std::string& text);
  Status loadFromFile(IFileSystem& fs, const std::string& path);

  bool has(const std::string& key) const;
  std::string get(const std::string& key) const;
  // Substitui {0} e {1}.
  std::string format(const std::string& key, const std::string& a0,
                     const std::string& a1 = std::string()) const;
  const std::string& language() const { return language_; }
  size_t size() const { return table_.size(); }

 private:
  std::string language_;
  std::map<std::string, std::string> table_;
};

}  // namespace nebula
