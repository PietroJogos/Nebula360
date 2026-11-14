#include "core/Json.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

#include "core/StringUtil.h"

namespace nebula {

JsonValue::JsonValue()
    : type_(kNull), bool_(false), number_(0), array_(NULL), object_(NULL) {}
JsonValue::JsonValue(bool v)
    : type_(kBool), bool_(v), number_(0), array_(NULL), object_(NULL) {}
JsonValue::JsonValue(int v)
    : type_(kNumber), bool_(false), number_(v), array_(NULL), object_(NULL) {}
JsonValue::JsonValue(int64_t v)
    : type_(kNumber), bool_(false), number_(static_cast<double>(v)),
      array_(NULL), object_(NULL) {}
JsonValue::JsonValue(uint32_t v)
    : type_(kNumber), bool_(false), number_(static_cast<double>(v)),
      array_(NULL), object_(NULL) {}
JsonValue::JsonValue(double v)
    : type_(kNumber), bool_(false), number_(v), array_(NULL), object_(NULL) {}
JsonValue::JsonValue(const char* v)
    : type_(kString), bool_(false), number_(0), string_(v ? v : ""),
      array_(NULL), object_(NULL) {}
JsonValue::JsonValue(const std::string& v)
    : type_(kString), bool_(false), number_(0), string_(v), array_(NULL),
      object_(NULL) {}

JsonValue::JsonValue(const JsonValue& other)
    : type_(kNull), bool_(false), number_(0), array_(NULL), object_(NULL) {
  copyFrom(other);
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
  if (this != &other) {
    // Copia primeiro: "other" pode ser filho deste valor.
    JsonValue tmp(other);
    destroy();
    copyFrom(tmp);
  }
  return *this;
}

JsonValue::~JsonValue() { destroy(); }

void JsonValue::destroy() {
  delete array_;
  delete object_;
  array_ = NULL;
  object_ = NULL;
  string_.clear();
  type_ = kNull;
}

void JsonValue::copyFrom(const JsonValue& other) {
  type_ = other.type_;
  bool_ = other.bool_;
  number_ = other.number_;
  string_ = other.string_;
  array_ = other.array_ ? new std::vector<JsonValue>(*other.array_) : NULL;
  object_ = other.object_ ? new std::vector<Member>(*other.object_) : NULL;
}

JsonValue JsonValue::array() {
  JsonValue v;
  v.type_ = kArray;
  v.array_ = new std::vector<JsonValue>();
  return v;
}

JsonValue JsonValue::object() {
  JsonValue v;
  v.type_ = kObject;
  v.object_ = new std::vector<Member>();
  return v;
}

const JsonValue& JsonValue::nullValue() {
  static const JsonValue kNullValue;
  return kNullValue;
}

bool JsonValue::asBool(bool fallback) const {
  return type_ == kBool ? bool_ : fallback;
}

double JsonValue::asNumber(double fallback) const {
  return type_ == kNumber ? number_ : fallback;
}

int64_t JsonValue::asInt(int64_t fallback) const {
  if (type_ != kNumber) return fallback;
  if (number_ != number_) return fallback;  // NaN
  if (number_ > 9.2e18 || number_ < -9.2e18) return fallback;
  return static_cast<int64_t>(number_);
}

const std::string& JsonValue::asString() const {
  static const std::string kEmpty;
  return type_ == kString ? string_ : kEmpty;
}

std::string JsonValue::asString(const std::string& fallback) const {
  return type_ == kString ? string_ : fallback;
}

size_t JsonValue::size() const {
  if (type_ == kArray) return array_->size();
  if (type_ == kObject) return object_->size();
  return 0;
}

const JsonValue& JsonValue::at(size_t index) const {
  if (type_ != kArray || index >= array_->size()) return nullValue();
  return (*array_)[index];
}

JsonValue& JsonValue::push(const JsonValue& value) {
  if (type_ != kArray) {
    destroy();
    type_ = kArray;
    array_ = new std::vector<JsonValue>();
  }
  array_->push_back(value);
  return (*array_)[array_->size() - 1];
}

bool JsonValue::has(const std::string& key) const {
  if (type_ != kObject) return false;
  for (size_t i = 0; i < object_->size(); ++i) {
    if ((*object_)[i].first == key) return true;
  }
  return false;
}

const JsonValue& JsonValue::get(const std::string& key) const {
  if (type_ != kObject) return nullValue();
  for (size_t i = 0; i < object_->size(); ++i) {
    if ((*object_)[i].first == key) return (*object_)[i].second;
  }
  return nullValue();
}

JsonValue& JsonValue::set(const std::string& key, const JsonValue& value) {
  if (type_ != kObject) {
    destroy();
    type_ = kObject;
    object_ = new std::vector<Member>();
  }
  for (size_t i = 0; i < object_->size(); ++i) {
    if ((*object_)[i].first == key) {
      (*object_)[i].second = value;
      return (*object_)[i].second;
    }
  }
  object_->push_back(Member(key, value));
  return (*object_)[object_->size() - 1].second;
}

bool JsonValue::remove(const std::string& key) {
  if (type_ != kObject) return false;
  for (size_t i = 0; i < object_->size(); ++i) {
    if ((*object_)[i].first == key) {
      object_->erase(object_->begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

const JsonValue::Member& JsonValue::memberAt(size_t index) const {
  static const Member kEmpty;
  if (type_ != kObject || index >= object_->size()) return kEmpty;
  return (*object_)[index];
}

// ---------------------------------------------------------------- escrita

namespace {

void writeString(std::string& out, const std::string& s) {
  static const char kHex[] = "0123456789abcdef";
  out += '"';
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += kHex[c >> 4];
          out += kHex[c & 0xF];
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += '"';
}

void writeNumber(std::string& out, double n) {
  if (n != n || n > 1.7e308 || n < -1.7e308) {
    out += "null";  // JSON não representa NaN/infinito
    return;
  }
  double integral = std::floor(n);
  if (integral == n && n < 9007199254740992.0 && n > -9007199254740992.0) {
    out += str::toString(static_cast<int64_t>(n));
    return;
  }
  std::ostringstream os;
  os.precision(17);
  os << n;
  out += os.str();
}

void indent(std::string& out, int depth) {
  out += '\n';
  for (int i = 0; i < depth; ++i) out += "  ";
}

void writeValue(std::string& out, const JsonValue& v, bool pretty, int depth) {
  switch (v.type()) {
    case JsonValue::kNull: out += "null"; break;
    case JsonValue::kBool: out += v.asBool() ? "true" : "false"; break;
    case JsonValue::kNumber: writeNumber(out, v.asNumber()); break;
    case JsonValue::kString: writeString(out, v.asString()); break;
    case JsonValue::kArray: {
      if (v.size() == 0) {
        out += "[]";
        break;
      }
      out += '[';
      for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) out += ',';
        if (pretty) indent(out, depth + 1);
        writeValue(out, v.at(i), pretty, depth + 1);
      }
      if (pretty) indent(out, depth);
      out += ']';
      break;
    }
    case JsonValue::kObject: {
      if (v.size() == 0) {
        out += "{}";
        break;
      }
      out += '{';
      for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) out += ',';
        if (pretty) indent(out, depth + 1);
        writeString(out, v.memberAt(i).first);
        out += pretty ? ": " : ":";
        writeValue(out, v.memberAt(i).second, pretty, depth + 1);
      }
      if (pretty) indent(out, depth);
      out += '}';
      break;
    }
  }
}

}  // namespace

std::string JsonValue::serialize(bool pretty) const {
  std::string out;
  writeValue(out, *this, pretty, 0);
  if (pretty) out += '\n';
  return out;
}

// ---------------------------------------------------------------- leitura

namespace {

class Parser {
 public:
  Parser(const std::string& text, const JsonParseOptions& options)
      : text_(text), pos_(0), options_(options) {}

  Status parseDocument(JsonValue& out) {
    // Ignora BOM UTF-8.
    if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xEF &&
        static_cast<unsigned char>(text_[1]) == 0xBB &&
        static_cast<unsigned char>(text_[2]) == 0xBF) {
      pos_ = 3;
    }
    skipWhitespace();
    Status s = parseValue(out, 0);
    if (!s.ok()) return s;
    skipWhitespace();
    if (pos_ != text_.size()) return error("conteúdo extra após o valor JSON");
    return Status::Ok();
  }

 private:
  Status error(const std::string& what) const {
    size_t line = 1;
    size_t col = 1;
    for (size_t i = 0; i < pos_ && i < text_.size(); ++i) {
      if (text_[i] == '\n') {
        ++line;
        col = 1;
      } else {
        ++col;
      }
    }
    return Status(kParseError, what + " (linha " + str::toString(static_cast<uint64_t>(line)) +
                                   ", coluna " + str::toString(static_cast<uint64_t>(col)) + ")");
  }

  void skipWhitespace() {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool consumeLiteral(const char* lit) {
    size_t n = 0;
    while (lit[n] != '\0') ++n;
    if (text_.compare(pos_, n, lit) == 0) {
      pos_ += n;
      return true;
    }
    return false;
  }

  Status parseValue(JsonValue& out, size_t depth) {
    if (depth > options_.maxDepth) return error("aninhamento JSON profundo demais");
    if (pos_ >= text_.size()) return error("fim inesperado do JSON");
    char c = text_[pos_];
    if (c == '{') return parseObject(out, depth);
    if (c == '[') return parseArray(out, depth);
    if (c == '"') {
      std::string s;
      Status st = parseString(s);
      if (!st.ok()) return st;
      out = JsonValue(s);
      return Status::Ok();
    }
    if (c == 't') {
      if (consumeLiteral("true")) { out = JsonValue(true); return Status::Ok(); }
      return error("literal inválido");
    }
    if (c == 'f') {
      if (consumeLiteral("false")) { out = JsonValue(false); return Status::Ok(); }
      return error("literal inválido");
    }
    if (c == 'n') {
      if (consumeLiteral("null")) { out = JsonValue(); return Status::Ok(); }
      return error("literal inválido");
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
    return error(std::string("caractere inesperado '") + c + "'");
  }

  Status parseNumber(JsonValue& out) {
    size_t start = pos_;
    if (text_[pos_] == '-') ++pos_;
    if (pos_ >= text_.size()) return error("número inválido");
    if (text_[pos_] == '0') {
      ++pos_;
    } else if (text_[pos_] >= '1' && text_[pos_] <= '9') {
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    } else {
      return error("número inválido");
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      size_t digits = pos_;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
      if (pos_ == digits) return error("número inválido");
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
      size_t digits = pos_;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
      if (pos_ == digits) return error("número inválido");
    }
    std::string token = text_.substr(start, pos_ - start);
    // O laço do Nébula nunca chama setlocale, então strtod usa o locale "C".
    double value = std::strtod(token.c_str(), NULL);
    out = JsonValue(value);
    return Status::Ok();
  }

  Status parseHex4(uint32_t& out) {
    if (pos_ + 4 > text_.size()) return error("escape \\u incompleto");
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = text_[pos_++];
      uint32_t d = 0;
      if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
      else return error("escape \\u inválido");
      v = (v << 4) | d;
    }
    out = v;
    return Status::Ok();
  }

  Status parseString(std::string& out) {
    ++pos_;  // aspas iniciais
    while (true) {
      if (pos_ >= text_.size()) return error("string não terminada");
      char c = text_[pos_++];
      if (c == '"') return Status::Ok();
      if (static_cast<unsigned char>(c) < 0x20) {
        return error("caractere de controle dentro de string");
      }
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= text_.size()) return error("escape incompleto");
      char e = text_[pos_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          uint32_t cp = 0;
          Status s = parseHex4(cp);
          if (!s.ok()) return s;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (pos_ + 1 < text_.size() && text_[pos_] == '\\' &&
                text_[pos_ + 1] == 'u') {
              pos_ += 2;
              uint32_t lo = 0;
              s = parseHex4(lo);
              if (!s.ok()) return s;
              if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              } else {
                cp = 0xFFFD;
              }
            } else {
              cp = 0xFFFD;
            }
          }
          str::appendUtf8(out, cp);
          break;
        }
        default:
          return error("escape inválido em string");
      }
    }
  }

  Status parseArray(JsonValue& out, size_t depth) {
    ++pos_;
    out = JsonValue::array();
    skipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      return Status::Ok();
    }
    while (true) {
      skipWhitespace();
      JsonValue item;
      Status s = parseValue(item, depth + 1);
      if (!s.ok()) return s;
      out.push(item);
      skipWhitespace();
      if (pos_ >= text_.size()) return error("array não terminado");
      char c = text_[pos_++];
      if (c == ']') return Status::Ok();
      if (c != ',') return error("esperado ',' ou ']' no array");
    }
  }

  Status parseObject(JsonValue& out, size_t depth) {
    ++pos_;
    out = JsonValue::object();
    skipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      return Status::Ok();
    }
    while (true) {
      skipWhitespace();
      if (pos_ >= text_.size() || text_[pos_] != '"') {
        return error("esperada chave entre aspas no objeto");
      }
      std::string key;
      Status s = parseString(key);
      if (!s.ok()) return s;
      skipWhitespace();
      if (pos_ >= text_.size() || text_[pos_] != ':') return error("esperado ':' no objeto");
      ++pos_;
      skipWhitespace();
      JsonValue value;
      s = parseValue(value, depth + 1);
      if (!s.ok()) return s;
      out.set(key, value);
      skipWhitespace();
      if (pos_ >= text_.size()) return error("objeto não terminado");
      char c = text_[pos_++];
      if (c == '}') return Status::Ok();
      if (c != ',') return error("esperado ',' ou '}' no objeto");
    }
  }

  const std::string& text_;
  size_t pos_;
  JsonParseOptions options_;
};

}  // namespace

Status parseJson(const std::string& text, JsonValue& out,
                 const JsonParseOptions& options) {
  if (text.size() > options.maxInputBytes) {
    return Status(kLimitExceeded, "JSON maior que o limite permitido");
  }
  Parser parser(text, options);
  JsonValue result;
  Status s = parser.parseDocument(result);
  if (!s.ok()) return s;
  out = result;
  return Status::Ok();
}

}  // namespace nebula
