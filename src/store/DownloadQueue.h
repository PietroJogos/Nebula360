// Fila de downloads incremental (sem threads), com verificação de integridade.
//
// Fluxo de cada arquivo:
//   1. confere o espaço livre no volume de destino;
//   2. baixa para "<destino>.part"; se já existir um .part, tenta RETOMAR
//      pedindo os bytes restantes (o transporte informa se o servidor aceitou);
//   3. confere tamanho e SHA-256 e só então renomeia para o destino.
// Falhas de rede mantêm o .part (para retomar); falhas de integridade apagam
// o .part. Nunca grava além do tamanho esperado. Velocidade opcionalmente
// limitada (token bucket).
//
// O transporte é abstrato. Nesta fase existe só o MemoryTransport (testes);
// HTTP/HTTPS no console (sockets + TLS) fica para a fase da loja.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Sha256.h"
#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class IClock;
class IFileSystem;
struct CatalogItem;

class IDownloadTransport {
 public:
  virtual ~IDownloadTransport() {}
  // Abre a URL a partir de "offset". "rangeHonored" = o servidor enviará a
  // partir do offset (se false, enviará desde o byte 0). "remaining" = bytes
  // que virão (0 se desconhecido).
  virtual Status open(const std::string& url, uint64_t offset, uint64_t& remaining,
                      bool& rangeHonored) = 0;
  virtual Status read(char* buffer, size_t capacity, size_t& got, bool& finished) = 0;
  virtual void close() = 0;
};

enum DownloadState {
  kDownloadQueued = 0,
  kDownloadActive,
  kDownloadCompleted,
  kDownloadFailed,
  kDownloadCancelled
};
const char* downloadStateName(DownloadState state);

struct DownloadRequest {
  std::string itemId;
  std::string url;
  std::string destination;
  uint64_t expectedSize;
  std::string expectedSha256;
  DownloadRequest() : expectedSize(0) {}
};

struct DownloadJob {
  DownloadRequest request;
  DownloadState state;
  uint64_t received;
  uint64_t resumedFrom;  // bytes reaproveitados de um .part na última tentativa
  uint32_t attempts;
  std::string error;
  DownloadJob() : state(kDownloadQueued), received(0), resumedFrom(0), attempts(0) {}
};

struct DownloadOptions {
  uint32_t maxAttempts;
  uint64_t freeSpaceMarginBytes;  // folga exigida além do que falta baixar
  uint32_t rateLimitBytesPerSecond;  // 0 = sem limite (exige IClock)
  DownloadOptions() : maxAttempts(3), freeSpaceMarginBytes(1024 * 1024), rateLimitBytesPerSecond(0) {}
};

class DownloadQueue {
 public:
  DownloadQueue(IFileSystem& fs, IDownloadTransport& transport,
                const DownloadOptions& options = DownloadOptions(), IClock* clock = NULL);
  ~DownloadQueue();

  Status enqueue(const DownloadRequest& request);
  // Avança o download atual em até maxBytes (0 = um buffer). Retorna true se
  // ainda há trabalho.
  bool step(size_t maxBytes);
  void runToCompletion();
  bool cancel(const std::string& destination);

  bool idle() const;
  const std::vector<DownloadJob>& jobs() const { return jobs_; }

 private:
  bool beginAttempt(DownloadJob& job);
  bool prepareResume(DownloadJob& job, uint64_t& offset);
  void finishAttempt(DownloadJob& job);
  void failAttempt(DownloadJob& job, const Status& why, bool keepPartial, bool retry);
  std::string partPath(const DownloadJob& job) const;
  size_t rateBudget(size_t wanted);

  IFileSystem& fs_;
  IDownloadTransport& transport_;
  DownloadOptions options_;
  IClock* clock_;
  std::vector<DownloadJob> jobs_;
  size_t current_;
  bool transportOpen_;
  Sha256 hash_;
  std::vector<char> buffer_;
  uint64_t tokens_;
  uint64_t lastRefillMs_;
  NB_NONCOPYABLE(DownloadQueue);
};

// Transforma um item de catálogo em requisições de download.
Status planInstall(const CatalogItem& item, const std::string& installRoot, char sep,
                   std::vector<DownloadRequest>& out);

// Transporte em memória para testes: URL -> conteúdo, com falhas simuladas.
class MemoryTransport : public IDownloadTransport {
 public:
  MemoryTransport()
      : pos_(0), chunk_(4096), open_(false), supportsRanges_(true), servedBytes_(0),
        servedThisConnection_(0), dropLimit_(0) {}
  void add(const std::string& url, const std::string& data) { content_[url] = data; }
  // As próximas "count" aberturas dessa URL falham com kIoError.
  void failNextOpens(const std::string& url, int count) { openFailures_[url] = count; }
  // Nas próximas "count" conexões, a leitura falha depois de "bytes" bytes
  // (queda de conexão no meio do download).
  void dropAfter(const std::string& url, uint64_t bytes, int count);
  void setChunkSize(size_t n) { chunk_ = n == 0 ? 1 : n; }
  void setSupportsRanges(bool v) { supportsRanges_ = v; }
  uint64_t servedBytes() const { return servedBytes_; }

  virtual Status open(const std::string& url, uint64_t offset, uint64_t& remaining,
                      bool& rangeHonored) NB_OVERRIDE;
  virtual Status read(char* buffer, size_t capacity, size_t& got, bool& finished) NB_OVERRIDE;
  virtual void close() NB_OVERRIDE { open_ = false; }

 private:
  struct Drop {
    uint64_t afterBytes;
    int count;
  };
  std::map<std::string, std::string> content_;
  std::map<std::string, int> openFailures_;
  std::map<std::string, Drop> drops_;
  std::string currentUrl_;
  std::string current_;
  size_t pos_;
  size_t chunk_;
  bool open_;
  bool supportsRanges_;
  uint64_t servedBytes_;
  uint64_t servedThisConnection_;
  uint64_t dropLimit_;  // 0 = sem queda nesta conexão
};

}  // namespace nebula
