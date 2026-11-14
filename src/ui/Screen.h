// Telas e pilha de telas.
#pragma once

#include <vector>

#include "ui/Input.h"

namespace nebula {

class IRenderer;
struct FontSet;
struct Theme;
class Strings;
class TextureCache;
class IAppController;

struct UiContext {
  IRenderer* renderer;
  FontSet* fonts;
  const Theme* theme;
  const Strings* strings;
  TextureCache* textures;
  uint64_t nowMs;
  UiContext()
      : renderer(NULL), fonts(NULL), theme(NULL), strings(NULL), textures(NULL), nowMs(0) {}
};

class Screen {
 public:
  explicit Screen(IAppController& app) : app_(app) {}
  virtual ~Screen() {}
  virtual const char* name() const = 0;
  virtual void handleAction(Action action) = 0;
  virtual void update(uint64_t nowMs) { (void)nowMs; }
  virtual void render(UiContext& ctx) = 0;
  // Sobreposição: a tela de baixo continua visível.
  virtual bool isOverlay() const { return false; }

 protected:
  IAppController& app_;

 private:
  NB_NONCOPYABLE(Screen);
};

// A pilha é dona das telas. pop() adia a destruição para collectGarbage(),
// então uma tela pode se fechar de dentro do próprio handleAction().
class ScreenStack {
 public:
  ScreenStack() {}
  ~ScreenStack();

  void push(Screen* screen);
  void pop();
  void clear();
  void collectGarbage();
  void render(UiContext& ctx);

  Screen* top() const { return stack_.empty() ? NULL : stack_[stack_.size() - 1]; }
  size_t size() const { return stack_.size(); }
  Screen* at(size_t index) const { return stack_[index]; }
  bool contains(const Screen* screen) const;

 private:
  std::vector<Screen*> stack_;
  std::vector<Screen*> graveyard_;
  NB_NONCOPYABLE(ScreenStack);
};

}  // namespace nebula
