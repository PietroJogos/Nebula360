#include <string>

#include "TestFramework.h"
#include "core/Json.h"
#include "platform/MemoryFileSystem.h"
#include "platform/host/PosixFileSystem.h"
#include "theme/Theme.h"

using namespace nebula;

NB_TEST(theme_json_partial_override_and_validation) {
  JsonValue json;
  REQUIRE(parseJson("{\"schema\":1,\"id\":\"meu-tema\",\"name\":\"Meu\","
                    "\"colors\":{\"accent\":\"#FF0000\",\"text\":\"vermelho\"},"
                    "\"metrics\":{\"columns\":50,\"tileWidth\":\"x\",\"fontBody\":22}}",
                    json)
              .ok());
  Theme base = builtinDarkTheme();
  Theme t;
  std::vector<std::string> warnings;
  REQUIRE(themeFromJson(json, base, t, warnings).ok());
  CHECK_EQ(t.id, std::string("meu-tema"));
  CHECK_EQ(t.colors.accent, 0xFFFF0000u);
  CHECK_EQ(t.colors.text, base.colors.text);  // inválida: mantém a base
  CHECK_EQ(t.metrics.columns, 10);            // limitado ao máximo
  CHECK_EQ(t.metrics.tileWidth, base.metrics.tileWidth);
  CHECK_EQ(t.metrics.fontBody, 22);
  CHECK_EQ(warnings.size(), 3u);

  JsonValue badId;
  REQUIRE(parseJson("{\"schema\":1,\"id\":\"Tema Inválido\"}", badId).ok());
  CHECK_EQ(themeFromJson(badId, base, t, warnings).code(), kInvalidArgument);
  JsonValue future;
  REQUIRE(parseJson("{\"schema\":7,\"id\":\"x\"}", future).ok());
  CHECK_EQ(themeFromJson(future, base, t, warnings).code(), kUnsupported);
  JsonValue escape;
  REQUIRE(parseJson("{\"schema\":1,\"id\":\"x\",\"backgroundImage\":\"../../segredo.png\"}", escape).ok());
  REQUIRE(themeFromJson(escape, base, t, warnings).ok());
  CHECK(t.backgroundImage.empty());
}

NB_TEST(theme_roundtrip) {
  Theme t = builtinDarkTheme();
  t.id = "copia";
  t.colors.surface = 0x80123456u;
  Theme back;
  std::vector<std::string> warnings;
  REQUIRE(themeFromJson(themeToJson(t), builtinDarkTheme(), back, warnings).ok());
  CHECK_EQ(warnings.size(), 0u);
  CHECK_EQ(themeToJson(back).serialize(false), themeToJson(t).serialize(false));
}

NB_TEST(theme_manager_loads_bundled_themes) {
  PosixFileSystem fs;
  ThemeManager themes(fs);
  std::vector<std::string> warnings;
  size_t n = themes.loadDirectory(std::string(NEBULA_SOURCE_DIR) + "/assets/themes", warnings);
  CHECK_EQ(n, 3u);
  CHECK_EQ(warnings.size(), 0u);
  CHECK_EQ(themes.themes().size(), 3u);  // o arquivo substitui o embutido de mesmo id
  CHECK_EQ(themes.current().id, std::string("nebula-escuro"));
  CHECK(themes.select("alto-contraste"));
  CHECK_EQ(themes.current().metrics.columns, 5);
  CHECK(!themes.select("nao-existe"));
  CHECK_EQ(themes.current().id, std::string("alto-contraste"));
}

NB_TEST(theme_manager_survives_broken_theme_files) {
  MemoryFileSystem fs('/');
  fs.addFile("/temas/quebrado/theme.json", "{ nada");
  fs.addFile("/temas/sem-id/theme.json", "{\"schema\":1}");
  fs.addFile("/temas/bom/theme.json", "{\"schema\":1,\"id\":\"bom\",\"name\":\"Bom\"}");
  fs.addDirectory("/temas/vazio");
  ThemeManager themes(fs);
  std::vector<std::string> warnings;
  CHECK_EQ(themes.loadDirectory("/temas", warnings), 1u);
  CHECK_EQ(warnings.size(), 3u);
  CHECK(themes.find("bom") != NULL);
  CHECK(themes.find("nebula-escuro") != NULL);
  std::vector<std::string> w2;
  CHECK_EQ(themes.loadDirectory("/nao-existe", w2), 0u);
  CHECK_EQ(w2.size(), 1u);
}
