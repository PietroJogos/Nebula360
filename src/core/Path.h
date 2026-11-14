// Manipulação de caminhos com separador explícito.
//
// Xbox 360: "Hdd1:\Games\Jogo" (sep '\\', raiz de dispositivo "Hdd1:").
// PC:       "/home/u/jogos"     (sep '/').
#pragma once

#include <string>

namespace nebula {
namespace path {

std::string join(const std::string& base, const std::string& name, char sep);
std::string parent(const std::string& p, char sep);
std::string fileName(const std::string& p, char sep);
// Extensão em minúsculas, sem o ponto ("xex"), ou "" se não houver.
std::string extensionLower(const std::string& p, char sep);
std::string stem(const std::string& p, char sep);
// Remove separadores finais (exceto na raiz "/" ou "Hdd1:\").
std::string stripTrailingSeparator(const std::string& p, char sep);
// "p" está dentro de "root" (ou é igual)? Comparação sem diferenciar
// maiúsculas, como no FATX do Xbox 360.
bool isWithin(const std::string& p, const std::string& root, char sep);

}  // namespace path
}  // namespace nebula
