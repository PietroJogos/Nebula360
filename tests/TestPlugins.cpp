// Testes do sistema de módulos, da guarda de sessão (encerramento inesperado
// e modo seguro) e do retorno ao dashboard.
#include <string>
#include <vector>

#include "TestFramework.h"
#include "app/BuiltinModules.h"
#include "core/Version.h"
#include "platform/MemoryFileSystem.h"
#include "plugins/BootGuard.h"
#include "plugins/ModuleManifest.h"
#include "plugins/ModuleRegistry.h"

using namespace nebula;

namespace {

std::vector<std::string>& events() {
  static std::vector<std::string> e;
  return e;
}

// Módulo de teste: registra start/stop; falha se o id começar com "falha".
class ProbeModule : public IModule {
 public:
  explicit ProbeModule(const std::string& id) : id_(id) {}
  virtual Status start(ModuleContext&) NB_OVERRIDE {
    if (id_.compare(0, 5, "falha") == 0) return Status(kIoError, "falha simulada ao iniciar");
    events().push_back("start:" + id_);
    return Status::Ok();
  }
  virtual void stop() NB_OVERRIDE { events().push_back("stop:" + id_); }

 private:
  std::string id_;
};

// C++03 não tem lambdas: uma fábrica por id usado nos testes.
#define NB_PROBE(name, id) \
  IModule* name() { return new ProbeModule(id); }
NB_PROBE(makeCore, "nebula.core")
NB_PROBE(makeLibrary, "nebula.library")
NB_PROBE(makeFtp, "nebula.ftp")
NB_PROBE(makeHttp, "nebula.http")
NB_PROBE(makeMedia, "nebula.media")
NB_PROBE(makeFail, "falha.mod")
NB_PROBE(makeA, "mod.a")
NB_PROBE(makeB, "mod.b")
#undef NB_PROBE

ModuleManifest manifest(const std::string& id, bool required) {
  ModuleManifest m;
  m.id = id;
  m.name = id;
  m.version = Version(1, 0, 0);
  m.apiVersion = ModuleRegistry::kApiVersion;
  m.required = required;
  return m;
}

ModuleDependency dep(const std::string& id, const Version& min = Version(), bool optional = false) {
  ModuleDependency d;
  d.id = id;
  d.minVersion = min;
  d.optional = optional;
  return d;
}

ModuleManifest server(const std::string& id, uint32_t port) {
  ModuleManifest m = manifest(id, false);
  m.permissions.push_back("net.server");
  m.ports.push_back(port);
  return m;
}

}  // namespace

NB_TEST(version_parse_and_compare) {
  Version v;
  CHECK(Version::parse("1.2.3", v));
  CHECK_EQ(v.toString(), std::string("1.2.3"));
  CHECK(Version::parse("2", v));
  CHECK_EQ(v.toString(), std::string("2.0.0"));
  CHECK(!Version::parse("1.2.3.4", v));
  CHECK(!Version::parse("1.x", v));
  CHECK(!Version::parse("", v));
  CHECK(Version(1, 2, 0) < Version(1, 10, 0));
  CHECK(Version(0, 3, 0) == Version(0, 3, 0));
}

NB_TEST(manifest_parsing_and_policy) {
  ModuleManifest m;
  REQUIRE(parseModuleManifestText(
              "{\"schema\":1,\"id\":\"nebula.ftp\",\"name\":\"FTP\",\"version\":\"0.1.0\",\"apiVersion\":1,"
              "\"permissions\":[\"net.server\",\"fs.read\"],\"ports\":[21],\"memoryBudgetKiB\":512,"
              "\"platforms\":[\"xbox360\"],\"dependencies\":[{\"id\":\"nebula.core\",\"minVersion\":\"0.3\"}]}",
              m)
              .ok());
  CHECK_EQ(m.ports.size(), 1u);
  CHECK(m.supportsPlatform(kPlatformXbox360));
  CHECK(!m.supportsPlatform(kPlatformHost));
  CHECK_EQ(m.dependencies[0].minVersion.toString(), std::string("0.3.0"));

  const char* const invalid[] = {
      "{\"schema\":1,\"id\":\"X\",\"version\":\"1.0\",\"apiVersion\":1}",                      // id inválido
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"permissions\":[\"root\"]}",
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"ports\":[21]}",  // sem net.server
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"kind\":\"native\"}",
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"kind\":\"script\"}",  // sem sha256
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"um\",\"apiVersion\":1}",
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\"}",                                      // sem apiVersion
      "{\"schema\":2,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1}",
      "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"ports\":[70000],"
      "\"permissions\":[\"net.server\"]}"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    ModuleManifest bad;
    if (parseModuleManifestText(invalid[i], bad).ok()) {
      CHECK(!"manifesto inválido foi aceito");
    }
  }
  ModuleManifest native;
  CHECK_EQ(parseModuleManifestText(
               "{\"schema\":1,\"id\":\"mod.x\",\"version\":\"1.0\",\"apiVersion\":1,\"kind\":\"native\"}", native)
               .code(),
           kPolicyViolation);
}

NB_TEST(builtin_manifests_are_valid_and_start) {
  const std::vector<std::string>& texts = builtinManifestTexts();
  CHECK(texts.size() >= 4);
  for (size_t i = 0; i < texts.size(); ++i) {
    ModuleManifest m;
    CHECK(parseModuleManifestText(texts[i], m).ok());
  }
  ModuleRegistry reg(kPlatformXbox360);
  REQUIRE(registerBuiltinModules(reg).ok());
  reg.setEnabled("nebula.export", false);  // dependeria de integrações
  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());
  CHECK(reg.isRunning("nebula.core"));
  CHECK(reg.isRunning("nebula.library"));
  CHECK_EQ(reg.find("nebula.export")->state, kModuleDisabled);
}

NB_TEST(registry_starts_without_optional_modules) {
  events().clear();
  ModuleRegistry reg(kPlatformHost);
  reg.registerModule(manifest("nebula.core", true), makeCore);
  ModuleManifest lib = manifest("nebula.library", true);
  lib.dependencies.push_back(dep("nebula.core"));
  reg.registerModule(lib, makeLibrary);
  ModuleManifest ftp = server("nebula.ftp", 21);
  ftp.enabledByDefault = false;
  reg.registerModule(ftp, makeFtp);
  ModuleManifest media = manifest("nebula.media", false);
  reg.registerModule(media, makeMedia);
  reg.setEnabled("nebula.media", false);
  CHECK(!reg.setEnabled("nao.existe", true));
  CHECK(!reg.registerModule(manifest("nebula.core", false), makeCore).ok());  // duplicado

  ModuleContext ctx;
  REQUIRE(reg.startAll(ctx).ok());
  CHECK(reg.isRunning("nebula.core"));
  CHECK(reg.isRunning("nebula.library"));
  CHECK_EQ(reg.find("nebula.ftp")->state, kModuleDisabled);
  CHECK_EQ(reg.find("nebula.media")->state, kModuleDisabled);
  REQUIRE(reg.startOrder().size() == 2);
  CHECK_EQ(reg.startOrder()[0], std::string("nebula.core"));  // dependência primeiro
  reg.stopAll();
  REQUIRE(events().size() == 4);
  CHECK_EQ(events()[2], std::string("stop:nebula.library"));  // ordem inversa
  CHECK_EQ(events()[3], std::string("stop:nebula.core"));
  CHECK_EQ(reg.find("nebula.core")->state, kModuleStopped);
}

NB_TEST(registry_handles_missing_and_incompatible_dependencies) {
  ModuleRegistry reg(kPlatformHost);
  reg.registerModule(manifest("nebula.core", true), makeCore);
  ModuleManifest ftp = server("nebula.ftp", 21);
  ftp.dependencies.push_back(dep("nebula.network"));  // ausente
  reg.registerModule(ftp, makeFtp);
  ModuleManifest http = server("nebula.http", 8080);
  http.dependencies.push_back(dep("nebula.core", Version(9, 0, 0)));  // versão alta demais
  reg.registerModule(http, makeHttp);
  ModuleManifest media = manifest("nebula.media", false);
  media.dependencies.push_back(dep("nebula.codecs", Version(), true));  // opcional ausente
  reg.registerModule(media, makeMedia);
  ModuleManifest oldApi = manifest("mod.a", false);
  oldApi.apiVersion = 99;
  reg.registerModule(oldApi, makeA);
  ModuleManifest otherPlatform = manifest("mod.b", false);
  otherPlatform.platforms.push_back("xbox360");
  reg.registerModule(otherPlatform, makeB);

  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());  // só opcionais ficaram de fora
  CHECK_EQ(reg.find("nebula.ftp")->state, kModuleSkipped);
  CHECK(reg.find("nebula.ftp")->reason.find("dependência ausente: nebula.network") != std::string::npos);
  CHECK_EQ(reg.find("nebula.http")->state, kModuleIncompatible);
  CHECK(reg.find("nebula.http")->reason.find(">= 9.0.0") != std::string::npos);
  CHECK(reg.isRunning("nebula.media"));
  CHECK_EQ(reg.find("mod.a")->state, kModuleIncompatible);
  CHECK_EQ(reg.find("mod.b")->state, kModuleIncompatible);
}

NB_TEST(registry_isolates_start_failures) {
  events().clear();
  ModuleRegistry reg(kPlatformHost);
  reg.registerModule(manifest("nebula.core", true), makeCore);
  reg.registerModule(manifest("falha.mod", false), makeFail);
  ModuleManifest dependent = manifest("mod.a", false);
  dependent.dependencies.push_back(dep("falha.mod"));
  reg.registerModule(dependent, makeA);
  reg.registerModule(manifest("mod.b", false), makeB);
  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());
  CHECK_EQ(reg.find("falha.mod")->state, kModuleFailed);
  CHECK(reg.find("falha.mod")->reason.find("falha simulada") != std::string::npos);
  CHECK_EQ(reg.find("mod.a")->state, kModuleSkipped);
  CHECK(reg.isRunning("mod.b"));
  CHECK(reg.isRunning("nebula.core"));

  // Obrigatório que falha: startAll informa o erro (o app decide o que fazer).
  ModuleRegistry strict(kPlatformHost);
  strict.registerModule(manifest("falha.mod", true), makeFail);
  ModuleContext ctx2;
  Status s = strict.startAll(ctx2);
  CHECK(!s.ok());
  CHECK(s.message().find("falha.mod") != std::string::npos);
}

NB_TEST(registry_detects_port_conflicts) {
  ModuleRegistry reg(kPlatformHost);
  reg.registerModule(server("nebula.ftp", 21), makeFtp);
  ModuleManifest http = server("nebula.http", 21);  // mesma porta
  http.required = true;  // obrigatório vence, mesmo registrado depois
  reg.registerModule(http, makeHttp);
  ModuleManifest media = manifest("nebula.media", false);
  media.conflicts.push_back("nebula.http");
  reg.registerModule(media, makeMedia);
  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());
  CHECK(reg.isRunning("nebula.http"));
  CHECK_EQ(reg.find("nebula.ftp")->state, kModuleSkipped);
  CHECK(reg.find("nebula.ftp")->reason.find("porta 21") != std::string::npos);
  CHECK_EQ(reg.find("nebula.media")->state, kModuleSkipped);
  CHECK(reg.find("nebula.media")->reason.find("conflito") != std::string::npos);
}

NB_TEST(registry_low_memory_skips_optional_modules) {
  ModuleRegistry reg(kPlatformHost);
  ModuleManifest core = manifest("nebula.core", true);
  core.memoryBudgetKiB = 6000;
  reg.registerModule(core, makeCore);
  ModuleManifest media = manifest("nebula.media", false);
  media.memoryBudgetKiB = 8000;
  reg.registerModule(media, makeMedia);
  ModuleManifest small = manifest("mod.a", false);
  small.memoryBudgetKiB = 1000;
  reg.registerModule(small, makeA);
  reg.setMemoryLimitKiB(8000);
  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());
  CHECK(reg.isRunning("nebula.core"));
  CHECK_EQ(reg.find("nebula.media")->state, kModuleSkipped);
  CHECK(reg.find("nebula.media")->reason.find("memória insuficiente") != std::string::npos);
  CHECK(reg.isRunning("mod.a"));
  CHECK_EQ(reg.memoryInUseKiB(), 7000u);

  ModuleRegistry tiny(kPlatformHost);
  tiny.registerModule(core, makeCore);
  tiny.setMemoryLimitKiB(1000);
  ModuleContext ctx2;
  CHECK(!tiny.startAll(ctx2).ok());  // nem o obrigatório cabe
}

NB_TEST(registry_safe_mode_and_cycles) {
  ModuleRegistry reg(kPlatformHost);
  reg.registerModule(manifest("nebula.core", true), makeCore);
  reg.registerModule(manifest("nebula.media", false), makeMedia);
  reg.setSafeMode(true);
  ModuleContext ctx;
  CHECK(reg.startAll(ctx).ok());
  CHECK(reg.isRunning("nebula.core"));
  CHECK_EQ(reg.find("nebula.media")->state, kModuleDisabled);
  CHECK_EQ(reg.find("nebula.media")->reason, std::string("modo seguro"));
  CHECK(!reg.startAll(ctx).ok());  // não reinicia duas vezes

  ModuleRegistry cyc(kPlatformHost);
  ModuleManifest a = manifest("mod.a", false);
  a.dependencies.push_back(dep("mod.b"));
  ModuleManifest b = manifest("mod.b", false);
  b.dependencies.push_back(dep("mod.a"));
  cyc.registerModule(a, makeA);
  cyc.registerModule(b, makeB);
  cyc.registerModule(manifest("nebula.core", true), makeCore);
  ModuleContext ctx2;
  CHECK(cyc.startAll(ctx2).ok());
  CHECK(!cyc.isRunning("mod.a"));
  CHECK(!cyc.isRunning("mod.b"));
  CHECK(cyc.isRunning("nebula.core"));
  bool cycleReported = cyc.find("mod.a")->reason.find("ciclo") != std::string::npos ||
                       cyc.find("mod.b")->reason.find("ciclo") != std::string::npos;
  CHECK(cycleReported);
}

NB_TEST(bootguard_detects_unexpected_shutdown_and_recommends_safe_mode) {
  MemoryFileSystem fs('\\');
  fs.addDirectory("Hdd1:\\Nebula");
  const std::string path = "Hdd1:\\Nebula\\session.json";
  {
    BootGuard g(fs, path);
    REQUIRE(g.begin().ok());
    CHECK(!g.previousSessionCrashed());
    // sem markCleanExit: simula queda
  }
  {
    BootGuard g(fs, path);
    REQUIRE(g.begin().ok());
    CHECK(g.previousSessionCrashed());
    CHECK_EQ(g.consecutiveCrashes(), 1u);
    CHECK(!g.safeModeRecommended());
  }
  {
    BootGuard g(fs, path);
    REQUIRE(g.begin().ok());
    CHECK_EQ(g.consecutiveCrashes(), 2u);
    CHECK(g.safeModeRecommended());
    CHECK(g.markCleanExit().ok());
  }
  {
    BootGuard g(fs, path);
    REQUIRE(g.begin().ok());
    CHECK(!g.previousSessionCrashed());
    CHECK_EQ(g.consecutiveCrashes(), 0u);
    g.markCleanExit();
  }
  fs.addFile(path, "{ ilegível");
  {
    BootGuard g(fs, path);
    REQUIRE(g.begin().ok());  // arquivo ruim não é tratado como queda
    CHECK(!g.previousSessionCrashed());
  }
  fs.failPath("Hdd1:\\Nebula");
  {
    BootGuard g(fs, path);
    CHECK(!g.begin().ok());  // HDD com erro: informa, mas não impede o app
    CHECK(!g.safeModeRecommended());
  }
}
