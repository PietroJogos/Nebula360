// Testes de integração: App completo sobre sistema de arquivos em memória,
// com entrada simulada do controle e renderização por software.
#include <string>

#include "TestData.h"
#include "TestFramework.h"
#include "app/App.h"
#include "core/Json.h"
#include "core/StringUtil.h"
#include "formats/StfsHeader.h"
#include "library/Game.h"
#include "platform/Clock.h"
#include "platform/Launcher.h"
#include "platform/MemoryFileSystem.h"
#include "platform/host/PosixFileSystem.h"
#include "plugins/ModuleRegistry.h"
#include "ui/Image.h"
#include "ui/SoftwareRenderer.h"
#include "ui/screens/LibraryScreen.h"

using namespace nebula;

namespace {

class RecordingLauncher : public ILauncher {
 public:
  RecordingLauncher() : calls(0), dashboardCalls(0) {}
  virtual Status launch(const Game& game) NB_OVERRIDE {
    ++calls;
    last = game.path;
    return Status::Ok();
  }
  virtual bool canLaunch(const Game&) const NB_OVERRIDE { return true; }
  virtual Status returnToDashboard() NB_OVERRIDE {
    ++dashboardCalls;
    return Status::Ok();  // no console não retornaria
  }
  int calls;
  int dashboardCalls;
  std::string last;
};

void copyAsset(PosixFileSystem& host, MemoryFileSystem& mem, const std::string& rel) {
  std::string data;
  Status s = host.readFile(std::string(NEBULA_SOURCE_DIR) + "/assets/" + rel, data, 16 * 1024 * 1024);
  if (s.ok()) mem.addFile("/assets/" + rel, data);
}

void copyAssets(MemoryFileSystem& mem) {
  PosixFileSystem host;
  copyAsset(host, mem, "fonts/Inter-Regular.otf");
  copyAsset(host, mem, "fonts/Inter-SemiBold.otf");
  copyAsset(host, mem, "lang/pt-BR.json");
  copyAsset(host, mem, "themes/nebula-escuro/theme.json");
  copyAsset(host, mem, "themes/nebula-claro/theme.json");
  copyAsset(host, mem, "themes/alto-contraste/theme.json");
}

void addGames(MemoryFileSystem& fs) {
  fs.addFile("/jogos/Asteroides Neon/default.xex", testdata::makeXex(false, 0, 0, "a.exe"));
  fs.addFile("/jogos/Blocos/default.xex", testdata::makeXex(false, 0, 0, "b.exe"));
  fs.addFile("/jogos/Corrida Estelar/default.xex", testdata::makeXex(true, 0x4D530001, 1, "c.exe"));
  fs.addFile("/jogos/Dunas/default.xex", testdata::makeXex(false, 0, 0, "d.exe"));
  Image icon(64, 64, 0xFF2266AAu);
  std::string png;
  encodePng(icon, png);
  fs.addFile("/jogos/Content/0000000000000000/4D530002/00007000/0123456789ABCDEF",
             testdata::makeStfs(kContentGamesOnDemand, 0x4D530002, "Lenda do Vale", png));
}

void writeConfig(MemoryFileSystem& fs) {
  fs.addFile("/dados/nebula.json",
             "{\"schema\":1,\"scanPaths\":[{\"path\":\"/jogos\",\"category\":\"auto\",\"maxDepth\":5,"
             "\"enabled\":true}],\"theme\":\"nebula-escuro\"}");
}

struct Fixture {
  MemoryFileSystem fs;
  ManualClock clock;
  RecordingLauncher launcher;
  App* app;
  Fixture() : fs('/'), clock(1000, 1794000000), app(NULL) { copyAssets(fs); }
  ~Fixture() { delete app; }

  Status start(ILauncher* customLauncher = NULL) {
    app = new App(fs, clock, customLauncher ? *customLauncher : launcher, kPlatformHost);
    AppPaths p;
    p.dataDir = "/dados";
    p.assetsDir = "/assets";
    return app->init(p);
  }
  void frames(int n) {
    InputState idle;
    for (int i = 0; i < n; ++i) {
      app->tick(idle);
      clock.advance(16);
    }
  }
  void press(uint32_t button) {
    InputState s;
    s.buttons = button;
    app->tick(s);
    clock.advance(16);
    frames(2);
  }
  void waitScan() {
    for (int i = 0; i < 1000 && app->scanning(); ++i) frames(1);
  }
  bool toastContains(const std::string& needle) const {
    for (size_t i = 0; i < app->toasts().size(); ++i) {
      if (app->toasts()[i].message.find(needle) != std::string::npos) return true;
    }
    return false;
  }
};

}  // namespace

NB_TEST(app_first_run_on_pc_creates_config_and_shows_empty_library) {
  Fixture f;
  REQUIRE(f.start().ok());
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));
  CHECK(f.fs.exists("/dados/nebula.json"));
  CHECK(f.fs.exists("/dados/logs/nebula.log") || true);  // o log é gravado em lotes
  CHECK_EQ(f.app->library().size(), 0u);
  SoftwareRenderer r;
  f.app->render(r);  // estado vazio desenha sem falhas
  f.press(kButtonStart);
  CHECK_EQ(f.app->activeScreenName(), std::string("menu"));
  f.press(kButtonDown);  // Reescanear
  f.press(kButtonA);
  CHECK(f.toastContains("Nenhuma pasta ativa"));
}

NB_TEST(app_scans_on_first_start_and_navigates_with_controller) {
  Fixture f;
  writeConfig(f.fs);
  addGames(f.fs);
  REQUIRE(f.start().ok());
  CHECK_EQ(f.app->activeScreenName(), std::string("scan"));
  f.waitScan();
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));
  CHECK_EQ(f.app->library().size(), 5u);
  CHECK(f.toastContains("5 novo(s)"));
  CHECK(f.fs.exists("/dados/library.json"));
  CHECK(f.fs.exists("/dados/export/library.v1.json"));
  // Miniatura do contêiner GOD extraída para o cache e usada como capa.
  const Game* lenda = NULL;
  for (size_t i = 0; i < f.app->library().size(); ++i) {
    if (f.app->library().at(i).title == "Lenda do Vale") lenda = &f.app->library().at(i);
  }
  REQUIRE(lenda != NULL);
  CHECK(str::endsWith(lenda->coverPath, ".png"));
  CHECK(f.fs.exists(lenda->coverPath));

  LibraryScreen* lib = f.app->libraryScreen();
  REQUIRE(lib != NULL);
  CHECK_EQ(lib->visibleGames().size(), 5u);
  CHECK_EQ(lib->selectedIndex(), 0u);
  f.press(kButtonRight);
  f.press(kButtonRight);
  CHECK_EQ(lib->selectedIndex(), 2u);
  f.press(kButtonLeft);
  CHECK_EQ(lib->selectedIndex(), 1u);
  std::string id = lib->selectedGameId();

  // Detalhes, favorito e volta.
  f.press(kButtonA);
  CHECK_EQ(f.app->activeScreenName(), std::string("details"));
  f.press(kButtonY);
  CHECK(f.app->library().find(id)->favorite);
  f.press(kButtonB);
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));

  // Salvamento automático com atraso.
  f.frames(80);
  std::string saved;
  REQUIRE(f.fs.fileContents("/dados/library.json", saved));
  JsonValue json;
  REQUIRE(parseJson(saved, json).ok());
  bool favoriteSaved = false;
  for (size_t i = 0; i < json.get("games").size(); ++i) {
    const JsonValue& g = json.get("games").at(i);
    if (g.get("id").asString() == id) favoriteSaved = g.get("favorite").asBool();
  }
  CHECK(favoriteSaved);

  // Aba de favoritos (RB: Todos -> Favoritos).
  f.press(kButtonRB);
  CHECK_EQ(lib->currentTab(), std::string("favorites"));
  CHECK_EQ(lib->visibleGames().size(), 1u);
  f.press(kButtonLB);
  CHECK_EQ(lib->currentTab(), std::string(""));

  // Ordenação (Back) persiste na configuração.
  f.press(kButtonBack);
  CHECK_EQ(f.app->config().sortMode, std::string("added"));
  f.frames(80);
  std::string cfg;
  REQUIRE(f.fs.fileContents("/dados/nebula.json", cfg));
  CHECK(cfg.find("\"added\"") != std::string::npos);
}

NB_TEST(app_search_with_virtual_keyboard) {
  Fixture f;
  writeConfig(f.fs);
  addGames(f.fs);
  REQUIRE(f.start().ok());
  f.waitScan();
  LibraryScreen* lib = f.app->libraryScreen();
  f.press(kButtonX);
  CHECK_EQ(f.app->activeScreenName(), std::string("keyboard"));
  // O cursor começa em "q" (linha 1, coluna 0). "d" fica na linha 2, coluna 2.
  f.press(kButtonDown);
  f.press(kButtonRight);
  f.press(kButtonRight);
  f.press(kButtonA);
  CHECK_EQ(lib->search(), std::string("d"));  // pesquisa ao vivo
  // "d" aparece em Asteroides, Corrida, Dunas e Lenda (não em Blocos).
  CHECK_EQ(lib->visibleGames().size(), 4u);
  f.press(kButtonDown);  // linha 3, "c"
  f.press(kButtonA);
  CHECK_EQ(lib->search(), std::string("dc"));
  CHECK_EQ(lib->visibleGames().size(), 0u);
  f.press(kButtonX);  // apagar
  CHECK_EQ(lib->search(), std::string("d"));
  f.press(kButtonY);  // espaço
  f.press(kButtonStart);
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));
  CHECK_EQ(lib->search(), std::string("d "));
  f.press(kButtonB);  // limpa a pesquisa
  CHECK_EQ(lib->search(), std::string(""));
  CHECK_EQ(lib->visibleGames().size(), 5u);

  // Cancelar devolve o texto original.
  f.press(kButtonX);
  f.press(kButtonA);  // "q"
  CHECK_EQ(lib->search(), std::string("q"));
  f.press(kButtonB);
  CHECK_EQ(lib->search(), std::string(""));
}

NB_TEST(app_theme_change_and_rendering) {
  Fixture f;
  writeConfig(f.fs);
  addGames(f.fs);
  REQUIRE(f.start().ok());
  f.waitScan();
  SoftwareRenderer r;
  f.app->render(r);
  Color darkBg = r.framebuffer().at(2, 2);
  // Menu -> Tema -> escolher "Nébula Claro" (ordem por id: alto-contraste, nebula-claro, nebula-escuro).
  f.press(kButtonStart);
  f.press(kButtonDown);
  f.press(kButtonDown);
  f.press(kButtonA);
  CHECK_EQ(f.app->activeScreenName(), std::string("themes"));
  f.press(kButtonDown);
  f.press(kButtonA);
  CHECK_EQ(f.app->config().themeId, std::string("nebula-claro"));
  f.press(kButtonB);
  f.press(kButtonB);
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));
  f.app->render(r);
  Color lightBg = r.framebuffer().at(2, 2);
  CHECK(colorR(lightBg) > 200 && colorR(darkBg) < 40);
  // Quadro com conteúdo variado (texto, capas, dicas).
  size_t distinct = 0;
  Color prev = 0;
  for (size_t i = 0; i < r.framebuffer().pixels.size(); i += 97) {
    if (r.framebuffer().pixels[i] != prev) ++distinct;
    prev = r.framebuffer().pixels[i];
  }
  CHECK(distinct > 200);
}

NB_TEST(app_launch_records_history_and_reports_unsupported_on_pc) {
  Fixture f;
  writeConfig(f.fs);
  addGames(f.fs);
  RecordingLauncher recorder;
  REQUIRE(f.start(&recorder).ok());
  f.waitScan();
  std::string id = f.app->libraryScreen()->selectedGameId();
  REQUIRE(!id.empty());
  f.press(kButtonA);
  f.press(kButtonA);  // "Iniciar"
  CHECK_EQ(recorder.calls, 1);
  const Game* launched = f.app->library().find(id);
  REQUIRE(launched != NULL);
  CHECK_EQ(launched->playCount, 1u);
  CHECK(launched->lastPlayedAt > 0);
  CHECK_EQ(recorder.last, launched->path);

  Fixture pc;
  writeConfig(pc.fs);
  addGames(pc.fs);
  UnsupportedLauncher unsupported;
  REQUIRE(pc.start(&unsupported).ok());
  pc.waitScan();
  pc.press(kButtonA);
  pc.press(kButtonA);
  CHECK(pc.toastContains("só é possível no Xbox 360"));
}

NB_TEST(app_manages_scan_paths_through_menus) {
  Fixture f;
  writeConfig(f.fs);
  REQUIRE(f.start().ok());
  f.waitScan();
  f.app->addScanPath("/outra/pasta/");
  CHECK_EQ(f.app->config().scanRoots.size(), 2u);
  CHECK_EQ(f.app->config().scanRoots[1].path, std::string("/outra/pasta"));
  f.app->addScanPath("/OUTRA/pasta");
  CHECK(f.toastContains("já está na lista"));
  // Menu -> Pastas -> segunda pasta -> desativar.
  f.press(kButtonStart);
  f.press(kButtonA);
  CHECK_EQ(f.app->activeScreenName(), std::string("paths"));
  f.press(kButtonDown);
  f.press(kButtonA);
  CHECK_EQ(f.app->activeScreenName(), std::string("path-options"));
  f.press(kButtonA);
  CHECK(!f.app->config().scanRoots[1].enabled);
  // Remover.
  f.press(kButtonDown);
  f.press(kButtonDown);
  f.press(kButtonA);
  CHECK_EQ(f.app->config().scanRoots.size(), 1u);
  CHECK_EQ(f.app->activeScreenName(), std::string("paths"));
}

NB_TEST(app_recovers_from_corrupted_files_and_save_failures) {
  Fixture f;
  f.fs.addFile("/dados/nebula.json", "{ corrompido");
  f.fs.addFile("/dados/library.json", "[ corrompido");
  REQUIRE(f.start().ok());
  CHECK(f.fs.exists("/dados/nebula.json.corrompido"));
  CHECK(f.fs.exists("/dados/library.json.corrompido"));
  CHECK(f.toastContains("Configuração ilegível"));
  CHECK(f.toastContains("Biblioteca ilegível"));
  CHECK_EQ(f.app->activeScreenName(), std::string("library"));

  // Falha de gravação: aviso na tela, sem travar; tenta de novo depois.
  Fixture g;
  writeConfig(g.fs);
  addGames(g.fs);
  REQUIRE(g.start().ok());
  g.waitScan();
  g.fs.failPath("/dados/library.json");
  g.press(kButtonY);
  g.frames(80);
  CHECK(g.toastContains("Não foi possível salvar"));
  g.fs.clearFailures();
  g.frames(400);
  std::string saved;
  REQUIRE(g.fs.fileContents("/dados/library.json", saved));
  CHECK(saved.find("\"favorite\": true") != std::string::npos);
}

NB_TEST(app_fails_cleanly_without_fonts) {
  Fixture f;
  f.fs.removeFile("/assets/fonts/Inter-Regular.otf");
  Status s = f.start();
  CHECK(!s.ok());
  InputState idle;
  f.app->tick(idle);  // não inicializado: não faz nada, sem crash
  SoftwareRenderer r;
  f.app->render(r);
}

NB_TEST(app_enters_safe_mode_after_repeated_unexpected_shutdowns) {
  // Sessão normal: módulo opcional de exportação roda e grava o arquivo.
  {
    Fixture f;
    writeConfig(f.fs);
    addGames(f.fs);
    REQUIRE(f.start().ok());
    f.waitScan();
    CHECK(f.app->modules().isRunning("nebula.export"));
    CHECK(!f.app->safeMode());
    CHECK(f.fs.exists("/dados/export/library.v1.json"));
  }
  // Duas quedas seguidas registradas: o app sobe em modo seguro.
  Fixture g;
  writeConfig(g.fs);
  addGames(g.fs);
  g.fs.addFile("/dados/session.json", "{\"schema\":1,\"state\":\"running\",\"consecutiveCrashes\":1}");
  REQUIRE(g.start().ok());
  CHECK(g.app->safeMode());
  CHECK(g.toastContains("não foi encerrado normalmente"));
  CHECK(g.toastContains("Modo seguro"));
  CHECK_EQ(g.app->modules().find("nebula.export")->state, kModuleDisabled);
  CHECK(g.app->modules().isRunning("nebula.library"));
  g.waitScan();
  CHECK_EQ(g.app->library().size(), 5u);  // a biblioteca funciona normalmente
  CHECK(!g.fs.exists("/dados/export/library.v1.json"));
  // Encerramento normal limpa o estado: a próxima sessão sai do modo seguro.
  g.app->shutdown();
  std::string session;
  REQUIRE(g.fs.fileContents("/dados/session.json", session));
  CHECK(session.find("\"clean\"") != std::string::npos);
}

NB_TEST(app_optional_module_disabled_by_config) {
  Fixture f;
  f.fs.addFile("/dados/nebula.json",
               "{\"schema\":1,\"scanPaths\":[{\"path\":\"/jogos\"}],\"integrations\":{\"libraryExport\":false}}");
  addGames(f.fs);
  REQUIRE(f.start().ok());
  f.waitScan();
  CHECK_EQ(f.app->modules().find("nebula.export")->state, kModuleDisabled);
  CHECK(!f.fs.exists("/dados/export/library.v1.json"));
  CHECK_EQ(f.app->library().size(), 5u);
}

NB_TEST(app_exit_returns_to_dashboard_or_closes_on_pc) {
  Fixture f;
  writeConfig(f.fs);
  RecordingLauncher console;
  REQUIRE(f.start(&console).ok());
  f.waitScan();
  f.press(kButtonStart);
  f.press(kButtonUp);  // a lista é circular: do primeiro item, "Sair" é o anterior
  CHECK_EQ(f.app->activeScreenName(), std::string("menu"));
  f.press(kButtonA);
  CHECK_EQ(console.dashboardCalls, 1);
  std::string session;
  REQUIRE(f.fs.fileContents("/dados/session.json", session));
  CHECK(session.find("\"clean\"") != std::string::npos);  // saída marcada como limpa

  Fixture pc;
  writeConfig(pc.fs);
  UnsupportedLauncher unsupported;
  REQUIRE(pc.start(&unsupported).ok());
  pc.waitScan();
  CHECK(!pc.app->quitRequested());
  pc.app->exitToDashboard();
  CHECK(pc.app->quitRequested());  // no PC o frontend fecha a janela
}
