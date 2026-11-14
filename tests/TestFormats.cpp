#include <string>

#include "TestData.h"
#include "TestFramework.h"
#include "formats/StfsHeader.h"
#include "formats/XexHeader.h"
#include "platform/host/PosixFileSystem.h"
#include "ui/Image.h"

using namespace nebula;

namespace {
const uint8_t* bytes(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

std::string tinyPng() {
  Image img(4, 4, 0xFF336699u);
  std::string png;
  encodePng(img, png);
  return png;
}
}  // namespace

NB_TEST(xex_header_with_execution_info) {
  std::string x = testdata::makeXex(true, 0x4D5307E6, 0x12345678, "Jogo.exe");
  XexInfo info;
  REQUIRE(parseXexHeader(bytes(x), x.size(), info).ok());
  CHECK(info.isTitle());
  CHECK(info.hasExecutionInfo);
  CHECK_EQ(info.titleId, 0x4D5307E6u);
  CHECK_EQ(info.mediaId, 0x12345678u);
  CHECK_EQ(info.discNumber, 1);
  CHECK_EQ(info.originalPeName, std::string("Jogo.exe"));
  CHECK(info.hasFileFormat);
  CHECK_EQ(info.encryptionType, 1);
  CHECK_EQ(info.compressionType, 1);
  CHECK_EQ(info.entryPoint, 0x82001000u);
  CHECK_EQ(info.gameRegion, 0xFFFFFFFFu);
  CHECK(!looksLikeHomebrewTitleId(info));
}

NB_TEST(xex_header_without_execution_info_is_homebrew) {
  std::string x = testdata::makeXex(false, 0, 0, "App.exe");
  XexInfo info;
  REQUIRE(parseXexHeader(bytes(x), x.size(), info).ok());
  CHECK(!info.hasExecutionInfo);
  CHECK(looksLikeHomebrewTitleId(info));
}

NB_TEST(xex_header_rejects_garbage_and_truncation) {
  XexInfo info;
  std::string junk(64, 'Z');
  CHECK_EQ(parseXexHeader(bytes(junk), junk.size(), info).code(), kParseError);
  std::string x = testdata::makeXex(true, 1, 1, "a.exe");
  CHECK(!parseXexHeader(bytes(x), 10, info).ok());
  std::string xex1 = x;
  xex1[3] = '1';
  CHECK_EQ(parseXexHeader(bytes(xex1), xex1.size(), info).code(), kUnsupported);
  // Quantidade absurda de cabeçalhos opcionais.
  std::string many = x;
  testdata::putBe32(many, 0x14, 100000);
  CHECK(!parseXexHeader(bytes(many), many.size(), info).ok());
}

NB_TEST(xex_header_ignores_out_of_bounds_offsets) {
  std::string x = testdata::makeXex(true, 0x1234, 1, "a.exe");
  // Aponta EXECUTION_INFO e o nome PE para muito além do buffer, inclusive
  // valores perto de 0xFFFFFFFF que dariam a volta em size_t de 32 bits.
  testdata::putBe32(x, 0x18 + 3 * 8 + 4, 0xFFFFFFF0u);
  testdata::putBe32(x, 0x18 + 2 * 8 + 4, 0x7FFFFFFFu);
  testdata::putBe32(x, 0x10, 0xFFFFFF00u);
  XexInfo info;
  REQUIRE(parseXexHeader(bytes(x), x.size(), info).ok());
  CHECK(!info.hasExecutionInfo);
  CHECK(info.originalPeName.empty());
  CHECK(!info.hasGameRegion);
}

NB_TEST(xex_reference_files_if_present) {
  // Os XEX originais não são versionados (ver references/xex/README.md).
  PosixFileSystem fs;
  std::string dir = std::string(NEBULA_SOURCE_DIR) + "/references/xex/original/";
  if (!fs.exists(dir + "Aurora.xex") || !fs.exists(dir + "GodStix.xex")) {
    SKIP("XEX de referência ausentes neste checkout");
  }
  XexInfo aurora;
  REQUIRE(readXexInfo(fs, dir + "Aurora.xex", aurora).ok());
  CHECK_EQ(aurora.originalPeName, std::string("Aurora.exe"));
  CHECK(!aurora.hasExecutionInfo);
  CHECK_EQ(aurora.encryptionType, 1);
  CHECK_EQ(aurora.entryPoint, 0x828050E0u);
  CHECK_EQ(aurora.resourceNames.size(), 8u);
  XexInfo godstix;
  REQUIRE(readXexInfo(fs, dir + "GodStix.xex", godstix).ok());
  CHECK_EQ(godstix.originalPeName, std::string("GodStixStore.exe"));
  CHECK(godstix.hasExecutionInfo);
  CHECK_EQ(godstix.titleId, 0x05AB81F0u);
}

NB_TEST(stfs_header_god_container) {
  std::string png = tinyPng();
  std::string s = testdata::makeStfs(kContentGamesOnDemand, 0x4D5307E6, "Aventura Exemplo", png);
  StfsInfo info;
  REQUIRE(parseStfsHeader(bytes(s), s.size(), info).ok());
  CHECK_EQ(info.magic, std::string("LIVE"));
  CHECK_EQ(info.contentType, static_cast<uint32_t>(kContentGamesOnDemand));
  CHECK_EQ(info.titleId, 0x4D5307E6u);
  CHECK_EQ(info.mediaId, 0xABCD0001u);
  CHECK_EQ(info.displayName, std::string("Aventura Exemplo"));
  CHECK_EQ(info.publisherName, std::string("Estudio Exemplo"));
  CHECK_EQ(info.bestName(), std::string("Aventura Exemplo"));
  CHECK_EQ(info.thumbnailPng, png);
  CHECK_EQ(info.titleThumbnailPng, png);
  CHECK(isLaunchableContentType(info.contentType));
  CHECK_EQ(std::string(stfsContentTypeName(info.contentType)), std::string("GamesOnDemand"));
}

NB_TEST(stfs_header_rejects_bad_thumbnails_and_truncation) {
  std::string s = testdata::makeStfs(kContentArcadeTitle, 0x58410001, "Arcade", "");
  // Tamanho de miniatura declarado, mas conteúdo não é PNG.
  testdata::putBe32(s, 0x1712, 100);
  StfsInfo info;
  REQUIRE(parseStfsHeader(bytes(s), s.size(), info).ok());
  CHECK(info.thumbnailPng.empty());
  // Tamanho maior que o permitido.
  testdata::putBe32(s, 0x1712, 0x10000);
  REQUIRE(parseStfsHeader(bytes(s), s.size(), info).ok());
  CHECK(info.thumbnailPng.empty());
  CHECK(!parseStfsHeader(bytes(s), 0x500, info).ok());
  std::string notStfs = s;
  notStfs[0] = 'X';
  CHECK(!parseStfsHeader(bytes(notStfs), notStfs.size(), info).ok());
  CHECK(!isLaunchableContentType(kContentSavedGame));
  CHECK(!isLaunchableContentType(kContentInstaller));
}
