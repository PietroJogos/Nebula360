// Tratamento de erros sem exceções: Status e Result<T>.
#pragma once

#include <string>

#include "core/Types.h"

namespace nebula {

enum ErrorCode {
  kOk = 0,
  kNotFound,
  kIoError,
  kParseError,
  kInvalidArgument,
  kUnsupported,
  kIntegrityError,
  kPolicyViolation,
  kLimitExceeded,
  kCancelled,
  kInternal
};

const char* errorCodeName(ErrorCode code);

class Status {
 public:
  Status() : code_(kOk) {}
  Status(ErrorCode code, const std::string& message)
      : code_(code), message_(message) {}

  static Status Ok() { return Status(); }

  bool ok() const { return code_ == kOk; }
  ErrorCode code() const { return code_; }
  const std::string& message() const { return message_; }

  // Retorna uma cópia com contexto prefixado: "contexto: mensagem".
  Status withContext(const std::string& context) const;

  // "OK" ou "CODIGO: mensagem".
  std::string toString() const;

 private:
  ErrorCode code_;
  std::string message_;
};

// Valor ou erro. T precisa ter construtor padrão.
template <typename T>
class Result {
 public:
  Result(const T& value) : value_(value), status_() {}
  Result(const Status& status) : value_(), status_(status) {
    if (status_.ok()) {
      status_ = Status(kInternal, "Result construído com Status OK sem valor");
    }
  }

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  const T& value() const { return value_; }
  T& value() { return value_; }

 private:
  T value_;
  Status status_;
};

}  // namespace nebula
