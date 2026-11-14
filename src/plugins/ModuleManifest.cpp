#include "plugins/ModuleManifest.h"

#include "core/StringUtil.h"

namespace nebula {

namespace {

const char* const kPermissions[] = {"fs.read",  "fs.write",    "net.client", "net.server",
                                    "launch",   "system.info", "ui"};

bool readStringList(const JsonValue& v, std::vector<std::string>& out, const char* field,
                    Status& error) {
  out.clear();
  if (v.isNull()) return true;
  if (!v.isArray()) {
    error = Status(kParseError, std::string("'") + field + "' deve ser uma lista");
    return false;
  }
  for (size_t i = 0; i < v.size(); ++i) {
    if (!v.at(i).isString()) {
      error = Status(kParseError, std::string("'") + field + "' deve conter só textos");
      return false;
    }
    out.push_back(v.at(i).asString());
  }
  return true;
}

}  // namespace

bool isKnownPermission(const std::string& p) {
  for (size_t i = 0; i < sizeof(kPermissions) / sizeof(kPermissions[0]); ++i) {
    if (p == kPermissions[i]) return true;
  }
  return false;
}

bool isValidModuleId(const std::string& id) {
  if (id.size() < 3 || id.size() > 48) return false;
  if (id[0] < 'a' || id[0] > 'z') return false;
  for (size_t i = 0; i < id.size(); ++i) {
    char c = id[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
    if (!ok) return false;
  }
  return id[id.size() - 1] != '.';
}

bool ModuleManifest::supportsPlatform(PlatformKind platform) const {
  if (platforms.empty()) return true;
  const char* name = platform == kPlatformXbox360 ? "xbox360" : "host";
  for (size_t i = 0; i < platforms.size(); ++i) {
    if (platforms[i] == name) return true;
  }
  return false;
}

bool ModuleManifest::hasPermission(const std::string& permission) const {
  for (size_t i = 0; i < permissions.size(); ++i) {
    if (permissions[i] == permission) return true;
  }
  return false;
}

Status parseModuleManifest(const JsonValue& json, ModuleManifest& out) {
  out = ModuleManifest();
  if (!json.isObject()) return Status(kParseError, "manifesto não é um objeto JSON");
  if (json.get("schema").asInt(-1) != ModuleManifest::kSchemaVersion) {
    return Status(kUnsupported, "versão de manifesto não suportada");
  }
  ModuleManifest m;
  m.id = json.get("id").asString();
  if (!isValidModuleId(m.id)) return Status(kInvalidArgument, "id de módulo inválido: '" + m.id + "'");
  m.name = json.get("name").asString(m.id);
  if (!Version::parse(json.get("version").asString(), m.version)) {
    return Status(kInvalidArgument, m.id + ": versão inválida");
  }
  m.apiVersion = static_cast<int>(json.get("apiVersion").asInt(0));
  if (m.apiVersion <= 0) return Status(kInvalidArgument, m.id + ": apiVersion ausente");
  std::string kind = json.get("kind").asString("builtin");
  if (kind == "builtin") {
    m.kind = kModuleBuiltin;
  } else if (kind == "script") {
    m.kind = kModuleScript;
  } else {
    // "native" (DLL/XEX de terceiros) é recusado de propósito.
    return Status(kPolicyViolation, m.id + ": tipo de módulo não permitido: '" + kind + "'");
  }
  m.required = json.get("required").asBool(false);
  m.enabledByDefault = json.get("enabledByDefault").asBool(true);

  Status err;
  if (!readStringList(json.get("platforms"), m.platforms, "platforms", err)) return err.withContext(m.id);
  for (size_t i = 0; i < m.platforms.size(); ++i) {
    if (m.platforms[i] != "host" && m.platforms[i] != "xbox360") {
      return Status(kInvalidArgument, m.id + ": plataforma desconhecida '" + m.platforms[i] + "'");
    }
  }
  if (!readStringList(json.get("conflicts"), m.conflicts, "conflicts", err)) return err.withContext(m.id);
  if (!readStringList(json.get("permissions"), m.permissions, "permissions", err)) return err.withContext(m.id);
  for (size_t i = 0; i < m.permissions.size(); ++i) {
    if (!isKnownPermission(m.permissions[i])) {
      return Status(kInvalidArgument, m.id + ": permissão desconhecida '" + m.permissions[i] + "'");
    }
  }

  const JsonValue& deps = json.get("dependencies");
  if (!deps.isNull() && !deps.isArray()) return Status(kParseError, m.id + ": 'dependencies' deve ser lista");
  for (size_t i = 0; i < deps.size(); ++i) {
    ModuleDependency d;
    d.id = deps.at(i).get("id").asString();
    if (!isValidModuleId(d.id) || d.id == m.id) {
      return Status(kInvalidArgument, m.id + ": dependência inválida '" + d.id + "'");
    }
    std::string minV = deps.at(i).get("minVersion").asString("0.0.0");
    if (!Version::parse(minV, d.minVersion)) {
      return Status(kInvalidArgument, m.id + ": minVersion inválida em " + d.id);
    }
    d.optional = deps.at(i).get("optional").asBool(false);
    m.dependencies.push_back(d);
  }

  const JsonValue& ports = json.get("ports");
  if (!ports.isNull() && !ports.isArray()) return Status(kParseError, m.id + ": 'ports' deve ser lista");
  for (size_t i = 0; i < ports.size(); ++i) {
    int64_t p = ports.at(i).asInt(-1);
    if (p < 1 || p > 65535) return Status(kInvalidArgument, m.id + ": porta inválida");
    m.ports.push_back(static_cast<uint32_t>(p));
  }
  if (!m.ports.empty() && !m.hasPermission("net.server")) {
    return Status(kPolicyViolation, m.id + ": declarar portas exige a permissão net.server");
  }

  int64_t mem = json.get("memoryBudgetKiB").asInt(0);
  if (mem < 0 || mem > 512 * 1024) return Status(kInvalidArgument, m.id + ": memoryBudgetKiB fora do intervalo");
  m.memoryBudgetKiB = static_cast<uint32_t>(mem);

  m.sha256 = str::toLowerAscii(json.get("sha256").asString());
  if (!m.sha256.empty() && (m.sha256.size() != 64 || !str::isHexString(m.sha256))) {
    return Status(kInvalidArgument, m.id + ": sha256 inválido");
  }
  if (m.kind == kModuleScript && m.sha256.empty()) {
    return Status(kPolicyViolation, m.id + ": módulo de script exige sha256");
  }
  out = m;
  return Status::Ok();
}

Status parseModuleManifestText(const std::string& text, ModuleManifest& out) {
  JsonValue json;
  Status s = parseJson(text, json);
  if (!s.ok()) return s;
  return parseModuleManifest(json, out);
}

}  // namespace nebula
