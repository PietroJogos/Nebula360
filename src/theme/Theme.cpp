#include "theme/Theme.h"

#include <algorithm>

#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

const char* const kTag = "theme";

struct ColorField {
  const char* name;
  Color ThemeColors::*member;
};

const ColorField kColorFields[] = {
    {"backgroundTop", &ThemeColors::backgroundTop},
    {"backgroundBottom", &ThemeColors::backgroundBottom},
    {"surface", &ThemeColors::surface},
    {"surfaceFocused", &ThemeColors::surfaceFocused},
    {"accent", &ThemeColors::accent},
    {"accentText", &ThemeColors::accentText},
    {"text", &ThemeColors::text},
    {"textMuted", &ThemeColors::textMuted},
    {"favorite", &ThemeColors::favorite},
    {"danger", &ThemeColors::danger},
    {"overlay", &ThemeColors::overlay}};

struct MetricField {
  const char* name;
  int ThemeMetrics::*member;
  int minValue;
  int maxValue;
};

const MetricField kMetricFields[] = {
    {"safeMarginX", &ThemeMetrics::safeMarginX, 0, 200},
    {"safeMarginY", &ThemeMetrics::safeMarginY, 0, 120},
    {"headerHeight", &ThemeMetrics::headerHeight, 40, 200},
    {"tileWidth", &ThemeMetrics::tileWidth, 80, 400},
    {"tileHeight", &ThemeMetrics::tileHeight, 100, 560},
    {"tileSpacing", &ThemeMetrics::tileSpacing, 0, 80},
    {"columns", &ThemeMetrics::columns, 1, 10},
    {"cornerRadius", &ThemeMetrics::cornerRadius, 0, 40},
    {"fontTitle", &ThemeMetrics::fontTitle, 16, 72},
    {"fontBody", &ThemeMetrics::fontBody, 12, 48},
    {"fontSmall", &ThemeMetrics::fontSmall, 10, 36}};

const size_t kColorFieldCount = sizeof(kColorFields) / sizeof(kColorFields[0]);
const size_t kMetricFieldCount = sizeof(kMetricFields) / sizeof(kMetricFields[0]);

bool isValidThemeId(const std::string& id) {
  if (id.empty() || id.size() > 40) return false;
  for (size_t i = 0; i < id.size(); ++i) {
    char c = id[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

struct ThemeNameLess {
  bool operator()(const Theme& a, const Theme& b) const { return a.id < b.id; }
};

}  // namespace

ThemeColors::ThemeColors()
    : backgroundTop(makeColor(0x0B, 0x0F, 0x2A)),
      backgroundBottom(makeColor(0x1C, 0x0B, 0x33)),
      surface(makeColor(0xFF, 0xFF, 0xFF, 0x14)),
      surfaceFocused(makeColor(0xFF, 0xFF, 0xFF, 0x2E)),
      accent(makeColor(0x8B, 0x5C, 0xF6)),
      accentText(makeColor(0xFF, 0xFF, 0xFF)),
      text(makeColor(0xF4, 0xF2, 0xFF)),
      textMuted(makeColor(0xA8, 0xA3, 0xC7)),
      favorite(makeColor(0xFA, 0xCC, 0x15)),
      danger(makeColor(0xF8, 0x71, 0x71)),
      overlay(makeColor(0x05, 0x06, 0x14, 0xD0)) {}

ThemeMetrics::ThemeMetrics()
    : safeMarginX(64), safeMarginY(36), headerHeight(64), tileWidth(168),
      tileHeight(232), tileSpacing(18), columns(6), cornerRadius(12),
      fontTitle(30), fontBody(20), fontSmall(15) {}

Theme builtinDarkTheme() {
  Theme t;
  t.id = "nebula-escuro";
  t.name = "Nébula Escuro";
  t.author = "Projeto Nébula 360";
  t.version = "1.0";
  return t;
}

Status themeFromJson(const JsonValue& json, const Theme& base, Theme& out,
                     std::vector<std::string>& warnings) {
  if (!json.isObject()) return Status(kParseError, "tema não é um objeto JSON");
  Theme t = base;
  t.directory.clear();
  t.backgroundImage.clear();
  int64_t schema = json.get("schema").asInt(Theme::kSchemaVersion);
  if (schema != Theme::kSchemaVersion) {
    return Status(kUnsupported, "versão de tema não suportada: " + str::toString(schema));
  }
  t.id = json.get("id").asString();
  if (!isValidThemeId(t.id)) {
    return Status(kInvalidArgument, "id de tema inválido (use a-z, 0-9, - e _): '" + t.id + "'");
  }
  t.name = json.get("name").asString(t.id);
  t.author = json.get("author").asString();
  t.version = json.get("version").asString("1.0");
  t.backgroundImage = json.get("backgroundImage").asString();
  if (t.backgroundImage.find("..") != std::string::npos) {
    warnings.push_back(t.id + ": backgroundImage não pode sair da pasta do tema");
    t.backgroundImage.clear();
  }

  const JsonValue& colors = json.get("colors");
  for (size_t i = 0; i < kColorFieldCount; ++i) {
    const JsonValue& v = colors.get(kColorFields[i].name);
    if (v.isNull()) continue;
    Color c = 0;
    if (v.isString() && parseColor(v.asString(), c)) {
      t.colors.*(kColorFields[i].member) = c;
    } else {
      warnings.push_back(t.id + ": cor inválida em colors." + kColorFields[i].name);
    }
  }
  const JsonValue& metrics = json.get("metrics");
  for (size_t i = 0; i < kMetricFieldCount; ++i) {
    const JsonValue& v = metrics.get(kMetricFields[i].name);
    if (v.isNull()) continue;
    if (!v.isNumber()) {
      warnings.push_back(t.id + ": metrics." + kMetricFields[i].name + " não é número");
      continue;
    }
    int64_t n = v.asInt();
    if (n < kMetricFields[i].minValue || n > kMetricFields[i].maxValue) {
      warnings.push_back(t.id + ": metrics." + kMetricFields[i].name +
                         " fora do intervalo; ajustado");
      n = n < kMetricFields[i].minValue ? kMetricFields[i].minValue
                                        : kMetricFields[i].maxValue;
    }
    t.metrics.*(kMetricFields[i].member) = static_cast<int>(n);
  }
  out = t;
  return Status::Ok();
}

JsonValue themeToJson(const Theme& theme) {
  JsonValue o = JsonValue::object();
  o.set("schema", Theme::kSchemaVersion);
  o.set("id", theme.id);
  o.set("name", theme.name);
  o.set("author", theme.author);
  o.set("version", theme.version);
  if (!theme.backgroundImage.empty()) o.set("backgroundImage", theme.backgroundImage);
  JsonValue colors = JsonValue::object();
  for (size_t i = 0; i < kColorFieldCount; ++i) {
    colors.set(kColorFields[i].name, colorToString(theme.colors.*(kColorFields[i].member)));
  }
  o.set("colors", colors);
  JsonValue metrics = JsonValue::object();
  for (size_t i = 0; i < kMetricFieldCount; ++i) {
    metrics.set(kMetricFields[i].name, theme.metrics.*(kMetricFields[i].member));
  }
  o.set("metrics", metrics);
  return o;
}

ThemeManager::ThemeManager(IFileSystem& fs) : fs_(fs), current_(0) {
  themes_.push_back(builtinDarkTheme());
}

size_t ThemeManager::loadDirectory(const std::string& themesDir,
                                   std::vector<std::string>& warnings) {
  std::vector<DirEntry> entries;
  Status s = fs_.listDirectory(themesDir, entries);
  if (!s.ok()) {
    warnings.push_back("pasta de temas indisponível: " + s.toString());
    return 0;
  }
  std::string currentId = current().id;
  size_t accepted = 0;
  const Theme base = builtinDarkTheme();
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].isDirectory) continue;
    std::string dir = path::join(themesDir, entries[i].name, fs_.separator());
    std::string file = path::join(dir, "theme.json", fs_.separator());
    std::string text;
    s = fs_.readFile(file, text, 256 * 1024);
    if (!s.ok()) {
      warnings.push_back(s.toString());
      continue;
    }
    JsonValue json;
    s = parseJson(text, json);
    Theme t;
    if (s.ok()) s = themeFromJson(json, base, t, warnings);
    if (!s.ok()) {
      warnings.push_back(file + ": " + s.toString());
      continue;
    }
    t.directory = dir;
    bool replaced = false;
    for (size_t k = 0; k < themes_.size(); ++k) {
      if (themes_[k].id == t.id) {
        themes_[k] = t;  // arquivo substitui o embutido de mesmo id
        replaced = true;
      }
    }
    if (!replaced) themes_.push_back(t);
    ++accepted;
  }
  std::sort(themes_.begin(), themes_.end(), ThemeNameLess());
  if (!select(currentId)) current_ = 0;
  for (size_t i = 0; i < warnings.size(); ++i) NB_WARN(kTag, warnings[i]);
  NB_INFO(kTag, accepted << " tema(s) carregado(s) de " << themesDir);
  return accepted;
}

const Theme* ThemeManager::find(const std::string& id) const {
  for (size_t i = 0; i < themes_.size(); ++i) {
    if (themes_[i].id == id) return &themes_[i];
  }
  return NULL;
}

bool ThemeManager::select(const std::string& id) {
  for (size_t i = 0; i < themes_.size(); ++i) {
    if (themes_[i].id == id) {
      current_ = i;
      return true;
    }
  }
  return false;
}

}  // namespace nebula
