// JSON mínimo (C++03), usado para configuração, biblioteca, temas e catálogo.
//
// - Objetos preservam a ordem de inserção (arquivos legíveis e estáveis no git).
// - O parser limita a profundidade de aninhamento e informa linha/coluna do erro.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class JsonValue {
 public:
  enum Type { kNull, kBool, kNumber, kString, kArray, kObject };
  typedef std::pair<std::string, JsonValue> Member;

  JsonValue();
  JsonValue(bool value);
  JsonValue(int value);
  JsonValue(int64_t value);
  JsonValue(uint32_t value);
  JsonValue(double value);
  JsonValue(const char* value);
  JsonValue(const std::string& value);
  JsonValue(const JsonValue& other);
  JsonValue& operator=(const JsonValue& other);
  ~JsonValue();

  static JsonValue array();
  static JsonValue object();

  Type type() const { return type_; }
  bool isNull() const { return type_ == kNull; }
  bool isBool() const { return type_ == kBool; }
  bool isNumber() const { return type_ == kNumber; }
  bool isString() const { return type_ == kString; }
  bool isArray() const { return type_ == kArray; }
  bool isObject() const { return type_ == kObject; }

  bool asBool(bool fallback = false) const;
  double asNumber(double fallback = 0.0) const;
  int64_t asInt(int64_t fallback = 0) const;
  const std::string& asString() const;  // "" se não for string
  std::string asString(const std::string& fallback) const;

  // Arrays e objetos.
  size_t size() const;
  const JsonValue& at(size_t index) const;  // null estático se fora do limite
  JsonValue& push(const JsonValue& value);  // converte null em array

  bool has(const std::string& key) const;
  const JsonValue& get(const std::string& key) const;  // null se ausente
  JsonValue& set(const std::string& key, const JsonValue& value);  // null->objeto
  bool remove(const std::string& key);
  const Member& memberAt(size_t index) const;

  std::string serialize(bool pretty) const;

  static const JsonValue& nullValue();

 private:
  void destroy();
  void copyFrom(const JsonValue& other);

  Type type_;
  bool bool_;
  double number_;
  std::string string_;
  std::vector<JsonValue>* array_;
  std::vector<Member>* object_;
};

struct JsonParseOptions {
  size_t maxDepth;
  size_t maxInputBytes;
  JsonParseOptions() : maxDepth(64), maxInputBytes(16 * 1024 * 1024) {}
};

Status parseJson(const std::string& text, JsonValue& out,
                 const JsonParseOptions& options = JsonParseOptions());

}  // namespace nebula
