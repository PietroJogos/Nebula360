// Fila de downloads incremental (sem threads), com verificação de integridade.
//
// Fluxo de cada arquivo: baixa para "<destino>.part" -> confere tamanho e
// SHA-256 -> só então renomeia para o destino. Em falha, apaga o parcial e
// tenta de novo até maxAttempts. Nunca grava além do tamanho esperado.
//
// O transporte é abstrato. Nesta fase existe só o MemoryTransport (testes);
// HTTP no Xbox 360 (XHTTP/sockets) e no PC ficam para a fase da loja.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Sha256.h"
#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

class IFileSystem;
struct CatalogItem;

class IDownloadTransport {
 public:
  virtual ~IDownloadTransport() {}
  // contentLength = 0 se desconhecido.
  virtual Status open(const std::string& url, uint64_t& contentLength) = 0;
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
  uint32_t attempts;
  std::string error;
  DownloadJob() : state(kDownloadQueued), received(0), attempts(0) {}
};

class DownloadQueue {
 public:
  DownloadQueue(IFileSystem& fs, IDownloadTransport& transport, uint32_t maxAttempts = 3);
  ~DownloadQueue();

  Status enqueue(const DownloadRequest& request);
  // Avança o download atual em até maxBytes. Retorna true se ainda há trabalho.
  bool step(size_t maxBytes);
  void runToCompletion();
  bool cancel(const std::string& destination);

  bool idle() const;
  const std::vector<DownloadJob>& jobs() const { return jobs_; }

 private:
  bool beginAttempt(DownloadJob& job);
  void finishAttempt(DownloadJob& job);
  void failAttempt(DownloadJob& job, const Status& why);
  std::string partPath(const DownloadJob& job) const;

  IFileSystem& fs_;
  IDownloadTransport& transport_;
  uint32_t maxAttempts_;
  std::vector<DownloadJob> jobs_;
  size_t current_;
  bool transportOpen_;
  Sha256 hash_;
  std::vector<char> buffer_;
  NB_NONCOPYABLE(DownloadQueue);
};

// Transforma um item de catálogo em requisições de download.
Status planInstall(const CatalogItem& item, const std::string& installRoot, char sep,
                   std::vector<DownloadRequest>& out);

// Transporte em memória para testes e demonstração: URL -> conteúdo.
class MemoryTransport : public IDownloadTransport {
 public:
  MemoryTransport() : pos_(0), chunk_(4096), open_(false) {}
  void add(const std::string& url, const std::string& data) { content_[url] = data; }
  // As próximas "count" aberturas dessa URL falham com kIoError.
  void failNextOpens(const std::string& url, int count) { failures_[url] = count; }
  void setChunkSize(size_t n) { chunk_ = n == 0 ? 1 : n; }

  virtual Status open(const std::string& url, uint64_t& contentLength) NB_OVERRIDE;
  virtual Status read(char* buffer, size_t capacity, size_t& got, bool& finished) NB_OVERRIDE;
  virtual void close() NB_OVERRIDE { open_ = false; }

 private:
  std::map<std::string, std::string> content_;
  std::map<std::string, int> failures_;
  std::string current_;
  size_t pos_;
  size_t chunk_;
  bool open_;
};

}  // namespace nebula
