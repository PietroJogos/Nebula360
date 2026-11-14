#include "store/Catalog.h"

#include "core/StringUtil.h"
#include "store/StorePolicy.h"

namespace nebula {

uint64_t CatalogItem::totalSize() const {
  uint64_t total = 0;
  for (size_t i = 0; i < files.size(); ++i) total += files[i].size;
  return total;
}

namespace store {

bool isValidStoreCategory(const std::string& id) {
  static const char* const kCategories[] = {"emulator", "media", "tool", "utility",
                                            "homebrew-game"};
  for (size_t i = 0; i < sizeof(kCategories) / sizeof(kCategories[0]); ++i) {
    if (id == kCategories[i]) return true;
  }
  return false;
}

bool isValidItemId(const std::string& id) {
  if (id.empty() || id.size() > 32) return false;
  for (size_t i = 0; i < id.size(); ++i) {
    char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

}  // namespace store

namespace {

CatalogItem itemFromJson(const JsonValue& o) {
  CatalogItem it;
  it.id = o.get("id").asString();
  it.name = o.get("name").asString();
  it.version = o.get("version").asString();
  it.author = o.get("author").asString();
  it.category = o.get("category").asString();
  it.description = o.get("description").asString();
  it.license = o.get("license").asString();
  it.licenseUrl = o.get("licenseUrl").asString();
  it.sourceUrl = o.get("sourceUrl").asString();
  it.homepage = o.get("homepage").asString();
  const JsonValue& a = o.get("authorization");
  it.authorization.type = a.get("type").asString();
  it.authorization.reference = a.get("reference").asString();
  it.authorization.grantedBy = a.get("grantedBy").asString();
  it.authorization.date = a.get("date").asString();
  const JsonValue& files = o.get("files");
  for (size_t i = 0; i < files.size(); ++i) {
    const JsonValue& f = files.at(i);
    CatalogFile cf;
    cf.url = f.get("url").asString();
    cf.installPath = f.get("installPath").asString();
    int64_t size = f.get("size").asInt(0);
    cf.size = size > 0 ? static_cast<uint64_t>(size) : 0;
    cf.sha256 = str::toLowerAscii(f.get("sha256").asString());
    it.files.push_back(cf);
  }
  return it;
}

}  // namespace

Status parseCatalog(const JsonValue& json, Catalog& catalog,
                    std::vector<CatalogIssue>& rejected) {
  catalog = Catalog();
  rejected.clear();
  if (!json.isObject()) return Status(kParseError, "catálogo não é um objeto JSON");
  if (json.get("schema").asInt(-1) != Catalog::kSchemaVersion) {
    return Status(kUnsupported, "versão de catálogo não suportada");
  }
  catalog.name = json.get("name").asString();
  catalog.updatedAt = json.get("updatedAt").asString();
  const JsonValue& items = json.get("items");
  if (!items.isArray()) return Status(kParseError, "catálogo sem lista 'items'");
  for (size_t i = 0; i < items.size(); ++i) {
    CatalogItem item = itemFromJson(items.at(i));
    CatalogIssue issue;
    issue.itemId = item.id.empty() ? ("#" + str::toString(static_cast<uint64_t>(i))) : item.id;
    bool duplicate = false;
    for (size_t k = 0; k < catalog.items.size(); ++k) {
      if (catalog.items[k].id == item.id) duplicate = true;
    }
    if (duplicate) {
      issue.reason = "id duplicado";
      rejected.push_back(issue);
      continue;
    }
    Status s = store::evaluateItemPolicy(item);
    if (!s.ok()) {
      issue.reason = s.message();
      rejected.push_back(issue);
      continue;
    }
    catalog.items.push_back(item);
  }
  return Status::Ok();
}

}  // namespace nebula
