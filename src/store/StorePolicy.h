// Regras de licença e de instalação da loja.
#pragma once

#include <string>

#include "core/Status.h"

namespace nebula {

struct CatalogItem;

namespace store {

// Lista de licenças livres aceitas sem autorização adicional (SPDX).
bool isApprovedFreeLicense(const std::string& spdx);
// Licenças que exigem publicar o código-fonte (sourceUrl obrigatório).
bool isCopyleftLicense(const std::string& spdx);

// Avalia um item contra a política da loja. kPolicyViolation se recusado.
Status evaluateItemPolicy(const CatalogItem& item);

// FATX (sistema de arquivos do Xbox 360): nomes de 1 a 42 caracteres, com
// um conjunto restrito de caracteres. Usamos um conjunto conservador.
const size_t kFatxMaxNameLength = 42;
bool isValidFatxName(const std::string& name);

// Caminho relativo seguro: componentes separados por '/', nenhum vazio,
// sem "." / "..", sem dispositivo (":"), cada componente válido no FATX.
Status validateInstallRelativePath(const std::string& relative);

// <installRoot>\<itemId>\<relative convertido para o separador local>.
std::string buildInstallPath(const std::string& installRoot, const std::string& itemId,
                             const std::string& relative, char sep);

}  // namespace store
}  // namespace nebula
