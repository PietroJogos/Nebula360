#include "ui/Screen.h"

namespace nebula {

ScreenStack::~ScreenStack() {
  clear();
  collectGarbage();
}

void ScreenStack::push(Screen* screen) {
  if (screen != NULL) stack_.push_back(screen);
}

void ScreenStack::pop() {
  if (stack_.empty()) return;
  graveyard_.push_back(stack_[stack_.size() - 1]);
  stack_.pop_back();
}

void ScreenStack::clear() {
  while (!stack_.empty()) pop();
}

void ScreenStack::collectGarbage() {
  for (size_t i = 0; i < graveyard_.size(); ++i) delete graveyard_[i];
  graveyard_.clear();
}

bool ScreenStack::contains(const Screen* screen) const {
  for (size_t i = 0; i < stack_.size(); ++i) {
    if (stack_[i] == screen) return true;
  }
  return false;
}

void ScreenStack::render(UiContext& ctx) {
  if (stack_.empty()) return;
  size_t first = stack_.size() - 1;
  while (first > 0 && stack_[first]->isOverlay()) --first;
  for (size_t i = first; i < stack_.size(); ++i) stack_[i]->render(ctx);
}

}  // namespace nebula
