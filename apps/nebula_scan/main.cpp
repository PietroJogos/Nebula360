// nebula_scan - roda o scanner da biblioteca sobre pastas do PC.
//
// Útil para validar a detecção com uma cópia real do HD do Xbox 360
// (por FTP ou leitor FATX), sem precisar do console.
//
//   nebula_scan [--json] [--depth N] [--category ID] PASTA [PASTA...]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Log.h"
#include "core/StringUtil.h"
#include "integrations/LibraryExport.h"
#include "library/Library.h"
#include "library/Scanner.h"
#include "platform/host/PosixFileSystem.h"

using namespace nebula;

int main(int argc, char** argv) {
  bool json = false;
  int depth = 4;
  std::string cat = "auto";
  std::vector<std::string> dirs;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--json") json = true;
    else if (a == "--depth" && i + 1 < argc) depth = std::atoi(argv[++i]);
    else if (a == "--category" && i + 1 < argc) cat = argv[++i];
    else if (!a.empty() && a[0] == '-') {
      std::fprintf(stderr, "uso: nebula_scan [--json] [--depth N] [--category ID] PASTA...\n");
      return 2;
    } else {
      dirs.push_back(a);
    }
  }
  if (dirs.empty()) {
    std::fprintf(stderr, "uso: nebula_scan [--json] [--depth N] [--category ID] PASTA...\n");
    return 2;
  }
  StderrLogSink log;
  Log::addSink(&log);
  Log::setLevel(json ? kLogWarning : kLogInfo);

  PosixFileSystem fs;
  std::vector<ScanRoot> roots;
  for (size_t i = 0; i < dirs.size(); ++i) {
    ScanRoot r;
    r.path = dirs[i];
    r.maxDepth = depth;
    r.category = cat;
    roots.push_back(r);
  }
  Scanner scanner(fs);
  scanner.start(roots);
  scanner.runToCompletion();

  Library library;
  SystemClock clock;
  library.mergeScan(scanner.results(), scanner.accessibleRoots(), clock.unixTimeSeconds(), '/');
  if (json) {
    std::printf("%s", buildLibraryExport(library, clock.unixTimeSeconds()).serialize(true).c_str());
  } else {
    const std::vector<DetectedGame>& r = scanner.results();
    for (size_t i = 0; i < r.size(); ++i) {
      std::printf("%-6s %-12s %s  %s\n    %s\n", r[i].format.c_str(), r[i].category.c_str(),
                  str::toHex32(r[i].titleId).c_str(), r[i].title.c_str(), r[i].launchPath.c_str());
    }
    std::printf("\n%u jogo(s), %u pasta(s), %u erro(s)\n", scanner.stats().gamesFound,
                scanner.stats().directoriesVisited, scanner.stats().errors);
  }
  Log::removeSink(&log);
  return scanner.stats().errors > 0 ? 1 : 0;
}
