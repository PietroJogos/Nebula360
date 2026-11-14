#include "store/DownloadQueue.h"

#include "core/Log.h"
#include "core/Path.h"
#include "core/StringUtil.h"
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
                             uint32_t maxAttempts)
    : fs_(fs), transport_(transport), maxAttempts_(maxAttempts == 0 ? 1 : maxAttempts),
      current_(kNoJob), transportOpen_(false), buffer_(kBufferSize) {}

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
      if (fs_.exists(partPath(j))) fs_.removeFile(partPath(j));
    }
    j.state = kDownloadCancelled;
    NB_INFO(kTag, "cancelado: " << destination);
    return true;
  }
  return false;
}

bool DownloadQueue::beginAttempt(DownloadJob& job) {
  ++job.attempts;
  job.received = 0;
  job.error.clear();
  job.state = kDownloadActive;
  hash_.reset();
  std::string part = partPath(job);
  if (fs_.exists(part)) fs_.removeFile(part);
  std::string dir = path::parent(job.request.destination, fs_.separator());
  if (!dir.empty()) {
    Status s = fs_.createDirectories(dir);
    if (!s.ok()) {
      failAttempt(job, s);
      return false;
    }
  }
  uint64_t length = 0;
  Status s = transport_.open(job.request.url, length);
  if (!s.ok()) {
    failAttempt(job, s);
    return false;
  }
  transportOpen_ = true;
  if (length != 0 && length != job.request.expectedSize) {
    failAttempt(job, Status(kIntegrityError, "servidor informou tamanho " +
                                                 str::toString(length) + ", esperado " +
                                                 str::toString(job.request.expectedSize)));
    return false;
  }
  NB_INFO(kTag, "baixando " << job.request.url << " (tentativa " << job.attempts << ")");
  return true;
}

void DownloadQueue::failAttempt(DownloadJob& job, const Status& why) {
  if (transportOpen_) transport_.close();
  transportOpen_ = false;
  std::string part = partPath(job);
  if (fs_.exists(part)) fs_.removeFile(part);
  job.error = why.toString();
  NB_WARN(kTag, job.request.url << ": " << job.error);
  if (job.attempts < maxAttempts_) {
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
                                                 str::toString(job.request.expectedSize)));
    return;
  }
  std::string digest = hash_.finishHex();
  if (digest != job.request.expectedSha256) {
    failAttempt(job, Status(kIntegrityError, "SHA-256 não confere (recebido " + digest + ")"));
    return;
  }
  if (fs_.exists(job.request.destination)) fs_.removeFile(job.request.destination);
  Status s = fs_.renameFile(partPath(job), job.request.destination);
  if (!s.ok()) {
    failAttempt(job, s);
    return;
  }
  job.state = kDownloadCompleted;
  current_ = kNoJob;
  NB_INFO(kTag, "concluído e verificado: " << job.request.destination);
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
  size_t budget = maxBytes == 0 ? buffer_.size() : maxBytes;
  while (budget > 0) {
    size_t want = budget < buffer_.size() ? budget : buffer_.size();
    size_t got = 0;
    bool finished = false;
    Status s = transport_.read(&buffer_[0], want, got, finished);
    if (!s.ok()) {
      failAttempt(job, s);
      return !idle();
    }
    if (got > 0) {
      if (job.received + got > job.request.expectedSize) {
        failAttempt(job, Status(kIntegrityError, "servidor enviou mais dados que o esperado"));
        return !idle();
      }
      s = fs_.appendToFile(partPath(job), &buffer_[0], got);
      if (!s.ok()) {
        failAttempt(job, s);
        return !idle();
      }
      hash_.update(&buffer_[0], got);
      job.received += got;
      budget -= got;
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

Status MemoryTransport::open(const std::string& url, uint64_t& contentLength) {
  std::map<std::string, int>::iterator f = failures_.find(url);
  if (f != failures_.end() && f->second > 0) {
    --f->second;
    return Status(kIoError, "falha de rede simulada: " + url);
  }
  std::map<std::string, std::string>::const_iterator it = content_.find(url);
  if (it == content_.end()) return Status(kNotFound, "URL inexistente: " + url);
  current_ = it->second;
  pos_ = 0;
  open_ = true;
  contentLength = current_.size();
  return Status::Ok();
}

Status MemoryTransport::read(char* buffer, size_t capacity, size_t& got, bool& finished) {
  got = 0;
  finished = false;
  if (!open_) return Status(kInternal, "transporte não está aberto");
  size_t n = current_.size() - pos_;
  if (n > capacity) n = capacity;
  if (n > chunk_) n = chunk_;
  for (size_t i = 0; i < n; ++i) buffer[i] = current_[pos_ + i];
  pos_ += n;
  got = n;
  finished = pos_ >= current_.size();
  return Status::Ok();
}

}  // namespace nebula
