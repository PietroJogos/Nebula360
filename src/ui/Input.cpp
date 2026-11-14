#include "ui/Input.h"

namespace nebula {

namespace {

struct ButtonAction {
  uint32_t button;
  Action action;
  bool repeats;
};

const ButtonAction kMap[] = {
    {kButtonUp, kActionUp, true},          {kButtonDown, kActionDown, true},
    {kButtonLeft, kActionLeft, true},      {kButtonRight, kActionRight, true},
    {kButtonA, kActionAccept, false},      {kButtonB, kActionBack, false},
    {kButtonX, kActionX, false},           {kButtonY, kActionY, false},
    {kButtonLB, kActionPrevTab, false},    {kButtonRB, kActionNextTab, false},
    {kButtonLT, kActionPageUp, true},      {kButtonRT, kActionPageDown, true},
    {kButtonStart, kActionMenu, false},    {kButtonBack, kActionView, false}};

const float kStickDeadzone = 0.55f;

int bitIndex(uint32_t button) {
  int i = 0;
  while (button > 1) {
    button >>= 1;
    ++i;
  }
  return i;
}

}  // namespace

const char* actionName(Action a) {
  switch (a) {
    case kActionNone: return "none";
    case kActionUp: return "up";
    case kActionDown: return "down";
    case kActionLeft: return "left";
    case kActionRight: return "right";
    case kActionAccept: return "accept";
    case kActionBack: return "back";
    case kActionX: return "x";
    case kActionY: return "y";
    case kActionPrevTab: return "prevTab";
    case kActionNextTab: return "nextTab";
    case kActionPageUp: return "pageUp";
    case kActionPageDown: return "pageDown";
    case kActionMenu: return "menu";
    case kActionView: return "view";
  }
  return "?";
}

uint32_t stickToButtons(float x, float y) {
  uint32_t b = 0;
  if (x <= -kStickDeadzone) b |= kButtonLeft;
  if (x >= kStickDeadzone) b |= kButtonRight;
  if (y >= kStickDeadzone) b |= kButtonUp;
  if (y <= -kStickDeadzone) b |= kButtonDown;
  return b;
}

InputMapper::InputMapper() { reset(); }

void InputMapper::reset() {
  previous_ = 0;
  for (int i = 0; i < kButtonCount; ++i) {
    pressedAt_[i] = 0;
    lastRepeat_[i] = 0;
  }
}

void InputMapper::update(const InputState& state, uint64_t now, std::vector<Action>& out) {
  out.clear();
  uint32_t current = state.buttons | stickToButtons(state.leftX, state.leftY);
  for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
    uint32_t b = kMap[i].button;
    int idx = bitIndex(b);
    bool down = (current & b) != 0;
    bool wasDown = (previous_ & b) != 0;
    if (down && !wasDown) {
      pressedAt_[idx] = now;
      lastRepeat_[idx] = now;
      out.push_back(kMap[i].action);
    } else if (down && kMap[i].repeats) {
      uint64_t held = now - pressedAt_[idx];
      uint64_t interval = held >= kFastRepeatAfterMs ? kFastRepeatIntervalMs : kRepeatIntervalMs;
      if (held >= kRepeatDelayMs && now - lastRepeat_[idx] >= interval) {
        lastRepeat_[idx] = now;
        out.push_back(kMap[i].action);
      }
    }
  }
  previous_ = current;
}

}  // namespace nebula
