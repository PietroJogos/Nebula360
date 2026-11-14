// Conversão do estado XInput (mesma API no Windows e no XDK do Xbox 360)
// para o InputState do Nébula.
//
// Verificado apenas por compilação contra os cabeçalhos públicos do MinGW
// (ver docs/fases/FASE_2.md). Não foi executado em um console.
#pragma once

#include "ui/Input.h"

struct _XINPUT_GAMEPAD;

namespace nebula {

// Gatilho analógico (0..255) considerado "apertado" a partir deste valor.
const unsigned kTriggerDigitalThreshold = 128;

InputState inputFromXInput(const _XINPUT_GAMEPAD& pad);

}  // namespace nebula
