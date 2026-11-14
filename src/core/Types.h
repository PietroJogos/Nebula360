// Nébula 360 - tipos e macros base.
//
// Regra do projeto: todo o código em src/ compila como C++03 estrito
// (o compilador do XDK do Xbox 360 é o MSVC 2010). Nada de auto, lambdas,
// nullptr, range-for, enum class ou bibliotecas C++11.
#pragma once

#include <stdint.h>
#include <cstddef>

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1700)
#define NB_OVERRIDE override
#else
#define NB_OVERRIDE
#endif

// Torna a classe não copiável (C++03: declara cópia privada sem definição).
#define NB_NONCOPYABLE(ClassName)            \
 private:                                    \
  ClassName(const ClassName&);               \
  ClassName& operator=(const ClassName&)

namespace nebula {

// Plataforma em que o código está rodando, informada pelo frontend.
enum PlatformKind {
  kPlatformHost = 0,    // PC (Linux/Windows) - desenvolvimento e testes
  kPlatformXbox360 = 1  // console Xbox 360 (RGH/JTAG) via XDK
};

}  // namespace nebula
