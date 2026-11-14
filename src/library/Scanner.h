// Varredura incremental das pastas configuradas pelo usuário.
//
// Não usa threads: step() processa um número limitado de diretórios por
// chamada, e a interface chama uma vez por quadro para continuar responsiva.
//
// Detecção:
//  - pasta com default.xex  -> jogo/app (XEX). Não desce dentro dela.
//  - pasta com default.xbe  -> Xbox clássico. Não desce dentro dela.
//  - arquivo sem extensão dentro de uma pasta com nome de 8 dígitos hex
//    (ex.: Content\0000000000000000\<TitleID>\00007000\<arquivo>) com
//    assinatura STFS e tipo de conteúdo jogável -> GOD / XBLA / demo.
//  - pastas "*.data" (partes de GOD), "$SystemUpdate" e ocultas são ignoradas.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class IFileSystem;

struct ScanRoot {
  std::string path;
  std::string category;  // category::kAuto ou um id de categoria
  int maxDepth;
  bool enabled;
  ScanRoot() : category("auto"), maxDepth(4), enabled(true) {}
};

struct DetectedGame {
  std::string launchPath;
  std::string folder;
  std::string root;
  std::string format;
  std::string category;
  std::string title;
  uint32_t titleId;
  uint32_t mediaId;
  std::string iconPng;  // miniatura do contêiner STFS, se houver
  DetectedGame() : titleId(0), mediaId(0) {}
};

struct ScanStats {
  uint32_t directoriesVisited;
  uint32_t filesExamined;
  uint32_t gamesFound;
  uint32_t errors;
  ScanStats() : directoriesVisited(0), filesExamined(0), gamesFound(0), errors(0) {}
};

class Scanner {
 public:
  explicit Scanner(IFileSystem& fs);

  void start(const std::vector<ScanRoot>& roots);
  // Processa até maxDirectories diretórios. Retorna true quando terminou.
  bool step(uint32_t maxDirectories);
  // Roda até o fim (CLI e testes).
  void runToCompletion();
  void cancel();

  bool running() const { return running_; }
  bool cancelled() const { return cancelled_; }
  const std::string& currentPath() const { return currentPath_; }
  const std::vector<DetectedGame>& results() const { return results_; }
  const ScanStats& stats() const { return stats_; }
  // Pastas de varredura que puderam ser lidas (só nelas jogos que sumiram
  // são marcados como ausentes - um pendrive desconectado não apaga nada).
  const std::vector<std::string>& accessibleRoots() const { return accessibleRoots_; }
  const std::vector<std::string>& errorMessages() const { return errors_; }

 private:
  struct Pending {
    std::string path;
    int depth;
    size_t root;
  };

  void visit(const Pending& item);
  void addXexGame(const Pending& item, const std::string& xexPath);
  void addXbeGame(const Pending& item);
  void examineStfsCandidate(const Pending& item, const std::string& filePath);
  std::string resolveCategory(const Pending& item, const std::string& detected) const;
  void recordError(const Status& status);

  IFileSystem& fs_;
  std::vector<ScanRoot> roots_;
  std::vector<Pending> stack_;
  std::vector<DetectedGame> results_;
  std::vector<std::string> accessibleRoots_;
  std::vector<std::string> errors_;
  ScanStats stats_;
  std::string currentPath_;
  bool running_;
  bool cancelled_;
  NB_NONCOPYABLE(Scanner);
};

}  // namespace nebula
