// Executor dos testes. Uso: nebula_tests [filtro]
#include <cstdio>
#include <cstring>

#include "TestFramework.h"

namespace nbtest {

namespace {
bool gFailed = false;
bool gSkipped = false;
std::string gSkipReason;
}  // namespace

std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void fail(const std::string& message, const char* file, int line) {
  gFailed = true;
  std::printf("    FALHA %s:%d: %s\n", file, line, message.c_str());
}

void skip(const std::string& reason) {
  gSkipped = true;
  gSkipReason = reason;
}

bool currentFailed() { return gFailed; }

}  // namespace nbtest

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : NULL;
  std::vector<nbtest::TestCase>& tests = nbtest::registry();
  int passed = 0, failed = 0, skipped = 0;
  for (size_t i = 0; i < tests.size(); ++i) {
    if (filter && std::strstr(tests[i].name, filter) == NULL) continue;
    nbtest::gFailed = false;
    nbtest::gSkipped = false;
    tests[i].fn();
    if (nbtest::gFailed) {
      ++failed;
      std::printf("[FALHOU] %s\n", tests[i].name);
    } else if (nbtest::gSkipped) {
      ++skipped;
      std::printf("[PULADO] %s (%s)\n", tests[i].name, nbtest::gSkipReason.c_str());
    } else {
      ++passed;
      std::printf("[ OK   ] %s\n", tests[i].name);
    }
  }
  std::printf("\n%d passaram, %d falharam, %d pulados (de %d)\n", passed, failed, skipped,
              passed + failed + skipped);
  return failed == 0 ? 0 : 1;
}
