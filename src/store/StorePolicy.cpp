#include "store/StorePolicy.h"

#include "core/Path.h"
#include "core/StringUtil.h"
#include "store/Catalog.h"

namespace nebula {
namespace store {

namespace {

const char* const kFreeLicenses[] = {
    "MIT", "BSD-2-Clause", "BSD-3-Clause", "ISC", "Zlib", "Unlicense", "CC0-1.0",
    "Apache-2.0", "BSL-1.0", "MPL-2.0", "GPL-2.0-only", "GPL-2.0-or-later",
    "GPL-3.0-only", "GPL-3.0-or-later", "LGPL-2.1-only", "LGPL-2.1-or-later",
    "LGPL-3.0-only", "LGPL-3.0-or-later", "AGPL-3.0-only", "AGPL-3.0-or-later",
    "OFL-1.1", "WTFPL", "0BSD"};

const char* const kCopyleftPrefixes[] = {"GPL-", "LGPL-", "AGPL-", "MPL-"};

bool isHex64(const std::string& s) {
  if (s.size() != 64) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool hasHttpScheme(const std::string& url) {
  return str::startsWith(url, "https://") || str::startsWith(url, "http://");
}

}  // namespace

bool isApprovedFreeLicense(const std::string& spdx) {
  for (size_t i = 0; i < sizeof(kFreeLicenses) / sizeof(kFreeLicenses[0]); ++i) {
    if (spdx == kFreeLicenses[i]) return true;
  }
  return false;
}

bool isCopyleftLicense(const std::string& spdx) {
  for (size_t i = 0; i < sizeof(kCopyleftPrefixes) / sizeof(kCopyleftPrefixes[0]); ++i) {
    if (str::startsWith(spdx, kCopyleftPrefixes[i])) return true;
  }
  return false;
}

Status evaluateItemPolicy(const CatalogItem& item) {
  if (!isValidItemId(item.id)) {
    return Status(kPolicyViolation, "id inválido (use a-z, 0-9, '-', '_', até 32)");
  }
  if (str::trim(item.name).empty()) return Status(kPolicyViolation, "nome ausente");
  if (str::trim(item.version).empty()) return Status(kPolicyViolation, "versão ausente");
  if (str::trim(item.author).empty()) return Status(kPolicyViolation, "autor ausente");
  if (!isValidStoreCategory(item.category)) {
    return Status(kPolicyViolation, "categoria não permitida na loja: '" + item.category + "'");
  }
  if (str::trim(item.license).empty()) {
    return Status(kPolicyViolation, "sem licença declarada");
  }
  const CatalogAuthorization& auth = item.authorization;
  if (isApprovedFreeLicense(item.license)) {
    if (auth.type != "license" && auth.type != "written-permission") {
      return Status(kPolicyViolation, "authorization.type deve ser 'license' ou 'written-permission'");
    }
    if (str::trim(auth.reference).empty() && str::trim(item.licenseUrl).empty()) {
      return Status(kPolicyViolation, "falta referência ao texto da licença");
    }
  } else {
    // Licença não-livre (ex.: "não comercial") só com autorização escrita.
    if (auth.type != "written-permission" || str::trim(auth.reference).empty() ||
        str::trim(auth.grantedBy).empty()) {
      return Status(kPolicyViolation,
                    "licença '" + item.license +
                        "' não é livre: exige authorization.type='written-permission' "
                        "com reference e grantedBy");
    }
  }
  if (isCopyleftLicense(item.license) && !hasHttpScheme(item.sourceUrl)) {
    return Status(kPolicyViolation, "licença copyleft exige sourceUrl com o código-fonte");
  }
  if (item.files.empty()) return Status(kPolicyViolation, "item sem arquivos");
  for (size_t i = 0; i < item.files.size(); ++i) {
    const CatalogFile& f = item.files[i];
    if (!hasHttpScheme(f.url)) {
      return Status(kPolicyViolation, "arquivo " + str::toString(static_cast<uint64_t>(i)) +
                                          ": URL deve usar http(s)");
    }
    if (f.size == 0) return Status(kPolicyViolation, "arquivo sem tamanho: " + f.installPath);
    if (!isHex64(f.sha256)) {
      return Status(kPolicyViolation, "SHA-256 ausente ou inválido: " + f.installPath);
    }
    Status s = validateInstallRelativePath(f.installPath);
    if (!s.ok()) return Status(kPolicyViolation, s.message());
  }
  return Status::Ok();
}

bool isValidFatxName(const std::string& name) {
  if (name.empty() || name.size() > kFatxMaxNameLength) return false;
  if (name == "." || name == "..") return false;
  static const char kAllowedPunct[] = " !#$%&'()-.@[]^_`{}~";
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    for (size_t k = 0; !ok && kAllowedPunct[k] != '\0'; ++k) ok = c == kAllowedPunct[k];
    if (!ok) return false;
  }
  return name[name.size() - 1] != ' ' && name[name.size() - 1] != '.';
}

Status validateInstallRelativePath(const std::string& relative) {
  if (relative.empty()) return Status(kInvalidArgument, "caminho de instalação vazio");
  if (relative[0] == '/' || relative.find('\\') != std::string::npos ||
      relative.find(':') != std::string::npos) {
    return Status(kInvalidArgument, "caminho de instalação deve ser relativo e usar '/': " + relative);
  }
  std::vector<std::string> parts = str::split(relative, '/');
  if (parts.size() > 8) return Status(kInvalidArgument, "caminho de instalação profundo demais");
  for (size_t i = 0; i < parts.size(); ++i) {
    if (!isValidFatxName(parts[i])) {
      return Status(kInvalidArgument, "nome inválido para FATX: '" + parts[i] + "'");
    }
  }
  return Status::Ok();
}

std::string buildInstallPath(const std::string& installRoot, const std::string& itemId,
                             const std::string& relative, char sep) {
  std::string out = path::join(installRoot, itemId, sep);
  std::vector<std::string> parts = str::split(relative, '/');
  for (size_t i = 0; i < parts.size(); ++i) out = path::join(out, parts[i], sep);
  return out;
}

}  // namespace store
}  // namespace nebula
