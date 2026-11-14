#include "platform/win32/XInputMapping.h"

#if defined(_XBOX)
#include <xtl.h>
#else
#include <windows.h>
#include <xinput.h>
#endif

namespace nebula {

namespace {

float normalizeThumb(SHORT v) {
  // -32768..32767 -> -1..1 (a zona morta fica no InputMapper).
  float f = static_cast<float>(v) / 32767.0f;
  if (f < -1.0f) f = -1.0f;
  if (f > 1.0f) f = 1.0f;
  return f;
}

struct ButtonPair {
  WORD xinput;
  uint32_t ours;
};

const ButtonPair kButtons[] = {
    {XINPUT_GAMEPAD_A, kButtonA},
    {XINPUT_GAMEPAD_B, kButtonB},
    {XINPUT_GAMEPAD_X, kButtonX},
    {XINPUT_GAMEPAD_Y, kButtonY},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, kButtonLB},
    {XINPUT_GAMEPAD_RIGHT_SHOULDER, kButtonRB},
    {XINPUT_GAMEPAD_START, kButtonStart},
    {XINPUT_GAMEPAD_BACK, kButtonBack},
    {XINPUT_GAMEPAD_DPAD_UP, kButtonUp},
    {XINPUT_GAMEPAD_DPAD_DOWN, kButtonDown},
    {XINPUT_GAMEPAD_DPAD_LEFT, kButtonLeft},
    {XINPUT_GAMEPAD_DPAD_RIGHT, kButtonRight},
    {XINPUT_GAMEPAD_LEFT_THUMB, kButtonLS},
    {XINPUT_GAMEPAD_RIGHT_THUMB, kButtonRS}};

}  // namespace

InputState inputFromXInput(const XINPUT_GAMEPAD& pad) {
  InputState s;
  for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i) {
    if (pad.wButtons & kButtons[i].xinput) s.buttons |= kButtons[i].ours;
  }
  if (pad.bLeftTrigger >= kTriggerDigitalThreshold) s.buttons |= kButtonLT;
  if (pad.bRightTrigger >= kTriggerDigitalThreshold) s.buttons |= kButtonRT;
  s.leftX = normalizeThumb(pad.sThumbLX);
  s.leftY = normalizeThumb(pad.sThumbLY);  // XInput: positivo = cima (igual ao Nébula)
  return s;
}

}  // namespace nebula
