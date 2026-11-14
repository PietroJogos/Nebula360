// Testes de robustez da fila de downloads: queda de conexão, retomada,
// servidor sem suporte a retomada, parcial corrompido, disco cheio,
// limite de velocidade e retomada após reiniciar o aplicativo.
#include <string>

#include "TestFramework.h"
#include "core/Sha256.h"
#include "platform/Clock.h"
#include "platform/MemoryFileSystem.h"
#include "store/DownloadQueue.h"

using namespace nebula;

namespace {

const char* const kUrl = "https://exemplo.invalid/pacote.zip";

std::string payload() {
  std::string p;
  for (int i = 0; i < 10000; ++i) p += static_cast<char>('a' + (i * 7) % 26);
  return p;
}

DownloadRequest request(const std::string& data) {
  DownloadRequest r;
  r.url = kUrl;
  r.destination = "Hdd1:\\Nebula\\Apps\\app\\pacote.zip";
  r.expectedSize = data.size();
  r.expectedSha256 = Sha256::hashHex(data);
  return r;
}

DownloadOptions noMargin(uint32_t attempts) {
  DownloadOptions o;
  o.maxAttempts = attempts;
  o.freeSpaceMarginBytes = 0;
  return o;
}

}  // namespace

NB_TEST(download_resumes_after_connection_loss) {
  MemoryFileSystem fs('\\');
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  net.setChunkSize(1000);
  net.dropAfter(kUrl, 4000, 1);  // primeira conexão cai em 4000 bytes
  DownloadQueue q(fs, net, noMargin(3));
  REQUIRE(q.enqueue(request(data)).ok());
  q.runToCompletion();
  const DownloadJob& j = q.jobs()[0];
  CHECK_EQ(j.state, kDownloadCompleted);
  CHECK_EQ(j.attempts, 2u);
  CHECK_EQ(j.resumedFrom, 4000u);
  CHECK_EQ(net.servedBytes(), 10000u);  // nada foi baixado duas vezes
  std::string out;
  CHECK(fs.fileContents("Hdd1:\\Nebula\\Apps\\app\\pacote.zip", out));
  CHECK(out == data);
  CHECK(!fs.exists("Hdd1:\\Nebula\\Apps\\app\\pacote.zip.part"));
}

NB_TEST(download_restarts_when_server_ignores_range) {
  MemoryFileSystem fs('\\');
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  net.setChunkSize(1000);
  net.setSupportsRanges(false);
  net.dropAfter(kUrl, 4000, 1);
  DownloadQueue q(fs, net, noMargin(3));
  REQUIRE(q.enqueue(request(data)).ok());
  q.runToCompletion();
  CHECK_EQ(q.jobs()[0].state, kDownloadCompleted);
  CHECK_EQ(q.jobs()[0].resumedFrom, 0u);
  CHECK_EQ(net.servedBytes(), 14000u);  // recomeçou do zero, sem corromper
  std::string out;
  CHECK(fs.fileContents("Hdd1:\\Nebula\\Apps\\app\\pacote.zip", out));
  CHECK(out == data);
}

NB_TEST(download_discards_stale_partial_with_wrong_data) {
  MemoryFileSystem fs('\\');
  fs.addFile("Hdd1:\\Nebula\\Apps\\app\\pacote.zip.part", std::string(3000, 'X'));  // lixo antigo
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  DownloadQueue q(fs, net, noMargin(3));
  REQUIRE(q.enqueue(request(data)).ok());
  q.runToCompletion();
  const DownloadJob& j = q.jobs()[0];
  CHECK_EQ(j.state, kDownloadCompleted);
  CHECK_EQ(j.attempts, 2u);  // 1ª: retoma o lixo e falha no SHA-256; 2ª: do zero
  std::string out;
  CHECK(fs.fileContents("Hdd1:\\Nebula\\Apps\\app\\pacote.zip", out));
  CHECK(out == data);
}

NB_TEST(download_refuses_when_disk_is_full) {
  MemoryFileSystem fs('\\');
  fs.setCapacity(5000);
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  DownloadQueue q(fs, net, noMargin(3));
  REQUIRE(q.enqueue(request(data)).ok());
  q.runToCompletion();
  const DownloadJob& j = q.jobs()[0];
  CHECK_EQ(j.state, kDownloadFailed);
  CHECK_EQ(j.attempts, 1u);  // esperar não libera espaço: não insiste
  CHECK(j.error.find("espaço insuficiente") != std::string::npos);
  CHECK_EQ(net.servedBytes(), 0u);
  CHECK(!fs.exists("Hdd1:\\Nebula\\Apps\\app\\pacote.zip.part"));

  // Com margem de segurança padrão (1 MiB), 10 KB livres a mais não bastam.
  MemoryFileSystem fs2('\\');
  fs2.setCapacity(20000);
  DownloadQueue q2(fs2, net, DownloadOptions());
  REQUIRE(q2.enqueue(request(data)).ok());
  q2.runToCompletion();
  CHECK_EQ(q2.jobs()[0].state, kDownloadFailed);
}

NB_TEST(download_respects_rate_limit) {
  MemoryFileSystem fs('\\');
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  ManualClock clock(0);
  DownloadOptions o = noMargin(1);
  o.rateLimitBytesPerSecond = 2000;
  DownloadQueue q(fs, net, o, &clock);
  REQUIRE(q.enqueue(request(data)).ok());
  int steps = 0;
  uint64_t previous = 0;
  bool withinLimit = true;
  while (q.step(0) && steps < 1000) {
    clock.advance(500);  // meio segundo por passo -> no máximo 1000 bytes
    uint64_t now = q.jobs()[0].received;
    if (now - previous > 1000) withinLimit = false;
    previous = now;
    ++steps;
  }
  CHECK(withinLimit);
  CHECK_EQ(q.jobs()[0].state, kDownloadCompleted);
  CHECK(steps >= 10);  // 10 KB a 2 KB/s = ~5 s simulados
}

NB_TEST(download_resumes_after_app_restart) {
  MemoryFileSystem fs('\\');
  MemoryTransport net;
  std::string data = payload();
  net.add(kUrl, data);
  net.setChunkSize(1000);
  net.dropAfter(kUrl, 6000, 1);
  {
    DownloadQueue first(fs, net, noMargin(1));  // uma tentativa: falha e para
    REQUIRE(first.enqueue(request(data)).ok());
    first.runToCompletion();
    CHECK_EQ(first.jobs()[0].state, kDownloadFailed);
  }
  CHECK(fs.exists("Hdd1:\\Nebula\\Apps\\app\\pacote.zip.part"));  // parcial preservado
  DownloadQueue second(fs, net, noMargin(1));
  REQUIRE(second.enqueue(request(data)).ok());
  second.runToCompletion();
  CHECK_EQ(second.jobs()[0].state, kDownloadCompleted);
  CHECK_EQ(second.jobs()[0].resumedFrom, 6000u);
  CHECK_EQ(net.servedBytes(), 10000u);
}
