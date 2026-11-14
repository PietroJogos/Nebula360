// Módulos embutidos do Nébula 360 e seus manifestos.
//
// Só entram aqui módulos JÁ implementados. Os planejados (FTP, HTTP, loja,
// mídia, emuladores...) estão descritos em docs/fases/PLANO_IMPLEMENTACAO_PLUGINS.md
// e serão registrados quando existirem.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"

namespace nebula {

class ModuleRegistry;

// Textos JSON dos manifestos embutidos (expostos para testes e documentação).
const std::vector<std::string>& builtinManifestTexts();

// Registra todos os módulos embutidos. Manifestos inválidos são registrados
// no log e ignorados; o retorno informa o primeiro erro, se houver.
Status registerBuiltinModules(ModuleRegistry& registry);

}  // namespace nebula
