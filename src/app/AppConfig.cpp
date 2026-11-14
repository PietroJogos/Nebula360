#include "app/AppConfig.h"

#include "core/Log.h"
#include "core/StringUtil.h"
#include "library/Category.h"
#include "library/LibraryQuery.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {
const char* const kTag = "config";
const int kMaxScanDepth = 8;
const size_t kMaxScanRoots = 32;
}  // namespace

AppConfig::AppConfig()
    : themeId("nebula-escuro"), language("pt-BR"), sortMode("title"),
      showMissing(false), exportLibrary(true), logLevel("info") {}

JsonValue AppConfig::toJson() const {
  JsonValue root = JsonValue::object();
  root.set("schema", kSchemaVersion);
  JsonValue roots = JsonValue::array();
  for (size_t i = 0; i < scanRoots.size(); ++i) {
    JsonValue o = JsonValue::object();
    o.set("path", scanRoots[i].path);
    o.set("category", scanRoots[i].category);
    o.set("maxDepth", scanRoots[i].maxDepth);
    o.set("enabled", scanRoots[i].enabled);
    roots.push(o);
  }
  root.set("scanPaths", roots);
  root.set("theme", themeId);
  root.set("language", language);
  root.set("sort", sortMode);
  root.set("lastCategory", lastCategory);
  root.set("showMissing", showMissing);
  JsonValue integrations = JsonValue::object();
  integrations.set("libraryExport", exportLibrary);
  root.set("integrations", integrations);
  root.set("logLevel", logLevel);
  return root;
}

AppConfig AppConfig::fromJson(const JsonValue& json, std::vector<std::string>& warnings) {
  AppConfig c;
  if (!json.isObject()) {
    warnings.push_back("configuração não é um objeto JSON; usando padrões");
    return c;
  }
  int64_t schema = json.get("schema").asInt(kSchemaVersion);
  if (schema != kSchemaVersion) {
    warnings.push_back("versão de configuração desconhecida (" + str::toString(schema) +
                       "); campos serão lidos com cautela");
  }
  const JsonValue& roots = json.get("scanPaths");
  if (roots.isArray()) {
    for (size_t i = 0; i < roots.size() && c.scanRoots.size() < kMaxScanRoots; ++i) {
      const JsonValue& o = roots.at(i);
      ScanRoot r;
      r.path = str::trim(o.get("path").asString());
      if (r.path.empty()) {
        warnings.push_back("pasta de varredura sem caminho ignorada");
        continue;
      }
      r.category = o.get("category").asString(category::kAuto);
      if (r.category != category::kAuto && !category::isValid(r.category)) {
        warnings.push_back("categoria inválida '" + r.category + "' em " + r.path +
                           "; usando auto");
        r.category = category::kAuto;
      }
      int64_t depth = o.get("maxDepth").asInt(4);
      if (depth < 0 || depth > kMaxScanDepth) {
        warnings.push_back("profundidade fora do intervalo em " + r.path);
        depth = depth < 0 ? 0 : kMaxScanDepth;
      }
      r.maxDepth = static_cast<int>(depth);
      r.enabled = o.get("enabled").asBool(true);
      c.scanRoots.push_back(r);
    }
  } else if (json.has("scanPaths")) {
    warnings.push_back("'scanPaths' não é uma lista");
  }
  c.themeId = json.get("theme").asString(c.themeId);
  c.language = json.get("language").asString(c.language);
  std::string sort = json.get("sort").asString(c.sortMode);
  SortMode mode;
  if (parseSortMode(sort, mode)) {
    c.sortMode = sort;
  } else {
    warnings.push_back("modo de ordenação inválido: " + sort);
  }
  std::string last = json.get("lastCategory").asString();
  if (last.empty() || last == "favorites" || category::isValid(last)) c.lastCategory = last;
  c.showMissing = json.get("showMissing").asBool(false);
  c.exportLibrary = json.get("integrations").get("libraryExport").asBool(true);
  std::string level = json.get("logLevel").asString(c.logLevel);
  LogLevel parsed;
  if (parseLogLevel(level, parsed)) {
    c.logLevel = level;
  } else {
    warnings.push_back("nível de log inválido: " + level);
  }
  return c;
}

AppConfig defaultConfig(PlatformKind platform) {
  AppConfig c;
  if (platform == kPlatformXbox360) {
    // Locais usuais em consoles RGH. O usuário pode alterar em Configurações.
    const char* const paths[] = {"Hdd1:\\Games", "Hdd1:\\Content\\0000000000000000",
                                 "Hdd1:\\Homebrew", "Usb0:\\Games"};
    const char* const cats[] = {"auto", "auto", "homebrew", "auto"};
    const int depths[] = {3, 3, 3, 3};
    for (size_t i = 0; i < 4; ++i) {
      ScanRoot r;
      r.path = paths[i];
      r.category = cats[i];
      r.maxDepth = depths[i];
      c.scanRoots.push_back(r);
    }
  }
  return c;
}

Status loadConfig(IFileSystem& fs, const std::string& filePath, PlatformKind platform,
                  AppConfig& out, std::vector<std::string>& warnings) {
  out = defaultConfig(platform);
  if (!fs.exists(filePath)) {
    NB_INFO(kTag, "configuração não encontrada; usando padrões");
    return Status::Ok();
  }
  std::string text;
  Status s = fs.readFile(filePath, text, 1024 * 1024);
  JsonValue json;
  if (s.ok()) s = parseJson(text, json);
  if (!s.ok()) {
    std::string backup = filePath + ".corrompido";
    if (fs.exists(backup)) fs.removeFile(backup);
    fs.renameFile(filePath, backup);
    NB_ERROR(kTag, "configuração ilegível (" << s.toString() << "); usando padrões");
    return Status(kParseError, s.message());
  }
  out = AppConfig::fromJson(json, warnings);
  for (size_t i = 0; i < warnings.size(); ++i) NB_WARN(kTag, warnings[i]);
  return Status::Ok();
}

Status saveConfig(IFileSystem& fs, const std::string& filePath, const AppConfig& config) {
  return fs.writeFileAtomic(filePath, config.toJson().serialize(true));
}

}  // namespace nebula
