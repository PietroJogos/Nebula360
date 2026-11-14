#include "ui/Strings.h"

#include "core/Json.h"
#include "core/StringUtil.h"
#include "platform/FileSystem.h"

namespace nebula {

Status Strings::loadFromJson(const std::string& text) {
  JsonValue json;
  Status s = parseJson(text, json);
  if (!s.ok()) return s;
  const JsonValue& strings = json.get("strings");
  if (!strings.isObject()) return Status(kParseError, "arquivo de idioma sem objeto 'strings'");
  std::map<std::string, std::string> table;
  for (size_t i = 0; i < strings.size(); ++i) {
    const JsonValue::Member& m = strings.memberAt(i);
    if (m.second.isString()) table[m.first] = m.second.asString();
  }
  table_.swap(table);
  language_ = json.get("language").asString("?");
  return Status::Ok();
}

Status Strings::loadFromFile(IFileSystem& fs, const std::string& path) {
  std::string text;
  Status s = fs.readFile(path, text, 1024 * 1024);
  if (!s.ok()) return s;
  return loadFromJson(text).withContext(path);
}

bool Strings::has(const std::string& key) const { return table_.find(key) != table_.end(); }

std::string Strings::get(const std::string& key) const {
  std::map<std::string, std::string>::const_iterator it = table_.find(key);
  return it == table_.end() ? key : it->second;
}

std::string Strings::format(const std::string& key, const std::string& a0,
                            const std::string& a1) const {
  std::string s = str::replaceAll(get(key), "{0}", a0);
  return str::replaceAll(s, "{1}", a1);
}

}  // namespace nebula
