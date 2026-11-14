#include <string>

#include "TestFramework.h"
#include "core/Color.h"
#include "core/Json.h"
#include "core/Log.h"
#include "core/Path.h"
#include "core/Sha256.h"
#include "core/StringUtil.h"
#include "platform/MemoryFileSystem.h"

using namespace nebula;

NB_TEST(utf8_decode_encode_roundtrip) {
  std::string s = "N\xC3\xA9" "bula \xE2\x98\x85 \xF0\x9F\x8E\xAE";  // Nébula ★ 🎮
  size_t pos = 0;
  uint32_t cp = 0;
  std::string rebuilt;
  size_t count = 0;
  while (str::decodeUtf8(s, pos, cp)) {
    str::appendUtf8(rebuilt, cp);
    ++count;
  }
  CHECK_EQ(rebuilt, s);
  CHECK_EQ(count, 10u);
  CHECK_EQ(str::utf8Length(s), 10u);
}

NB_TEST(utf8_invalid_bytes_become_replacement) {
  std::string s = "a\xFF" "b\xC3";  // byte inválido e sequência truncada
  size_t pos = 0;
  uint32_t cp = 0;
  std::vector<uint32_t> cps;
  while (str::decodeUtf8(s, pos, cp)) cps.push_back(cp);
  REQUIRE(cps.size() == 4);
  CHECK_EQ(cps[0], 'a');
  CHECK_EQ(cps[1], 0xFFFDu);
  CHECK_EQ(cps[2], 'b');
  CHECK_EQ(cps[3], 0xFFFDu);
}

NB_TEST(utf16be_conversion_with_surrogates) {
  const uint8_t data[] = {0x00, 'O', 0x00, 0xE9, 0xD8, 0x3C, 0xDF, 0xAE, 0x00, 0x00, 0x00, 'X'};
  CHECK_EQ(str::utf16beToUtf8(data, sizeof(data)), std::string("O\xC3\xA9\xF0\x9F\x8E\xAE"));
}

NB_TEST(fold_for_search_removes_accents_and_punctuation) {
  CHECK_EQ(str::foldForSearch("Nébula 360"), std::string("nebula 360"));
  CHECK_EQ(str::foldForSearch("  AÇÃO: Coração—Ñandú!! "), std::string("acao coracao nandu"));
  CHECK_EQ(str::foldForSearch("Straße Œuvre Łódź"), std::string("strasse oeuvre lodz"));
  CHECK_EQ(str::foldForSearch("x-men_origins"), std::string("x men origins"));
}

NB_TEST(string_helpers) {
  CHECK_EQ(str::trim("  a b \t\n"), std::string("a b"));
  CHECK_EQ(str::split("a,,b", ',').size(), 3u);
  CHECK_EQ(str::splitWhitespace("  um  dois tres ").size(), 3u);
  CHECK(str::iequalsAscii("Hdd1:\\Games", "HDD1:\\games"));
  CHECK_EQ(str::replaceAll("a{0}b{0}", "{0}", "X"), std::string("aXbX"));
  CHECK_EQ(str::toHex32(0x05AB81F0), std::string("05AB81F0"));
  uint32_t v = 0;
  CHECK(str::parseHex32("0x4D5307E6", v));
  CHECK_EQ(v, 0x4D5307E6u);
  CHECK(!str::parseHex32("XYZ", v));
  CHECK(!str::parseHex32("123456789", v));
  int64_t n = 0;
  CHECK(str::parseInt64("-42", n));
  CHECK_EQ(n, -42);
  CHECK(!str::parseInt64("99999999999999999999", n));
  CHECK_EQ(str::formatBytes(512), std::string("512 B"));
  CHECK_EQ(str::formatBytes(1536), std::string("1,5 KB"));
  CHECK_EQ(str::formatBytes(8630272), std::string("8,2 MB"));
}

NB_TEST(format_date_utc) {
  CHECK_EQ(str::formatDate(0), std::string("01/01/1970"));
  CHECK_EQ(str::formatDate(951782400), std::string("29/02/2000"));
  CHECK_EQ(str::formatDate(1700000000), std::string("14/11/2023"));
  CHECK_EQ(str::formatDate(1794245054), std::string("09/11/2026"));
}

NB_TEST(sha256_nist_vectors) {
  CHECK_EQ(Sha256::hashHex(""),
           std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK_EQ(Sha256::hashHex("abc"),
           std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(Sha256::hashHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
           std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  Sha256 h;
  std::string chunk(1000, 'a');
  for (int i = 0; i < 1000; ++i) h.update(chunk.data(), chunk.size());
  CHECK_EQ(h.finishHex(),
           std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

NB_TEST(json_parse_and_serialize_roundtrip) {
  std::string text =
      "{\"nome\": \"N\\u00e9bula\", \"n\": 360, \"x\": -1.5, \"ok\": true, \"nada\": null,"
      " \"lista\": [1, \"dois\", {\"tres\": 3}], \"emoji\": \"\\ud83c\\udfae\"}";
  JsonValue v;
  Status s = parseJson(text, v);
  REQUIRE(s.ok());
  CHECK_EQ(v.get("nome").asString(), std::string("N\xC3\xA9" "bula"));
  CHECK_EQ(v.get("n").asInt(), 360);
  CHECK(v.get("x").asNumber() == -1.5);
  CHECK(v.get("ok").asBool());
  CHECK(v.get("nada").isNull());
  CHECK_EQ(v.get("lista").size(), 3u);
  CHECK_EQ(v.get("lista").at(2).get("tres").asInt(), 3);
  CHECK_EQ(v.get("emoji").asString(), std::string("\xF0\x9F\x8E\xAE"));
  CHECK(v.get("ausente").isNull());
  // Ordem de inserção preservada e reparse idêntico.
  CHECK_EQ(v.memberAt(0).first, std::string("nome"));
  JsonValue again;
  REQUIRE(parseJson(v.serialize(true), again).ok());
  CHECK_EQ(again.serialize(false), v.serialize(false));
}

NB_TEST(json_reports_errors_with_position) {
  JsonValue v;
  Status s = parseJson("{\n  \"a\": 1,\n  \"b\": [1, 2,]\n}", v);
  CHECK(!s.ok());
  CHECK_EQ(s.code(), kParseError);
  CHECK(s.message().find("linha 3") != std::string::npos);
  CHECK(!parseJson("{\"a\": 1} lixo", v).ok());
  CHECK(!parseJson("\"sem fim", v).ok());
  CHECK(!parseJson("[01]", v).ok());
  CHECK(!parseJson("{\"a\": tru}", v).ok());
}

NB_TEST(json_depth_limit_prevents_stack_exhaustion) {
  std::string deep(500, '[');
  deep += std::string(500, ']');
  JsonValue v;
  Status s = parseJson(deep, v);
  CHECK(!s.ok());
  CHECK(s.message().find("profundo") != std::string::npos);
}

NB_TEST(json_escapes_control_characters) {
  JsonValue o = JsonValue::object();
  o.set("t", std::string("a\"b\\c\n\x01"));
  CHECK_EQ(o.serialize(false), std::string("{\"t\":\"a\\\"b\\\\c\\n\\u0001\"}"));
}

NB_TEST(json_self_assignment_of_child_is_safe) {
  JsonValue root = JsonValue::object();
  root.set("child", JsonValue::object()).set("x", 1);
  root = root.get("child");  // atribuir um filho ao próprio pai
  CHECK_EQ(root.get("x").asInt(), 1);
}

NB_TEST(path_helpers_xbox_and_posix) {
  CHECK_EQ(path::join("Hdd1:", "Games", '\\'), std::string("Hdd1:\\Games"));
  CHECK_EQ(path::join("Hdd1:\\Games\\", "\\Aventura", '\\'), std::string("Hdd1:\\Games\\Aventura"));
  CHECK_EQ(path::parent("Hdd1:\\Games\\Aventura\\default.xex", '\\'), std::string("Hdd1:\\Games\\Aventura"));
  CHECK_EQ(path::parent("Hdd1:\\Games", '\\'), std::string("Hdd1:"));
  CHECK_EQ(path::fileName("Hdd1:\\Games\\Aventura\\", '\\'), std::string("Aventura"));
  CHECK_EQ(path::extensionLower("Usb0:\\x\\DEFAULT.XEX", '\\'), std::string("xex"));
  CHECK_EQ(path::extensionLower("/a/.oculto", '/'), std::string(""));
  CHECK_EQ(path::stem("/a/capa.png", '/'), std::string("capa"));
  CHECK(path::isWithin("Hdd1:\\Games\\Jogo\\default.xex", "hdd1:\\games", '\\'));
  CHECK(!path::isWithin("Hdd1:\\GamesExtra\\x", "Hdd1:\\Games", '\\'));
  CHECK(path::isWithin("/home/u/jogos/a", "/home/u/jogos/", '/'));
}

NB_TEST(color_parse_and_format) {
  Color c = 0;
  CHECK(parseColor("#8B5CF6", c));
  CHECK_EQ(c, 0xFF8B5CF6u);
  CHECK(parseColor("#FFFFFF14", c));
  CHECK_EQ(c, 0x14FFFFFFu);
  CHECK_EQ(colorToString(0x14FFFFFFu), std::string("#FFFFFF14"));
  CHECK_EQ(colorToString(0xFF0B0F2Au), std::string("#0B0F2A"));
  CHECK(!parseColor("8B5CF6", c));
  CHECK(!parseColor("#12345", c));
  CHECK(!parseColor("#GGGGGG", c));
  CHECK_EQ(lerpColor(0xFF000000u, 0xFFFFFFFFu, 255), 0xFFFFFFFFu);
}

NB_TEST(status_and_result) {
  Status s(kNotFound, "x");
  CHECK_EQ(s.withContext("ctx").message(), std::string("ctx: x"));
  CHECK_EQ(s.toString(), std::string("NOT_FOUND: x"));
  Result<int> r(5);
  CHECK(r.ok());
  Result<int> e(Status(kIoError, "falhou"));
  CHECK(!e.ok());
  Result<int> wrong((Status()));
  CHECK(!wrong.ok());  // Result com Status OK e sem valor vira erro interno
}

NB_TEST(log_sinks_and_file_rotation) {
  MemoryFileSystem fs('/');
  fs.addDirectory("/logs");
  fs.addFile("/logs/n.log", std::string(2000, 'x'));
  MemoryLogSink mem;
  Log::addSink(&mem);
  LogLevel old = Log::level();
  Log::setLevel(kLogInfo);
  {
    FileLogSink file(fs, "/logs/n.log", 1000);  // > 1000 bytes: rotaciona
    Log::addSink(&file);
    NB_DEBUG("t", "invisível");
    NB_INFO("t", "olá " << 42);
    NB_WARN("t", "cuidado");
    Log::removeSink(&file);
  }
  Log::setLevel(old);
  Log::removeSink(&mem);
  CHECK(mem.contains("olá 42"));
  CHECK(!mem.contains("invisível"));
  CHECK_EQ(mem.count(kLogWarning), 1u);
  std::string rotated, current;
  CHECK(fs.fileContents("/logs/n.log.1", rotated));
  CHECK_EQ(rotated.size(), 2000u);
  CHECK(fs.fileContents("/logs/n.log", current));
  CHECK(current.find("cuidado") != std::string::npos);
  CHECK(current.find("olá 42") != std::string::npos);
}

NB_TEST(memory_filesystem_is_case_insensitive_like_fatx) {
  MemoryFileSystem fs('\\');
  fs.addFile("Hdd1:\\Games\\Jogo\\default.xex", "XEX2");
  CHECK(fs.exists("HDD1:\\games\\JOGO\\DEFAULT.XEX"));
  CHECK(fs.isDirectory("hdd1:\\games"));
  std::vector<DirEntry> list;
  CHECK(fs.listDirectory("Hdd1:\\Games", list).ok());
  REQUIRE(list.size() == 1);
  CHECK_EQ(list[0].name, std::string("Jogo"));
  CHECK(list[0].isDirectory);
  fs.failPath("Hdd1:\\Games");
  CHECK_EQ(fs.listDirectory("Hdd1:\\Games\\Jogo", list).code(), kIoError);
}
