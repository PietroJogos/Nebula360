#include <string>

#include "TestData.h"
#include "TestFramework.h"
#include "app/AppConfig.h"
#include "core/StringUtil.h"
#include "formats/StfsHeader.h"
#include "library/Category.h"
#include "library/CoverResolver.h"
#include "library/Library.h"
#include "library/LibraryQuery.h"
#include "library/Scanner.h"
#include "platform/MemoryFileSystem.h"

using namespace nebula;

namespace {

// Árvore no estilo do HD de um Xbox 360 RGH (nomes fictícios).
void buildConsoleTree(MemoryFileSystem& fs) {
  fs.addFile("Hdd1:\\Games\\Projeto \xC3\x93rion\\default.xex", testdata::makeXex(false, 0, 0, "orion.exe"));
  fs.addFile("Hdd1:\\Games\\Corrida_Estelar\\default.xex",
             testdata::makeXex(true, 0x4D530001, 0x11111111, "corrida.exe"));
  // Subpasta dentro de um jogo: não deve ser visitada.
  fs.addFile("Hdd1:\\Games\\Corrida_Estelar\\media\\extra\\default.xex",
             testdata::makeXex(false, 0, 0, "x.exe"));
  fs.addFile("Hdd1:\\Games\\Classico\\default.xbe", "XBEH");
  // Profundo demais para maxDepth = 3.
  fs.addFile("Hdd1:\\Games\\Profundo\\a\\b\\c\\default.xex", testdata::makeXex(false, 0, 0, "p.exe"));
  // GOD com pasta de dados, XBLA e um save (não jogável).
  fs.addFile("Hdd1:\\Content\\0000000000000000\\4D530002\\00007000\\ABCDEF0123456789",
             testdata::makeStfs(kContentGamesOnDemand, 0x4D530002, "Lenda do Vale", ""));
  fs.addFile("Hdd1:\\Content\\0000000000000000\\4D530002\\00007000\\ABCDEF0123456789.data\\Data0000",
             std::string(64, '\0'));
  fs.addFile("Hdd1:\\Content\\0000000000000000\\58410003\\000D0000\\FEDCBA9876",
             testdata::makeStfs(kContentArcadeTitle, 0x58410003, "Blocos Cadentes", ""));
  fs.addFile("Hdd1:\\Content\\0000000000000000\\4D530004\\00000001\\SAVE0001",
             testdata::makeStfs(kContentSavedGame, 0x4D530004, "Save", ""));
  fs.addFile("Hdd1:\\Emus\\SNESX\\default.xex", testdata::makeXex(false, 0, 0, "snes.exe"));
}

std::vector<ScanRoot> consoleRoots() {
  std::vector<ScanRoot> roots;
  ScanRoot a;
  a.path = "Hdd1:\\Games";
  a.maxDepth = 3;
  roots.push_back(a);
  ScanRoot b;
  b.path = "Hdd1:\\Content\\0000000000000000";
  b.maxDepth = 3;
  roots.push_back(b);
  ScanRoot c;
  c.path = "Hdd1:\\Emus";
  c.category = category::kEmulator;
  roots.push_back(c);
  ScanRoot d;
  d.path = "Usb0:\\Games";  // pendrive ausente
  roots.push_back(d);
  return roots;
}

const DetectedGame* findByTitle(const std::vector<DetectedGame>& r, const std::string& title) {
  for (size_t i = 0; i < r.size(); ++i) {
    if (r[i].title == title) return &r[i];
  }
  return NULL;
}

const Game* findGame(const Library& lib, const std::string& title) {
  for (size_t i = 0; i < lib.size(); ++i) {
    if (lib.at(i).title == title) return &lib.at(i);
  }
  return NULL;
}

}  // namespace

NB_TEST(scanner_detects_all_formats_on_console_tree) {
  MemoryFileSystem fs('\\');
  buildConsoleTree(fs);
  Scanner scanner(fs);
  scanner.start(consoleRoots());
  // Incremental: vários passos pequenos.
  int steps = 0;
  while (!scanner.step(1)) ++steps;
  CHECK(steps > 5);
  const std::vector<DetectedGame>& r = scanner.results();
  CHECK_EQ(r.size(), 6u);

  const DetectedGame* orion = findByTitle(r, "Projeto \xC3\x93rion");
  REQUIRE(orion != NULL);
  CHECK_EQ(orion->format, std::string("xex"));
  CHECK_EQ(orion->category, std::string("homebrew"));

  const DetectedGame* corrida = findByTitle(r, "Corrida Estelar");
  REQUIRE(corrida != NULL);
  CHECK_EQ(corrida->category, std::string("xbox360"));
  CHECK_EQ(corrida->titleId, 0x4D530001u);

  const DetectedGame* classico = findByTitle(r, "Classico");
  REQUIRE(classico != NULL);
  CHECK_EQ(classico->category, std::string("xbox_classic"));
  CHECK_EQ(classico->format, std::string("xbe"));

  const DetectedGame* god = findByTitle(r, "Lenda do Vale");
  REQUIRE(god != NULL);
  CHECK_EQ(god->format, std::string("god"));
  CHECK_EQ(god->category, std::string("xbox360"));
  CHECK_EQ(god->titleId, 0x4D530002u);

  const DetectedGame* xbla = findByTitle(r, "Blocos Cadentes");
  REQUIRE(xbla != NULL);
  CHECK_EQ(xbla->format, std::string("xbla"));
  CHECK_EQ(xbla->category, std::string("arcade"));

  const DetectedGame* emu = findByTitle(r, "SNESX");
  REQUIRE(emu != NULL);
  CHECK_EQ(emu->category, std::string("emulator"));

  CHECK(findByTitle(r, "Save") == NULL);
  CHECK(findByTitle(r, "c") == NULL);  // além da profundidade
  CHECK_EQ(scanner.stats().errors, 1u);  // Usb0 ausente
  CHECK_EQ(scanner.accessibleRoots().size(), 3u);
}

NB_TEST(scanner_cancel_and_unreadable_files) {
  MemoryFileSystem fs('\\');
  buildConsoleTree(fs);
  fs.failPath("Hdd1:\\Games\\Corrida_Estelar\\default.xex");
  Scanner scanner(fs);
  scanner.start(consoleRoots());
  scanner.runToCompletion();
  // XEX ilegível ainda entra (como homebrew) e o erro é contado.
  const DetectedGame* corrida = findByTitle(scanner.results(), "Corrida Estelar");
  REQUIRE(corrida != NULL);
  CHECK_EQ(corrida->category, std::string("homebrew"));
  CHECK_EQ(scanner.stats().errors, 2u);

  Scanner s2(fs);
  s2.start(consoleRoots());
  s2.step(1);
  s2.cancel();
  CHECK(s2.cancelled());
  CHECK(!s2.running());
}

NB_TEST(library_merge_marks_missing_only_in_accessible_roots) {
  MemoryFileSystem fs('\\');
  buildConsoleTree(fs);
  fs.addFile("Usb0:\\Games\\Pendrive Jogo\\default.xex", testdata::makeXex(false, 0, 0, "u.exe"));
  Library lib;
  Scanner scanner(fs);
  scanner.start(consoleRoots());
  scanner.runToCompletion();
  MergeStats st = lib.mergeScan(scanner.results(), scanner.accessibleRoots(), 1000, '\\');
  CHECK_EQ(st.added, 7u);
  CHECK_EQ(lib.size(), 7u);
  const Game* usb = findGame(lib, "Pendrive Jogo");
  REQUIRE(usb != NULL);
  std::string usbId = usb->id;
  CHECK(lib.setFavorite(usbId, true));

  // Pendrive desconectado: o jogo NÃO deve ser marcado como ausente.
  fs.failPath("Usb0:");
  scanner.start(consoleRoots());
  scanner.runToCompletion();
  st = lib.mergeScan(scanner.results(), scanner.accessibleRoots(), 2000, '\\');
  CHECK_EQ(st.markedMissing, 0u);
  CHECK(!lib.find(usbId)->missing);
  CHECK(lib.find(usbId)->favorite);

  // Jogo apagado do HD: marcado como ausente, mantendo os dados.
  MemoryFileSystem fs2('\\');
  buildConsoleTree(fs2);
  fs2.removeFile("Hdd1:\\Games\\Corrida_Estelar\\default.xex");
  fs2.removeFile("Hdd1:\\Games\\Corrida_Estelar\\media\\extra\\default.xex");
  Scanner scanner2(fs2);
  scanner2.start(consoleRoots());
  scanner2.runToCompletion();
  st = lib.mergeScan(scanner2.results(), scanner2.accessibleRoots(), 3000, '\\');
  CHECK_EQ(st.markedMissing, 1u);
  const Game* corrida = findGame(lib, "Corrida Estelar");
  REQUIRE(corrida != NULL);
  CHECK(corrida->missing);
  CHECK_EQ(corrida->addedAt, 1000);

  // Volta a aparecer: restaurado.
  scanner.start(consoleRoots());
  fs.clearFailures();
  scanner.start(consoleRoots());
  scanner.runToCompletion();
  st = lib.mergeScan(scanner.results(), scanner.accessibleRoots(), 4000, '\\');
  CHECK_EQ(st.restored, 1u);
  CHECK_EQ(lib.removeMissing(), 0u);
}

NB_TEST(library_persistence_roundtrip_and_corruption) {
  MemoryFileSystem fs('/');
  fs.addDirectory("/dados");
  Library lib;
  std::vector<DetectedGame> found;
  DetectedGame d;
  d.launchPath = "/jogos/A/default.xex";
  d.folder = "/jogos/A";
  d.title = "Árvore \"Mágica\"";
  d.format = "xex";
  d.category = "homebrew";
  d.titleId = 0xFFFF0001;
  found.push_back(d);
  lib.mergeScan(found, std::vector<std::string>(), 123, '/');
  std::string id = lib.at(0).id;
  lib.setFavorite(id, true);
  lib.recordLaunch(id, 456);
  lib.setCategoryOverride(id, "emulator");
  REQUIRE(saveLibrary(fs, "/dados/library.json", lib).ok());

  Library loaded;
  REQUIRE(loadLibrary(fs, "/dados/library.json", loaded).ok());
  REQUIRE(loaded.size() == 1);
  const Game& g = loaded.at(0);
  CHECK_EQ(g.id, id);
  CHECK_EQ(g.title, std::string("Árvore \"Mágica\""));
  CHECK(g.favorite);
  CHECK_EQ(g.playCount, 1u);
  CHECK_EQ(g.lastPlayedAt, 456);
  CHECK_EQ(g.effectiveCategory(), std::string("emulator"));
  CHECK_EQ(g.titleId, 0xFFFF0001u);

  fs.addFile("/dados/library.json", "{ isto não é json");
  Library broken;
  Status s = loadLibrary(fs, "/dados/library.json", broken);
  CHECK_EQ(s.code(), kParseError);
  CHECK_EQ(broken.size(), 0u);
  CHECK(fs.exists("/dados/library.json.corrompido"));
  CHECK(!fs.exists("/dados/library.json"));

  Library none;
  CHECK(loadLibrary(fs, "/dados/nao-existe.json", none).ok());
}

NB_TEST(library_rejects_invalid_entries_on_load) {
  JsonValue json;
  REQUIRE(parseJson("{\"schema\":1,\"games\":[{\"id\":\"a\",\"path\":\"/x\",\"title\":\"X\"},"
                    "{\"id\":\"\",\"path\":\"/y\"},{\"id\":\"a\",\"path\":\"/dup\"},"
                    "{\"id\":\"b\",\"path\":\"/z\",\"category\":\"invalida\",\"playCount\":-5}]}",
                    json)
              .ok());
  Library lib;
  size_t skipped = 0;
  REQUIRE(lib.fromJson(json, skipped).ok());
  CHECK_EQ(skipped, 2u);
  CHECK_EQ(lib.size(), 2u);
  CHECK_EQ(lib.find("b")->category, std::string("homebrew"));
  CHECK_EQ(lib.find("b")->playCount, 0u);
  JsonValue future;
  REQUIRE(parseJson("{\"schema\":99,\"games\":[]}", future).ok());
  CHECK_EQ(lib.fromJson(future, skipped).code(), kUnsupported);
}

NB_TEST(query_filters_search_and_sort) {
  MemoryFileSystem fs('\\');
  buildConsoleTree(fs);
  Scanner scanner(fs);
  scanner.start(consoleRoots());
  scanner.runToCompletion();
  Library lib;
  lib.mergeScan(scanner.results(), scanner.accessibleRoots(), 100, '\\');
  const Game* orion = findGame(lib, "Projeto \xC3\x93rion");
  const Game* lenda = findGame(lib, "Lenda do Vale");
  REQUIRE(orion != NULL && lenda != NULL);
  std::string orionId = orion->id;
  std::string lendaId = lenda->id;
  lib.setFavorite(lendaId, true);
  lib.recordLaunch(orionId, 500);
  lib.recordLaunch(orionId, 600);
  lib.recordLaunch(lendaId, 700);

  LibraryQuery q;
  std::vector<size_t> all = runQuery(lib, q);
  CHECK_EQ(all.size(), 6u);
  CHECK_EQ(lib.at(all[0]).title, std::string("Blocos Cadentes"));  // ordem alfabética

  q.search = "orion";  // sem acento encontra "Órion"
  std::vector<size_t> r = runQuery(lib, q);
  REQUIRE(r.size() == 1);
  CHECK_EQ(lib.at(r[0]).id, orionId);

  q.search = "4d530002";  // prefixo do Title ID
  r = runQuery(lib, q);
  REQUIRE(r.size() == 1);
  CHECK_EQ(lib.at(r[0]).id, lendaId);

  q.search = "LENDA vale";  // todos os termos, sem diferenciar maiúsculas
  CHECK_EQ(runQuery(lib, q).size(), 1u);
  q.search = "lenda inexistente";
  CHECK_EQ(runQuery(lib, q).size(), 0u);

  q.search.clear();
  q.favoritesOnly = true;
  CHECK_EQ(runQuery(lib, q).size(), 1u);
  q.favoritesOnly = false;
  q.category = "xbox360";
  CHECK_EQ(runQuery(lib, q).size(), 2u);

  q.category.clear();
  q.sort = kSortMostPlayed;
  r = runQuery(lib, q);
  CHECK_EQ(lib.at(r[0]).id, orionId);
  q.sort = kSortLastPlayed;
  r = runQuery(lib, q);
  CHECK_EQ(lib.at(r[0]).id, lendaId);

  SortMode m;
  CHECK(parseSortMode("playcount", m));
  CHECK_EQ(static_cast<int>(m), static_cast<int>(kSortMostPlayed));
  CHECK(!parseSortMode("x", m));
}

NB_TEST(cover_resolver_priority_order) {
  MemoryFileSystem fs('\\');
  fs.addDirectory("Hdd1:\\Nebula");
  Game g;
  g.id = "abc123";
  g.titleId = 0x4D530001;
  g.format = "xex";
  g.folder = "Hdd1:\\Games\\Corrida";
  CoverResolver covers(fs, "Hdd1:\\Nebula");
  CHECK_EQ(covers.resolve(g), std::string(""));

  std::string iconPath;
  REQUIRE(covers.storeIcon(g.id, "PNGDATA", iconPath).ok());
  CHECK_EQ(covers.resolve(g), std::string("Hdd1:\\Nebula\\cache\\icons\\abc123.png"));

  fs.addFile("Hdd1:\\Games\\Corrida\\boxart.jpg", "j");
  CHECK_EQ(covers.resolve(g), std::string("Hdd1:\\Games\\Corrida\\boxart.jpg"));
  fs.addFile("Hdd1:\\Games\\Corrida\\cover.png", "p");
  CHECK_EQ(covers.resolve(g), std::string("Hdd1:\\Games\\Corrida\\cover.png"));
  fs.addFile("Hdd1:\\Nebula\\covers\\abc123.jpg", "j");
  CHECK_EQ(covers.resolve(g), std::string("Hdd1:\\Nebula\\covers\\abc123.jpg"));
  fs.addFile("Hdd1:\\Nebula\\covers\\4D530001.png", "p");
  CHECK_EQ(covers.resolve(g), std::string("Hdd1:\\Nebula\\covers\\4D530001.png"));

  // Pastas de GOD são compartilhadas: não usa cover.* da pasta.
  Game god = g;
  god.id = "zzz";
  god.titleId = 0;
  god.format = "god";
  CHECK_EQ(covers.resolve(god), std::string(""));
}

NB_TEST(config_defaults_validation_and_corruption) {
  MemoryFileSystem fs('\\');
  fs.addDirectory("Hdd1:\\Nebula");
  std::vector<std::string> warnings;
  AppConfig c;
  REQUIRE(loadConfig(fs, "Hdd1:\\Nebula\\nebula.json", kPlatformXbox360, c, warnings).ok());
  CHECK_EQ(c.scanRoots.size(), 4u);
  CHECK_EQ(c.scanRoots[0].path, std::string("Hdd1:\\Games"));
  CHECK_EQ(c.themeId, std::string("nebula-escuro"));

  fs.addFile("Hdd1:\\Nebula\\nebula.json",
             "{\"schema\":1,\"scanPaths\":[{\"path\":\"Hdd1:\\\\Jogos\",\"category\":\"xpto\","
             "\"maxDepth\":99},{\"path\":\"  \"}],\"sort\":\"aleatorio\",\"logLevel\":\"muito\","
             "\"theme\":\"alto-contraste\",\"showMissing\":true}");
  warnings.clear();
  REQUIRE(loadConfig(fs, "Hdd1:\\Nebula\\nebula.json", kPlatformXbox360, c, warnings).ok());
  CHECK_EQ(c.scanRoots.size(), 1u);
  CHECK_EQ(c.scanRoots[0].category, std::string("auto"));
  CHECK_EQ(c.scanRoots[0].maxDepth, 8);
  CHECK_EQ(c.sortMode, std::string("title"));
  CHECK_EQ(c.themeId, std::string("alto-contraste"));
  CHECK(c.showMissing);
  CHECK_EQ(warnings.size(), 5u);

  REQUIRE(saveConfig(fs, "Hdd1:\\Nebula\\nebula.json", c).ok());
  AppConfig again;
  warnings.clear();
  REQUIRE(loadConfig(fs, "Hdd1:\\Nebula\\nebula.json", kPlatformXbox360, again, warnings).ok());
  CHECK_EQ(warnings.size(), 0u);
  CHECK_EQ(again.toJson().serialize(false), c.toJson().serialize(false));

  fs.addFile("Hdd1:\\Nebula\\nebula.json", "[[[");
  CHECK_EQ(loadConfig(fs, "Hdd1:\\Nebula\\nebula.json", kPlatformHost, c, warnings).code(), kParseError);
  CHECK(fs.exists("Hdd1:\\Nebula\\nebula.json.corrompido"));
  CHECK_EQ(c.scanRoots.size(), 0u);  // padrão do PC
}
