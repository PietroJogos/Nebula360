#include <string>

#include "TestFramework.h"
#include "core/Json.h"
#include "core/Sha256.h"
#include "platform/MemoryFileSystem.h"
#include "store/Catalog.h"
#include "store/DownloadQueue.h"
#include "store/StorePolicy.h"

using namespace nebula;

namespace {

// Domínio .invalid: reservado (RFC 2606), nunca resolve.
const char* const kUrl = "https://exemplo.invalid/app.zip";

std::string item(const std::string& id, const std::string& license, const std::string& extra,
                 const std::string& sha, const std::string& installPath) {
  return "{\"id\":\"" + id + "\",\"name\":\"App " + id + "\",\"version\":\"1.0\",\"author\":\"Autor\","
         "\"category\":\"tool\",\"license\":\"" + license + "\",\"licenseUrl\":\"https://exemplo.invalid/l\"," +
         extra + "\"files\":[{\"url\":\"" + kUrl + "\",\"installPath\":\"" + installPath +
         "\",\"size\":5,\"sha256\":\"" + sha + "\"}]}";
}

}  // namespace

NB_TEST(catalog_policy_accepts_only_authorized_items) {
  std::string sha = Sha256::hashHex("12345");
  std::string lic = "\"authorization\":{\"type\":\"license\",\"reference\":\"LICENSE\"},";
  std::string perm =
      "\"authorization\":{\"type\":\"written-permission\",\"reference\":\"autorizacao.pdf\","
      "\"grantedBy\":\"Autor\",\"date\":\"2026-11-01\"},";
  std::string doc = "{\"schema\":1,\"name\":\"Teste\",\"items\":[" +
                    item("mit-ok", "MIT", lic, sha, "default.xex") + "," +
                    item("gpl-sem-fonte", "GPL-2.0-only", lic, sha, "default.xex") + "," +
                    item("gpl-ok", "GPL-2.0-only", lic + "\"sourceUrl\":\"https://exemplo.invalid/src\",", sha,
                         "bin/default.xex") + "," +
                    item("nc-sem-permissao", "LicenseRef-NaoComercial", lic, sha, "default.xex") + "," +
                    item("nc-com-permissao", "LicenseRef-NaoComercial", perm, sha, "default.xex") + "," +
                    item("sem-hash", "MIT", lic, "", "default.xex") + "," +
                    item("caminho-ruim", "MIT", lic, sha, "../fora.xex") + "," +
                    item("mit-ok", "MIT", lic, sha, "default.xex") + "]}";
  JsonValue json;
  REQUIRE(parseJson(doc, json).ok());
  Catalog catalog;
  std::vector<CatalogIssue> rejected;
  REQUIRE(parseCatalog(json, catalog, rejected).ok());
  REQUIRE(catalog.items.size() == 3);
  CHECK_EQ(catalog.items[0].id, std::string("mit-ok"));
  CHECK_EQ(catalog.items[1].id, std::string("gpl-ok"));
  CHECK_EQ(catalog.items[2].id, std::string("nc-com-permissao"));
  CHECK_EQ(rejected.size(), 5u);
  CHECK(rejected[0].reason.find("sourceUrl") != std::string::npos);
  CHECK(rejected[1].reason.find("written-permission") != std::string::npos);
  CHECK(rejected[2].reason.find("SHA-256") != std::string::npos);
  CHECK(rejected[3].reason.find("FATX") != std::string::npos);
  CHECK_EQ(rejected[4].reason, std::string("id duplicado"));
}

NB_TEST(catalog_rejects_retail_like_categories_and_bad_documents) {
  CatalogItem it;
  it.id = "jogo-comercial";
  it.name = "X";
  it.version = "1";
  it.author = "Y";
  it.category = "game";  // só "homebrew-game" é aceito
  it.license = "MIT";
  CHECK_EQ(store::evaluateItemPolicy(it).code(), kPolicyViolation);
  JsonValue bad;
  Catalog c;
  std::vector<CatalogIssue> issues;
  REQUIRE(parseJson("{\"schema\":2,\"items\":[]}", bad).ok());
  CHECK_EQ(parseCatalog(bad, c, issues).code(), kUnsupported);
}

NB_TEST(fatx_name_rules) {
  CHECK(store::isValidFatxName("default.xex"));
  CHECK(store::isValidFatxName("Meu App (v1.2)"));
  CHECK(store::isValidFatxName(std::string(42, 'a')));
  CHECK(!store::isValidFatxName(std::string(43, 'a')));
  CHECK(!store::isValidFatxName(""));
  CHECK(!store::isValidFatxName(".."));
  CHECK(!store::isValidFatxName("a:b"));
  CHECK(!store::isValidFatxName("a*b"));
  CHECK(!store::isValidFatxName("final."));
  CHECK(!store::isValidFatxName("acentuação"));
  CHECK(store::validateInstallRelativePath("bin/data/x.bin").ok());
  CHECK(!store::validateInstallRelativePath("/abs").ok());
  CHECK(!store::validateInstallRelativePath("a\\b").ok());
  CHECK(!store::validateInstallRelativePath("Hdd1:/x").ok());
  CHECK(!store::validateInstallRelativePath("a//b").ok());
  CHECK_EQ(store::buildInstallPath("Hdd1:\\Nebula\\Apps", "meu-app", "bin/default.xex", '\\'),
           std::string("Hdd1:\\Nebula\\Apps\\meu-app\\bin\\default.xex"));
}

NB_TEST(download_queue_verifies_and_installs) {
  MemoryFileSystem fs('\\');
  fs.addDirectory("Hdd1:\\Nebula\\Apps");
  MemoryTransport net;
  std::string payload(10000, 'n');
  net.add(kUrl, payload);
  net.setChunkSize(1000);

  CatalogItem it;
  it.id = "meu-app";
  it.name = "Meu App";
  it.version = "1.0";
  it.author = "Autor";
  it.category = "tool";
  it.license = "MIT";
  it.licenseUrl = "https://exemplo.invalid/l";
  it.authorization.type = "license";
  CatalogFile f;
  f.url = kUrl;
  f.installPath = "default.xex";
  f.size = payload.size();
  f.sha256 = Sha256::hashHex(payload);
  it.files.push_back(f);

  std::vector<DownloadRequest> reqs;
  REQUIRE(planInstall(it, "Hdd1:\\Nebula\\Apps", '\\', reqs).ok());
  REQUIRE(reqs.size() == 1);
  DownloadQueue q(fs, net);
  REQUIRE(q.enqueue(reqs[0]).ok());
  CHECK(!q.enqueue(reqs[0]).ok());  // destino duplicado
  // Avanço incremental: 2500 bytes por passo.
  q.step(2500);
  CHECK_EQ(q.jobs()[0].state, kDownloadActive);
  CHECK(q.jobs()[0].received > 0 && q.jobs()[0].received < payload.size());
  CHECK(fs.exists("Hdd1:\\Nebula\\Apps\\meu-app\\default.xex.part"));
  q.runToCompletion();
  CHECK_EQ(q.jobs()[0].state, kDownloadCompleted);
  std::string installed;
  CHECK(fs.fileContents("Hdd1:\\Nebula\\Apps\\meu-app\\default.xex", installed));
  CHECK(installed == payload);
  CHECK(!fs.exists("Hdd1:\\Nebula\\Apps\\meu-app\\default.xex.part"));
}

NB_TEST(download_queue_rejects_tampered_data_and_retries) {
  MemoryFileSystem fs('/');
  MemoryTransport net;
  net.add("https://exemplo.invalid/a", "conteudo-adulterado");
  net.add("https://exemplo.invalid/b", "ok");
  net.add("https://exemplo.invalid/c", "grande-demais");
  net.failNextOpens("https://exemplo.invalid/b", 2);
  DownloadOptions options;
  options.maxAttempts = 3;
  DownloadQueue q(fs, net, options);

  DownloadRequest a;
  a.url = "https://exemplo.invalid/a";
  a.destination = "/apps/a.bin";
  a.expectedSize = 19;
  a.expectedSha256 = Sha256::hashHex("conteudo-original!!");
  REQUIRE(q.enqueue(a).ok());
  DownloadRequest b;
  b.url = "https://exemplo.invalid/b";
  b.destination = "/apps/b.bin";
  b.expectedSize = 2;
  b.expectedSha256 = Sha256::hashHex("ok");
  REQUIRE(q.enqueue(b).ok());
  DownloadRequest c;
  c.url = "https://exemplo.invalid/c";
  c.destination = "/apps/c.bin";
  c.expectedSize = 5;
  c.expectedSha256 = Sha256::hashHex("grand");
  REQUIRE(q.enqueue(c).ok());
  DownloadRequest bad;
  bad.url = "x";
  bad.destination = "/y";
  bad.expectedSize = 1;
  bad.expectedSha256 = "123";
  CHECK_EQ(q.enqueue(bad).code(), kInvalidArgument);

  q.runToCompletion();
  CHECK(q.idle());
  CHECK_EQ(q.jobs()[0].state, kDownloadFailed);
  CHECK_EQ(q.jobs()[0].attempts, 3u);
  CHECK(q.jobs()[0].error.find("SHA-256") != std::string::npos);
  CHECK(!fs.exists("/apps/a.bin"));
  CHECK(!fs.exists("/apps/a.bin.part"));
  CHECK_EQ(q.jobs()[1].state, kDownloadCompleted);
  CHECK_EQ(q.jobs()[1].attempts, 3u);  // duas falhas de rede e depois sucesso
  CHECK_EQ(q.jobs()[2].state, kDownloadFailed);
  CHECK(!fs.exists("/apps/c.bin"));
}

NB_TEST(download_queue_cancel) {
  MemoryFileSystem fs('/');
  MemoryTransport net;
  net.add("https://exemplo.invalid/x", std::string(5000, 'x'));
  net.setChunkSize(100);
  DownloadQueue q(fs, net);
  DownloadRequest r;
  r.url = "https://exemplo.invalid/x";
  r.destination = "/apps/x.bin";
  r.expectedSize = 5000;
  r.expectedSha256 = Sha256::hashHex(std::string(5000, 'x'));
  REQUIRE(q.enqueue(r).ok());
  q.step(300);
  CHECK(fs.exists("/apps/x.bin.part"));
  CHECK(q.cancel("/apps/x.bin"));
  CHECK_EQ(q.jobs()[0].state, kDownloadCancelled);
  CHECK(!fs.exists("/apps/x.bin.part"));
  CHECK(q.idle());
}
