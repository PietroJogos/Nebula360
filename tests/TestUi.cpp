#include <string>

#include "TestFramework.h"
#include "platform/MemoryFileSystem.h"
#include "platform/host/PosixFileSystem.h"
#include "ui/Font.h"
#include "ui/Image.h"
#include "ui/Input.h"
#include "ui/SoftwareRenderer.h"
#include "ui/Strings.h"
#include "ui/TextureCache.h"

using namespace nebula;

namespace {
std::string asset(const std::string& rel) { return std::string(NEBULA_SOURCE_DIR) + "/assets/" + rel; }

size_t countActions(const std::vector<Action>& v, Action a) {
  size_t n = 0;
  for (size_t i = 0; i < v.size(); ++i) n += v[i] == a ? 1 : 0;
  return n;
}
}  // namespace

NB_TEST(input_edges_and_repeat) {
  InputMapper m;
  std::vector<Action> out;
  InputState s;
  s.buttons = kButtonA;
  m.update(s, 0, out);
  CHECK_EQ(countActions(out, kActionAccept), 1u);
  m.update(s, 1000, out);  // A segurado não repete
  CHECK_EQ(out.size(), 0u);

  s.buttons = kButtonDown;
  size_t downs = 0;
  m.update(s, 2000, out);
  downs += countActions(out, kActionDown);
  for (uint64_t t = 2010; t <= 2000 + InputMapper::kRepeatDelayMs - 10; t += 10) {
    m.update(s, t, out);
    downs += countActions(out, kActionDown);
  }
  CHECK_EQ(downs, 1u);  // nada antes do atraso de repetição
  for (uint64_t t = 2000 + InputMapper::kRepeatDelayMs; t <= 2000 + 1000; t += 10) {
    m.update(s, t, out);
    downs += countActions(out, kActionDown);
  }
  CHECK(downs >= 6 && downs <= 9);

  s.buttons = 0;
  s.leftX = 0.9f;  // analógico vira direcional
  m.update(s, 5000, out);
  CHECK_EQ(countActions(out, kActionRight), 1u);
  s.leftX = 0.3f;  // zona morta
  m.update(s, 5100, out);
  s.leftX = 0.0f;
  m.update(s, 5200, out);
  CHECK_EQ(out.size(), 0u);
}

NB_TEST(software_renderer_primitives) {
  SoftwareRenderer r(64, 64);
  r.clear(0xFF000000u);
  r.fillRect(Rect(0, 0, 10, 10), 0xFFFF0000u);
  CHECK_EQ(r.framebuffer().at(5, 5), 0xFFFF0000u);
  r.fillRect(Rect(0, 0, 10, 10), 0x800000FFu);  // 50% azul sobre vermelho
  Color c = r.framebuffer().at(5, 5);
  CHECK(colorR(c) > 120 && colorR(c) < 135);
  CHECK(colorB(c) > 120 && colorB(c) < 135);

  r.pushClip(Rect(20, 20, 4, 4));
  r.fillRect(Rect(0, 0, 64, 64), 0xFF00FF00u);
  r.popClip();
  CHECK_EQ(r.framebuffer().at(21, 21), 0xFF00FF00u);
  CHECK_EQ(r.framebuffer().at(30, 30), 0xFF000000u);

  // Cobertura de canto arredondado.
  Rect box(0, 0, 40, 40);
  CHECK_EQ(roundedRectCoverage(box, 10, 0, 0), 0u);
  CHECK_EQ(roundedRectCoverage(box, 10, 20, 20), 255u);
  CHECK_EQ(roundedRectCoverage(box, 10, 20, 0), 255u);
  CHECK_EQ(roundedRectCoverage(box, 10, 40, 40), 0u);

  r.clear(0xFF000000u);
  r.strokeRoundedRect(Rect(10, 10, 40, 40), 8, 2, 0xFFFFFFFFu);
  CHECK_EQ(r.framebuffer().at(30, 30), 0xFF000000u);  // interior intacto
  CHECK_EQ(r.framebuffer().at(30, 10), 0xFFFFFFFFu);  // borda

  Image img(2, 2, 0xFF00FF00u);
  img.pixels[3] = 0xFFFF00FFu;
  r.drawImage(img, Rect(0, 0, 2, 2), Rect(0, 0, 20, 20), 255, 0);
  CHECK_EQ(r.framebuffer().at(2, 2), 0xFF00FF00u);
  CHECK_EQ(r.framebuffer().at(15, 15), 0xFFFF00FFu);

  const uint8_t mask[4] = {0, 255, 255, 0};
  r.clear(0xFF000000u);
  r.drawAlphaMask(mask, 2, 2, 2, 60, 60, 0xFFFFFFFFu);
  CHECK_EQ(r.framebuffer().at(61, 60), 0xFFFFFFFFu);
  CHECK_EQ(r.framebuffer().at(60, 60), 0xFF000000u);
  r.drawAlphaMask(mask, 2, 2, 2, 63, 63, 0xFFFFFFFFu);  // recortado na borda, sem crash
}

NB_TEST(image_png_roundtrip_and_errors) {
  Image img(3, 2, 0xFF102030u);
  img.pixels[1] = 0x80FFFFFFu;
  std::string png;
  REQUIRE(encodePng(img, png).ok());
  Image back;
  REQUIRE(decodeImage(png, back).ok());
  CHECK_EQ(back.width, 3);
  CHECK_EQ(back.height, 2);
  CHECK_EQ(back.pixels[0], 0xFF102030u);
  CHECK_EQ(back.pixels[1], 0x80FFFFFFu);
  CHECK(!decodeImage("lixo", back).ok());
  CHECK(!decodeImage("", back).ok());

  Image big(400, 300, 0xFFFFFFFFu);
  Image small = downscale(big, 100, 75);
  CHECK_EQ(small.width, 100);
  CHECK_EQ(small.pixels[0], 0xFFFFFFFFu);
  Image cover = downscaleToCover(big, 100, 140);  // cobre 100x140 mantendo proporção
  CHECK(cover.width >= 100 && cover.height >= 140);
  CHECK_EQ(cover.height, 140);
  CHECK_EQ(downscaleToCover(small, 400, 400).width, 100);  // nunca amplia
}

NB_TEST(font_rendering_with_bundled_inter) {
  PosixFileSystem fs;
  Font font;
  REQUIRE(font.loadFromFile(fs, asset("fonts/Inter-Regular.otf")).ok());
  CHECK(font.hasGlyph(0x2605));  // ★
  CHECK(font.hasGlyph(0xE7));    // ç
  int w = text::measure(font, 20, "Nébula");
  CHECK(w > 40 && w < 120);
  CHECK(text::measure(font, 40, "Nébula") > w);
  std::string e = text::ellipsize(font, 20, "Um título muito longo para caber", 120);
  CHECK(text::measure(font, 20, e) <= 120);
  CHECK(e.find("\xE2\x80\xA6") != std::string::npos);
  std::vector<std::string> lines =
      text::wrap(font, 18, "Lenda do Vale Perdido: Edição Definitiva do Colecionador", 150, 2);
  CHECK_EQ(lines.size(), 2u);
  CHECK(lines[1].find("\xE2\x80\xA6") != std::string::npos);
  SoftwareRenderer r(200, 50);
  r.clear(0xFF000000u);
  text::draw(r, font, 24, 5, 5, "Ação", 0xFFFFFFFFu);
  size_t lit = 0;
  for (size_t i = 0; i < r.framebuffer().pixels.size(); ++i) lit += r.framebuffer().pixels[i] != 0xFF000000u;
  CHECK(lit > 50);
  Font broken;
  CHECK(!broken.loadFromMemory("não é fonte").ok());
}

NB_TEST(strings_pt_br_and_format) {
  PosixFileSystem fs;
  Strings s;
  REQUIRE(s.loadFromFile(fs, asset("lang/pt-BR.json")).ok());
  CHECK_EQ(s.language(), std::string("pt-BR"));
  CHECK(s.size() > 100);
  CHECK_EQ(s.format("toast.scan.done", "3", "1"),
           std::string("Varredura concluída: 3 novo(s), 1 ausente(s)"));
  CHECK_EQ(s.get("chave.inexistente"), std::string("chave.inexistente"));
  const char* const required[] = {"category.xbox360", "category.arcade", "category.homebrew",
                                  "category.emulator", "category.xbox_classic", "category.demo",
                                  "format.xex", "format.god", "format.xbla", "format.xbe",
                                  "sort.title", "sort.added", "sort.played", "sort.playcount"};
  for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
    if (!s.has(required[i])) CHECK(s.has(required[i]));
  }
}

NB_TEST(texture_cache_budget_and_failures) {
  MemoryFileSystem fs('/');
  std::string png;
  REQUIRE(encodePng(Image(300, 400, 0xFF445566u), png).ok());
  fs.addFile("/c/a.png", png);
  fs.addFile("/c/b.png", png);
  fs.addFile("/c/c.png", png);
  fs.addFile("/c/ruim.png", "nao-e-png");
  TextureCache cache(fs, 10, 2);
  cache.beginFrame();
  const Image* a = cache.get("/c/a.png", 150, 200);
  REQUIRE(a != NULL);
  CHECK_EQ(a->width, 150);
  CHECK(cache.get("/c/b.png", 150, 200) != NULL);
  CHECK(cache.get("/c/c.png", 150, 200) == NULL);  // orçamento do quadro esgotado
  cache.beginFrame();
  CHECK(cache.get("/c/c.png", 150, 200) != NULL);
  CHECK(cache.get("/c/ruim.png", 150, 200) == NULL);
  cache.beginFrame();
  CHECK(cache.get("/c/ruim.png", 150, 200) == NULL);  // falha fica em cache
  CHECK_EQ(cache.failures(), 1u);
  TextureCache tiny(fs, 2, 5);
  tiny.beginFrame();
  tiny.get("/c/a.png", 10, 10);
  tiny.beginFrame();
  tiny.get("/c/b.png", 10, 10);
  tiny.beginFrame();
  tiny.get("/c/c.png", 10, 10);
  CHECK_EQ(tiny.size(), 2u);  // LRU
}
