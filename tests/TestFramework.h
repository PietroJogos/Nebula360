// Mini framework de testes em C++03 (sem dependências externas).
#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace nbtest {

typedef void (*TestFn)();

struct TestCase {
  const char* name;
  const char* file;
  TestFn fn;
};

std::vector<TestCase>& registry();
void fail(const std::string& message, const char* file, int line);
void skip(const std::string& reason);
bool currentFailed();

struct Registrar {
  Registrar(const char* name, const char* file, TestFn fn) {
    TestCase t;
    t.name = name;
    t.file = file;
    t.fn = fn;
    registry().push_back(t);
  }
};

template <typename A, typename B>
bool checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
  if (a == b) return true;
  std::ostringstream os;
  os << ea << " == " << eb << "  (obtido: '" << a << "', esperado: '" << b << "')";
  fail(os.str(), file, line);
  return false;
}

}  // namespace nbtest

#define NB_TEST(name)                                                     \
  static void name();                                                     \
  static ::nbtest::Registrar name##_registrar(#name, __FILE__, &name);    \
  static void name()

#define CHECK(cond) \
  ((cond) ? true : (::nbtest::fail("CHECK(" #cond ")", __FILE__, __LINE__), false))

#define CHECK_EQ(a, b) ::nbtest::checkEq((a), (b), #a, #b, __FILE__, __LINE__)

#define REQUIRE(cond)                                            \
  do {                                                           \
    if (!(cond)) {                                               \
      ::nbtest::fail("REQUIRE(" #cond ")", __FILE__, __LINE__);  \
      return;                                                    \
    }                                                            \
  } while (0)

#define SKIP(reason)          \
  do {                        \
    ::nbtest::skip(reason);   \
    return;                   \
  } while (0)
