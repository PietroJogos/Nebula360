#include "store/DownloadQueue.h"

#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
#include "platform/Clock.h"
#include "platform/FileSystem.h"
#include "store/Catalog.h"
#include "store/StorePolicy.h"

namespace nebula {

namespace {
const char* const kTag = "download";
const size_t kBufferSize = 64 * 1024;
const size_t kNoJob = static_cast<size_t>(-1);
}  // namespace

const char* downloadStateName(DownloadState s) {
  switch (s) {
    case kDownloadQueued: return "queued";
    case kDownloadActive: return "active";
    case kDownloadCompleted: return "completed";
    case kDownloadFailed: return "failed";
    case kDownloadCancelled: return "cancelled";
  }
  return "?";
}

DownloadQueue::DownloadQueue(IFileSystem& fs, IDownloadTransport& transport,
                             const DownloadOptions& options, IClock* clock)
    : fs_(fs), transport_(transport), options_(options), clock_(clock), current_(kNoJob),
      transportOpen_(false), buffer_(kBufferSize), tokens_(0), lastRefillMs_(0) {
  if (options_.maxAttempts == 0) options_.maxAttempts = 1;
  if (options_.rateLimitBytesPerSecond != 0 && clock_ == NULL) {
    NB_WARN(kTag, "limite de velocidade ignorado: nenhum relógio informado");
    options_.rateLimitBytesPerSecond = 0;
  }
  if (clock_ != NULL) lastRefillMs_ = clock_->monotonicMs();
}

DownloadQueue::~DownloadQueue() {
  if (transportOpen_) transport_.close();
}

std::string DownloadQueue::partPath(const DownloadJob& job) const {
  return job.request.destination + ".part";
}

Status DownloadQueue::enqueue(const DownloadRequest& r) {
  if (r.url.empty() || r.destination.empty()) {
    return Status(kInvalidArgument, "download sem URL ou destino");
  }
  if (r.expectedSize == 0) return Status(kInvalidArgument, "download sem tamanho esperado");
  std::string sha = str::toLowerAscii(r.expectedSha256);
  if (sha.size() != 64 || !str::isHexString(sha)) {
    return Status(kInvalidArgument, "download sem SHA-256 válido: " + r.url);
  }
  for (size_t i = 0; i < jobs_.size(); ++i) {
    if (str::iequalsAscii(jobs_[i].request.destination, r.destination) &&
        (jobs_[i].state == kDownloadQueued || jobs_[i].state == kDownloadActive)) {
      return Status(kInvalidArgument, "destino já está na fila: " + r.destination);
    }
  }
  DownloadJob job;
  job.request = r;
  job.request.expectedSha256 = sha;
  jobs_.push_back(job);
  return Status::Ok();
}

bool DownloadQueue::idle() const {
  for (size_t i = 0; i < jobs_.size(); ++i) {
    if (jobs_[i].state == kDownloadQueued || jobs_[i].state == kDownloadActive) return false;
  }
  return true;
}

bool DownloadQueue::cancel(const std::string& destination) {
  for (size_t i = 0; i < jobs_.size(); ++i) {
    DownloadJob& j = jobs_[i];
    if (!str::iequalsAscii(j.request.destination, destination)) continue;
    if (j.state != kDownloadQueued && j.state != kDownloadActive) continue;
    if (i == current_) {
      if (transportOpen_) transport_.close();
      transportOpen_ = false;
      current_ = kNoJob;
    }
    // Cancelar é definitivo: o parcial é apagado.
    if (fs_.exists(partPath(j))) fs_.removeFile(partPath(j));
    j.state = kDownloadCancelled;
    NB_INFO(kTag, "cancelado: " << destination);
    return true;
  }
  return false;
}

bool DownloadQueue::prepareResume(DownloadJob& job, uint64_t& offset) {
  offset = 0;
  hash_.reset();
  std::string part = partPath(job);
  uint64_t size = 0;
  if (!fs_.exists(part) || !fs_.fileSize(part, size).ok()) return false;
  if (size == 0 || size >= job.request.expectedSize) {
    fs_.removeFile(part);
    return false;
  }
  // Reconstrói o SHA-256 do que já foi baixado.
  uint64_t pos = 0;
  while (pos < size) {
    std::string chunk;
    Status s = fs_.readFileRange(part, pos, kBufferSize, chunk);
    if (!s.ok() || chunk.empty()) {
      hash_.reset();
      fs_.removeFile(part);
      return false;
    }
    hash_.update(chunk.data(), chunk.size());
    pos += chunk.size();
  }
  offset = size;
  return true;
}

bool DownloadQueue::beginAttempt(DownloadJob& job) {
  ++job.attempts;
  job.error.clear();
  job.state = kDownloadActive;
  job.resumedFrom = 0;
  std::string dir = path::parent(job.request.destination, fs_.separator());
  if (!dir.empty()) {
    Status s = fs_.createDirectories(dir);
    if (!s.ok()) {
      failAttempt(job, s, true, true);
      return false;
    }
  }
  uint64_t offset = 0;
  bool resuming = prepareResume(job, offset);

  // Espaço livre: o que falta baixar + folga.
  uint64_t free = 0;
  Status fsStatus = fs_.freeSpace(dir.empty() ? job.request.destination : dir, free);
  uint64_t needed = job.request.expectedSize - offset + options_.freeSpaceMarginBytes;
  if (fsStatus.ok() && free < needed) {
    failAttempt(job, Status(kLimitExceeded, "espaço insuficiente: faltam " +
                                                str::formatBytes(needed - free)),
                true, false);  // sem nova tentativa: esperar não libera espaço
    return false;
  }
  if (!fsStatus.ok()) NB_WARN(kTag, "não foi possível consultar o espaço livre: " << fsStatus.toString());

  uint64_t remaining = 0;
  bool honored = false;
  Status s = transport_.open(job.request.url, offset, remaining, honored);
  if (!s.ok()) {
    failAttempt(job, s, true, true);
    return false;
  }
  transportOpen_ = true;
  if (resuming && !honored) {
    // O servidor recomeçou do zero: descarta o parcial.
    fs_.removeFile(partPath(job));
    hash_.reset();
    offset = 0;
    resuming = false;
    NB_INFO(kTag, "servidor não aceitou retomar; recomeçando " << job.request.url);
  }
  job.received = offset;
  job.resumedFrom = offset;
  uint64_t expectedRemaining = job.request.expectedSize - offset;
  if (remaining != 0 && remaining != expectedRemaining) {
    failAttempt(job, Status(kIntegrityError, "servidor informou " + str::toString(remaining) +
                                                 " bytes, esperado " + str::toString(expectedRemaining)),
                false, true);
    return false;
  }
  NB_INFO(kTag, (resuming ? "retomando " : "baixando ") << job.request.url << " (tentativa "
                                                        << job.attempts << ", a partir de " << offset << ")");
  return true;
}

void DownloadQueue::failAttempt(DownloadJob& job, const Status& why, bool keepPartial, bool retry) {
  if (transportOpen_) transport_.close();
  transportOpen_ = false;
  if (!keepPartial) {
    std::string part = partPath(job);
    if (fs_.exists(part)) fs_.removeFile(part);
  }
  job.error = why.toString();
  NB_WARN(kTag, job.request.url << ": " << job.error);
  if (retry && job.attempts < options_.maxAttempts) {
    job.state = kDownloadQueued;  // volta para a fila
  } else {
    job.state = kDownloadFailed;
    NB_ERROR(kTag, "desistindo após " << job.attempts << " tentativa(s): " << job.request.url);
  }
  current_ = kNoJob;
}

void DownloadQueue::finishAttempt(DownloadJob& job) {
  transport_.close();
  transportOpen_ = false;
  if (job.received != job.request.expectedSize) {
    failAttempt(job, Status(kIntegrityError, "tamanho recebido " + str::toString(job.received) +
                                                 " difere do esperado " +
                                                 str::toString(job.request.expectedSize)),
                false, true);
    return;
  }
  std::string digest = hash_.finishHex();
  if (digest != job.request.expectedSha256) {
    failAttempt(job, Status(kIntegrityError, "SHA-256 não confere (recebido " + digest + ")"), false, true);
    return;
  }
  if (fs_.exists(job.request.destination)) fs_.removeFile(job.request.destination);
  Status s = fs_.renameFile(partPath(job), job.request.destination);
  if (!s.ok()) {
    failAttempt(job, s, false, true);
    return;
  }
  job.state = kDownloadCompleted;
  current_ = kNoJob;
  NB_INFO(kTag, "concluído e verificado: " << job.request.destination);
}

size_t DownloadQueue::rateBudget(size_t wanted) {
  if (options_.rateLimitBytesPerSecond == 0 || clock_ == NULL) return wanted;
  uint64_t now = clock_->monotonicMs();
  uint64_t elapsed = now - lastRefillMs_;
  lastRefillMs_ = now;
  uint64_t cap = options_.rateLimitBytesPerSecond;  // rajada máxima: 1 segundo
  tokens_ += elapsed * options_.rateLimitBytesPerSecond / 1000;
  if (tokens_ > cap) tokens_ = cap;
  return tokens_ < wanted ? static_cast<size_t>(tokens_) : wanted;
}

bool DownloadQueue::step(size_t maxBytes) {
  if (current_ == kNoJob) {
    for (size_t i = 0; i < jobs_.size(); ++i) {
      if (jobs_[i].state == kDownloadQueued) {
        current_ = i;
        break;
      }
    }
    if (current_ == kNoJob) return false;
    if (!beginAttempt(jobs_[current_])) return !idle();
  }
  DownloadJob& job = jobs_[current_];
  size_t budget = rateBudget(maxBytes == 0 ? buffer_.size() : maxBytes);
  while (budget > 0) {
    size_t want = budget < buffer_.size() ? budget : buffer_.size();
    size_t got = 0;
    bool finished = false;
    Status s = transport_.read(&buffer_[0], want, got, finished);
    if (!s.ok()) {
      failAttempt(job, s, true, true);  // rede caiu: mantém o parcial para retomar
      return !idle();
    }
    if (got > 0) {
      if (job.received + got > job.request.expectedSize) {
        failAttempt(job, Status(kIntegrityError, "servidor enviou mais dados que o esperado"), false, true);
        return !idle();
      }
      s = fs_.appendToFile(partPath(job), &buffer_[0], got);
      if (!s.ok()) {
        failAttempt(job, s, true, true);
        return !idle();
      }
      hash_.update(&buffer_[0], got);
      job.received += got;
      budget -= got;
      if (tokens_ >= got) tokens_ -= got;
    }
    if (finished) {
      finishAttempt(job);
      return !idle();
    }
    if (got == 0) break;  // sem dados agora; continua no próximo quadro
  }
  return !idle();
}

void DownloadQueue::runToCompletion() {
  size_t guard = 0;
  while (step(0) && guard < 1000000) ++guard;
}

Status planInstall(const CatalogItem& item, const std::string& installRoot, char sep,
                   std::vector<DownloadRequest>& out) {
  out.clear();
  Status s = store::evaluateItemPolicy(item);
  if (!s.ok()) return s;
  for (size_t i = 0; i < item.files.size(); ++i) {
    const CatalogFile& f = item.files[i];
    DownloadRequest r;
    r.itemId = item.id;
    r.url = f.url;
    r.destination = store::buildInstallPath(installRoot, item.id, f.installPath, sep);
    r.expectedSize = f.size;
    r.expectedSha256 = f.sha256;
    out.push_back(r);
  }
  return Status::Ok();
}

void MemoryTransport::dropAfter(const std::string& url, uint64_t bytes, int count) {
  Drop d;
  d.afterBytes = bytes;
  d.count = count;
  drops_[url] = d;
}

Status MemoryTransport::open(const std::string& url, uint64_t offset, uint64_t& remaining,
                             bool& rangeHonored) {
  std::map<std::string, int>::iterator f = openFailures_.find(url);
  if (f != openFailures_.end() && f->second > 0) {
    --f->second;
    return Status(kIoError, "falha de rede simulada: " + url);
  }
  std::map<std::string, std::string>::const_iterator it = content_.find(url);
  if (it == content_.end()) return Status(kNotFound, "URL inexistente: " + url);
  currentUrl_ = url;
  current_ = it->second;
  rangeHonored = supportsRanges_ && offset > 0 && offset < current_.size();
  pos_ = rangeHonored ? static_cast<size_t>(offset) : 0;
  remaining = current_.size() - pos_;
  servedThisConnection_ = 0;
  dropLimit_ = 0;
  std::map<std::string, Drop>::iterator d = drops_.find(url);
  if (d != drops_.end() && d->second.count > 0) {
    --d->second.count;
    dropLimit_ = d->second.afterBytes;
  }
  open_ = true;
  return Status::Ok();
}

Status MemoryTransport::read(char* buffer, size_t capacity, size_t& got, bool& finished) {
  got = 0;
  finished = false;
  if (!open_) return Status(kInternal, "transporte não está aberto");
  if (dropLimit_ != 0 && servedThisConnection_ >= dropLimit_) {
    open_ = false;
    return Status(kIoError, "conexão perdida (simulada): " + currentUrl_);
  }
  size_t n = current_.size() - pos_;
  if (n > capacity) n = capacity;
  if (n > chunk_) n = chunk_;
  if (dropLimit_ != 0 && servedThisConnection_ + n > dropLimit_) {
    n = static_cast<size_t>(dropLimit_ - servedThisConnection_);
  }
  for (size_t i = 0; i < n; ++i) buffer[i] = current_[pos_ + i];
  pos_ += n;
  got = n;
  servedBytes_ += n;
  servedThisConnection_ += n;
  finished = pos_ >= current_.size();
  return Status::Ok();
}

}  // namespace nebula
