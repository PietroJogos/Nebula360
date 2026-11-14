// Sistema de temas: cores e medidas da interface, carregados de
// assets/themes/<id>/theme.json. Campos ausentes ou inválidos herdam do tema
// base embutido, então um tema incompleto nunca deixa a interface ilegível.
#pragma once

#include <string>
#include <vector>

#include "core/Color.h"
#include "core/Json.h"
#include "core/Status.h"

namespace nebula {

class IFileSystem;

struct ThemeColors {
  Color backgroundTop;
  Color backgroundBottom;
  Color surface;
  Color surfaceFocused;
  Color accent;
  Color accentText;
  Color text;
  Color textMuted;
  Color favorite;
  Color danger;
  Color overlay;
  ThemeColors();
};

struct ThemeMetrics {
  int safeMarginX;  // área segura de TV (title safe)
  int safeMarginY;
  int headerHeight;
  int tileWidth;
  int tileHeight;
  int tileSpacing;
  int columns;
  int cornerRadius;
  int fontTitle;
  int fontBody;
  int fontSmall;
  ThemeMetrics();
};

struct Theme {
  static const int kSchemaVersion = 1;
  std::string id;
  std::string name;
  std::string author;
  std::string version;
  std::string backgroundImage;  // relativo à pasta do tema (opcional)
  std::string directory;        // pasta de onde foi carregado ("" = embutido)
  ThemeColors colors;
  ThemeMetrics metrics;
};

Theme builtinDarkTheme();

// Lê um tema sobre "base". Avisos não impedem o carregamento.
Status themeFromJson(const JsonValue& json, const Theme& base, Theme& out,
                     std::vector<std::string>& warnings);
JsonValue themeToJson(const Theme& theme);

class ThemeManager {
 public:
  explicit ThemeManager(IFileSystem& fs);

  // Carrega cada <dir>/<id>/theme.json. Retorna quantos foram aceitos.
  size_t loadDirectory(const std::string& themesDir, std::vector<std::string>& warnings);

  const std::vector<Theme>& themes() const { return themes_; }
  const Theme* find(const std::string& id) const;
  const Theme& current() const { return themes_[current_]; }
  // Retorna false (e mantém o atual) se o id não existir.
  bool select(const std::string& id);

 private:
  IFileSystem& fs_;
  std::vector<Theme> themes_;
  size_t current_;
  NB_NONCOPYABLE(ThemeManager);
};

}  // namespace nebula
