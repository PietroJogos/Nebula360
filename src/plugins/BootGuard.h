// Guarda de sessão: detecta encerramento inesperado (queda, travamento,
// desligamento) e recomenda o modo seguro após falhas consecutivas.
//
// No início grava <dados>/session.json com estado "running"; num
// encerramento limpo (ou antes de lançar um jogo, já que no Xbox 360 o
// lançamento não retorna) grava "clean". Se ao iniciar o estado anterior for
// "running", a sessão anterior não terminou bem.
#pragma once

#include <string>

#include "core/Status.h"

namespace nebula {

class IFileSystem;

class BootGuard {
 public:
  BootGuard(IFileSystem& fs, const std::string& path, uint32_t safeModeThreshold = 2);

  // Lê o estado anterior e marca a sessão atual como em execução. Se não
  // conseguir gravar, retorna o erro (o app continua; apenas sem a proteção).
  Status begin();
  Status markCleanExit();
  // Volta a "running" (ex.: o lançamento de um jogo falhou e o app continuou).
  Status markRunning();
  // O usuário pediu para sair do modo seguro.
  Status resetCrashCount();

  bool previousSessionCrashed() const { return previousCrashed_; }
  uint32_t consecutiveCrashes() const { return consecutive_; }
  bool safeModeRecommended() const { return consecutive_ >= threshold_; }

 private:
  Status write(const char* state);

  IFileSystem& fs_;
  std::string path_;
  uint32_t threshold_;
  uint32_t consecutive_;
  bool previousCrashed_;
};

}  // namespace nebula
