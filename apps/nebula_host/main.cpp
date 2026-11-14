// Nébula 360 - frontend para PC.
//
// Modos:
//   nebula360                       janela SDL2 (se compilado com SDL2)
//   nebula360 --headless ...        sem janela, determinístico (testes e capturas)
//
// Opções:
//   --data DIR          pasta de dados (padrão: ./nebula_data)
//   --assets DIR        pasta de recursos (padrão: assets/ do código-fonte)
//   --add-path PATH     adiciona uma pasta de jogos e inicia a varredura
//   --headless          sem janela
//   --frames N          (headless) quadros a simular (padrão: 120)
//   --script LISTA      (headless) roteiro de botões, separado por vírgulas:
//                         a b x y lb rb lt rt start back up down left right
//                         wait:N   espera N quadros
//                         shot:ARQ salva captura PNG
//   --screenshot ARQ    salva uma captura ao terminar
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/App.h"
#include "core/Log.h"
#include "core/StringUtil.h"
#include "platform/Launcher.h"
#include "platform/host/PosixFileSystem.h"
#include "ui/Image.h"
#include "ui/SoftwareRenderer.h"

#ifndef NEBULA_DEFAULT_ASSETS
#define NEBULA_DEFAULT_ASSETS "assets"
#endif

namespace nebula {
#ifdef NEBULA_HAVE_SDL2
int runSdlFrontend(App& app, SoftwareRenderer& renderer, IFileSystem& fs,
                   const std::string& screenshotDir);
#endif
}  // namespace nebula

using namespace nebula;

namespace {

struct Options {
  std::string dataDir;
  std::string assetsDir;
  std::vector<std::string> addPaths;
  bool headless;
  int frames;
  std::string script;
  std::string screenshot;
  Options() : dataDir("nebula_data"), assetsDir(NEBULA_DEFAULT_ASSETS), headless(false), frames(120) {}
};

bool parseArgs(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    bool hasValue = i + 1 < argc;
    if (a == "--data" && hasValue) o.dataDir = argv[++i];
    else if (a == "--assets" && hasValue) o.assetsDir = argv[++i];
    else if (a == "--add-path" && hasValue) o.addPaths.push_back(argv[++i]);
    else if (a == "--headless") o.headless = true;
    else if (a == "--frames" && hasValue) o.frames = std::atoi(argv[++i]);
    else if (a == "--script" && hasValue) o.script = argv[++i];
    else if (a == "--screenshot" && hasValue) o.screenshot = argv[++i];
    else {
      std::fprintf(stderr, "opção desconhecida: %s\n", a.c_str());
      return false;
    }
  }
  return true;
}

uint32_t buttonFromName(const std::string& n) {
  if (n == "a") return kButtonA;
  if (n == "b") return kButtonB;
  if (n == "x") return kButtonX;
  if (n == "y") return kButtonY;
  if (n == "lb") return kButtonLB;
  if (n == "rb") return kButtonRB;
  if (n == "lt") return kButtonLT;
  if (n == "rt") return kButtonRT;
  if (n == "start") return kButtonStart;
  if (n == "back") return kButtonBack;
  if (n == "up") return kButtonUp;
  if (n == "down") return kButtonDown;
  if (n == "left") return kButtonLeft;
  if (n == "right") return kButtonRight;
  return 0;
}

Status saveScreenshot(IFileSystem& fs, const SoftwareRenderer& r, const std::string& file) {
  std::string png;
  Status s = encodePng(r.framebuffer(), png);
  if (s.ok()) s = fs.writeFileAtomic(file, png);
  if (s.ok()) NB_INFO("host", "captura salva: " << file);
  return s;
}

// Simula quadros de 16 ms com o relógio manual (resultado determinístico).
int runHeadless(App& app, SoftwareRenderer& renderer, ManualClock& clock, IFileSystem& fs,
                const Options& o) {
  std::vector<std::string> steps = str::split(o.script, ',');
  int framesRun = 0;
  InputState idle;
  for (size_t i = 0; i < steps.size(); ++i) {
    std::string step = str::toLowerAscii(str::trim(steps[i]));
    if (step.empty()) continue;
    if (str::startsWith(step, "wait:")) {
      int n = std::atoi(step.c_str() + 5);
      for (int k = 0; k < n; ++k, ++framesRun) {
        app.tick(idle);
        clock.advance(16);
      }
      continue;
    }
    if (str::startsWith(step, "shot:")) {
      app.render(renderer);
      Status s = saveScreenshot(fs, renderer, str::trim(steps[i]).substr(5));
      if (!s.ok()) {
        std::fprintf(stderr, "%s\n", s.toString().c_str());
        return 1;
      }
      continue;
    }
    uint32_t b = buttonFromName(step);
    if (b == 0) {
      std::fprintf(stderr, "passo de roteiro desconhecido: %s\n", step.c_str());
      return 2;
    }
    if (app.quitRequested()) break;
    InputState pressed;
    pressed.buttons = b;
    for (int k = 0; k < 2; ++k, ++framesRun) {
      app.tick(pressed);
      clock.advance(16);
    }
    for (int k = 0; k < 4; ++k, ++framesRun) {
      app.tick(idle);
      clock.advance(16);
    }
  }
  for (; framesRun < o.frames && !app.quitRequested(); ++framesRun) {
    app.tick(idle);
    clock.advance(16);
  }
  app.render(renderer);
  if (!o.screenshot.empty()) {
    Status s = saveScreenshot(fs, renderer, o.screenshot);
    if (!s.ok()) {
      std::fprintf(stderr, "%s\n", s.toString().c_str());
      return 1;
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  if (!parseArgs(argc, argv, o)) return 2;

  StderrLogSink stderrLog;
  Log::addSink(&stderrLog);

  PosixFileSystem fs;
  UnsupportedLauncher launcher;
  SoftwareRenderer renderer;
  ManualClock manualClock;
  SystemClock systemClock;
  IClock& clock = o.headless ? static_cast<IClock&>(manualClock) : static_cast<IClock&>(systemClock);

  int code = 0;
  {
    App app(fs, clock, launcher, kPlatformHost);
    AppPaths paths;
    paths.dataDir = o.dataDir;
    paths.assetsDir = o.assetsDir;
    Status s = app.init(paths);
    if (!s.ok()) {
      std::fprintf(stderr, "falha ao iniciar: %s\n", s.toString().c_str());
      Log::removeSink(&stderrLog);
      return 1;
    }
    for (size_t i = 0; i < o.addPaths.size(); ++i) app.addScanPath(o.addPaths[i]);
    if (!o.addPaths.empty()) app.startScan();

    if (o.headless) {
      code = runHeadless(app, renderer, manualClock, fs, o);
    } else {
#ifdef NEBULA_HAVE_SDL2
      code = runSdlFrontend(app, renderer, fs, o.dataDir);
#else
      std::fprintf(stderr, "compilado sem SDL2: use --headless\n");
      code = 2;
#endif
    }
    app.shutdown();
  }
  Log::removeSink(&stderrLog);
  return code;
}
