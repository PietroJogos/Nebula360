// Janela SDL2 para PC: mostra o framebuffer 1280x720 e lê teclado ou um
// controle de Xbox 360 conectado ao computador (SDL GameController).
//
// Teclado: setas = direcional, Enter = A, Esc/Backspace = B, X = X, Y = Y,
// Q = LB, E = RB, PageUp = LT, PageDown = RT, Tab = Back, M = Start,
// F12 = captura de tela, F10 = sair.
#include <SDL.h>

#include <cstdlib>
#include <string>

#include "app/App.h"
#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
#include "platform/FileSystem.h"
#include "ui/Image.h"
#include "ui/SoftwareRenderer.h"

namespace nebula {

namespace {

uint32_t keyToButton(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: return kButtonUp;
    case SDLK_DOWN: return kButtonDown;
    case SDLK_LEFT: return kButtonLeft;
    case SDLK_RIGHT: return kButtonRight;
    case SDLK_RETURN: return kButtonA;
    case SDLK_ESCAPE: return kButtonB;
    case SDLK_BACKSPACE: return kButtonB;
    case SDLK_x: return kButtonX;
    case SDLK_y: return kButtonY;
    case SDLK_q: return kButtonLB;
    case SDLK_e: return kButtonRB;
    case SDLK_PAGEUP: return kButtonLT;
    case SDLK_PAGEDOWN: return kButtonRT;
    case SDLK_TAB: return kButtonBack;
    case SDLK_m: return kButtonStart;
    default: return 0;
  }
}

uint32_t controllerButtons(SDL_GameController* pad, float& lx, float& ly) {
  uint32_t b = 0;
  if (pad == NULL) return 0;
  struct Map {
    SDL_GameControllerButton sdl;
    uint32_t ours;
  };
  static const Map kMap[] = {
      {SDL_CONTROLLER_BUTTON_A, kButtonA},          {SDL_CONTROLLER_BUTTON_B, kButtonB},
      {SDL_CONTROLLER_BUTTON_X, kButtonX},          {SDL_CONTROLLER_BUTTON_Y, kButtonY},
      {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, kButtonLB},
      {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, kButtonRB},
      {SDL_CONTROLLER_BUTTON_START, kButtonStart},  {SDL_CONTROLLER_BUTTON_BACK, kButtonBack},
      {SDL_CONTROLLER_BUTTON_DPAD_UP, kButtonUp},   {SDL_CONTROLLER_BUTTON_DPAD_DOWN, kButtonDown},
      {SDL_CONTROLLER_BUTTON_DPAD_LEFT, kButtonLeft},
      {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, kButtonRight},
      {SDL_CONTROLLER_BUTTON_LEFTSTICK, kButtonLS}, {SDL_CONTROLLER_BUTTON_RIGHTSTICK, kButtonRS},
      {SDL_CONTROLLER_BUTTON_GUIDE, kButtonGuide}};
  for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
    if (SDL_GameControllerGetButton(pad, kMap[i].sdl)) b |= kMap[i].ours;
  }
  if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) b |= kButtonLT;
  if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) b |= kButtonRT;
  lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
  // SDL: Y positivo para baixo; XInput (e o Nébula): positivo para cima.
  ly = -SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
  return b;
}

}  // namespace

int runSdlFrontend(App& app, SoftwareRenderer& renderer, IFileSystem& fs,
                   const std::string& screenshotDir) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    NB_ERROR("sdl", "SDL_Init falhou: " << SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("Nébula 360", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kScreenWidth, kScreenHeight, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* sdl = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
  if (sdl == NULL && window != NULL) sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  SDL_Texture* tex = sdl ? SDL_CreateTexture(sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                             kScreenWidth, kScreenHeight)
                         : NULL;
  if (tex == NULL) {
    NB_ERROR("sdl", "não foi possível criar a janela: " << SDL_GetError());
    if (sdl) SDL_DestroyRenderer(sdl);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(sdl, kScreenWidth, kScreenHeight);

  SDL_GameController* pad = NULL;
  uint32_t keys = 0;
  bool running = true;
  int shots = 0;
  const char* maxFrames = SDL_getenv("NEBULA_MAX_FRAMES");  // para testes automáticos
  long frameLimit = maxFrames ? std::atol(maxFrames) : -1;
  long frame = 0;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
      if (ev.type == SDL_CONTROLLERDEVICEADDED && pad == NULL) {
        pad = SDL_GameControllerOpen(ev.cdevice.which);
        if (pad) NB_INFO("sdl", "controle conectado: " << SDL_GameControllerName(pad));
      }
      if (ev.type == SDL_CONTROLLERDEVICEREMOVED && pad != NULL &&
          ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
        SDL_GameControllerClose(pad);
        pad = NULL;
      }
      if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        if (ev.key.keysym.sym == SDLK_F10) running = false;
        if (ev.key.keysym.sym == SDLK_F12) {
          std::string png;
          std::string file = path::join(screenshotDir, "captura-" + str::toString(++shots) + ".png", '/');
          if (encodePng(renderer.framebuffer(), png).ok() && fs.writeFileAtomic(file, png).ok()) {
            NB_INFO("sdl", "captura salva: " << file);
          }
        }
        keys |= keyToButton(ev.key.keysym.sym);
      }
      if (ev.type == SDL_KEYUP) keys &= ~keyToButton(ev.key.keysym.sym);
    }
    InputState input;
    input.buttons = keys | controllerButtons(pad, input.leftX, input.leftY);
    app.tick(input);
    app.render(renderer);
    const Image& fb = renderer.framebuffer();
    SDL_UpdateTexture(tex, NULL, &fb.pixels[0], fb.width * 4);
    SDL_RenderClear(sdl);
    SDL_RenderCopy(sdl, tex, NULL, NULL);
    SDL_RenderPresent(sdl);
    ++frame;
    if (frameLimit >= 0 && frame >= frameLimit) running = false;
  }
  if (pad) SDL_GameControllerClose(pad);
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(sdl);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace nebula
