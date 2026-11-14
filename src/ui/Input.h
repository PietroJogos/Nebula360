// Entrada do controle do Xbox 360 (ou equivalente no PC), convertida em ações.
#pragma once

#include <vector>

#include "core/Types.h"

namespace nebula {

enum Button {
  kButtonA = 1 << 0,
  kButtonB = 1 << 1,
  kButtonX = 1 << 2,
  kButtonY = 1 << 3,
  kButtonLB = 1 << 4,
  kButtonRB = 1 << 5,
  kButtonLT = 1 << 6,  // gatilhos já convertidos para digital pela plataforma
  kButtonRT = 1 << 7,
  kButtonStart = 1 << 8,
  kButtonBack = 1 << 9,
  kButtonUp = 1 << 10,
  kButtonDown = 1 << 11,
  kButtonLeft = 1 << 12,
  kButtonRight = 1 << 13,
  kButtonLS = 1 << 14,
  kButtonRS = 1 << 15,
  kButtonGuide = 1 << 16
};

const int kButtonCount = 17;

struct InputState {
  uint32_t buttons;
  float leftX;  // -1 (esquerda) .. 1 (direita)
  float leftY;  // -1 (baixo) .. 1 (cima), como no XInput
  InputState() : buttons(0), leftX(0), leftY(0) {}
};

enum Action {
  kActionNone = 0,
  kActionUp,
  kActionDown,
  kActionLeft,
  kActionRight,
  kActionAccept,    // A
  kActionBack,      // B
  kActionX,         // X (pesquisar / apagar)
  kActionY,         // Y (favorito / espaço)
  kActionPrevTab,   // LB
  kActionNextTab,   // RB
  kActionPageUp,    // LT
  kActionPageDown,  // RT
  kActionMenu,      // Start
  kActionView       // Back (ordenar)
};

const char* actionName(Action action);

// Converte o estado bruto em ações: detecção de borda (apertou agora) e
// repetição automática para direcionais e gatilhos quando mantidos.
class InputMapper {
 public:
  static const uint32_t kRepeatDelayMs = 380;
  static const uint32_t kRepeatIntervalMs = 90;
  static const uint32_t kFastRepeatAfterMs = 1500;
  static const uint32_t kFastRepeatIntervalMs = 45;

  InputMapper();
  void update(const InputState& state, uint64_t nowMs, std::vector<Action>& out);
  void reset();

 private:
  uint32_t previous_;
  uint64_t pressedAt_[kButtonCount];
  uint64_t lastRepeat_[kButtonCount];
};

// Analógico esquerdo -> direcionais (zona morta de 0.55).
uint32_t stickToButtons(float x, float y);

}  // namespace nebula
