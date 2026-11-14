#include "core/Status.h"

namespace nebula {

const char* errorCodeName(ErrorCode code) {
  switch (code) {
    case kOk: return "OK";
    case kNotFound: return "NOT_FOUND";
    case kIoError: return "IO_ERROR";
    case kParseError: return "PARSE_ERROR";
    case kInvalidArgument: return "INVALID_ARGUMENT";
    case kUnsupported: return "UNSUPPORTED";
    case kIntegrityError: return "INTEGRITY_ERROR";
    case kPolicyViolation: return "POLICY_VIOLATION";
    case kLimitExceeded: return "LIMIT_EXCEEDED";
    case kCancelled: return "CANCELLED";
    case kInternal: return "INTERNAL";
  }
  return "UNKNOWN";
}

Status Status::withContext(const std::string& context) const {
  if (ok()) return *this;
  return Status(code_, context + ": " + message_);
}

std::string Status::toString() const {
  if (ok()) return "OK";
  return std::string(errorCodeName(code_)) + ": " + message_;
}

}  // namespace nebula
