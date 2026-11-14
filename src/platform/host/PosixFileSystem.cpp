#include "platform/host/PosixFileSystem.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "core/Path.h"
#include "core/StringUtil.h"

namespace nebula {

namespace {

Status errnoStatus(const std::string& what, const std::string& p) {
  int e = errno;
  ErrorCode code = (e == ENOENT || e == ENOTDIR) ? kNotFound : kIoError;
  return Status(code, what + " " + p + ": " + std::strerror(e));
}

}  // namespace

bool PosixFileSystem::exists(const std::string& p) {
  struct stat st;
  return ::stat(p.c_str(), &st) == 0;
}

bool PosixFileSystem::isDirectory(const std::string& p) {
  struct stat st;
  return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

Status PosixFileSystem::listDirectory(const std::string& p,
                                      std::vector<DirEntry>& out) {
  out.clear();
  DIR* dir = ::opendir(p.c_str());
  if (dir == NULL) return errnoStatus("não foi possível abrir o diretório", p);
  struct dirent* ent = NULL;
  while ((ent = ::readdir(dir)) != NULL) {
    std::string name = ent->d_name;
    if (name == "." || name == "..") continue;
    std::string full = path::join(p, name, '/');
    struct stat st;
    if (::stat(full.c_str(), &st) != 0) continue;  // link quebrado etc.
    out.push_back(DirEntry(name, S_ISDIR(st.st_mode),
                           S_ISDIR(st.st_mode) ? 0 : static_cast<uint64_t>(st.st_size)));
  }
  ::closedir(dir);
  return Status::Ok();
}

Status PosixFileSystem::fileSize(const std::string& p, uint64_t& out) {
  struct stat st;
  if (::stat(p.c_str(), &st) != 0) return errnoStatus("stat falhou para", p);
  if (S_ISDIR(st.st_mode)) return Status(kInvalidArgument, "é um diretório: " + p);
  out = static_cast<uint64_t>(st.st_size);
  return Status::Ok();
}

Status PosixFileSystem::readFile(const std::string& p, std::string& out,
                                 uint64_t maxBytes) {
  uint64_t size = 0;
  Status s = fileSize(p, size);
  if (!s.ok()) return s;
  if (size > maxBytes) return Status(kLimitExceeded, "arquivo maior que o limite: " + p);
  return readFileRange(p, 0, static_cast<size_t>(size), out);
}

Status PosixFileSystem::readFileRange(const std::string& p, uint64_t offset,
                                      size_t length, std::string& out) {
  out.clear();
  std::FILE* f = std::fopen(p.c_str(), "rb");
  if (f == NULL) return errnoStatus("não foi possível abrir", p);
  if (offset > 0 && ::fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0) {
    std::fclose(f);
    return errnoStatus("seek falhou em", p);
  }
  out.resize(length);
  size_t got = length > 0 ? std::fread(&out[0], 1, length, f) : 0;
  bool failed = std::ferror(f) != 0;
  std::fclose(f);
  out.resize(got);
  if (failed) return Status(kIoError, "erro de leitura em " + p);
  return Status::Ok();
}

Status PosixFileSystem::writeFileAtomic(const std::string& p,
                                        const std::string& data) {
  std::string tmp = p + ".tmp";
  std::FILE* f = std::fopen(tmp.c_str(), "wb");
  if (f == NULL) return errnoStatus("não foi possível criar", tmp);
  size_t written = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
  bool ok = written == data.size() && std::fflush(f) == 0;
  if (ok) ok = ::fsync(::fileno(f)) == 0;
  std::fclose(f);
  if (!ok) {
    ::unlink(tmp.c_str());
    return Status(kIoError, "falha ao gravar " + tmp);
  }
  if (std::rename(tmp.c_str(), p.c_str()) != 0) {
    Status s = errnoStatus("falha ao substituir", p);
    ::unlink(tmp.c_str());
    return s;
  }
  return Status::Ok();
}

Status PosixFileSystem::appendToFile(const std::string& p, const char* data,
                                     size_t length) {
  std::FILE* f = std::fopen(p.c_str(), "ab");
  if (f == NULL) return errnoStatus("não foi possível abrir para acréscimo", p);
  size_t written = length > 0 ? std::fwrite(data, 1, length, f) : 0;
  bool ok = written == length && std::fflush(f) == 0;
  std::fclose(f);
  return ok ? Status::Ok() : Status(kIoError, "falha ao gravar em " + p);
}

Status PosixFileSystem::createDirectories(const std::string& p) {
  if (p.empty()) return Status(kInvalidArgument, "caminho vazio");
  if (isDirectory(p)) return Status::Ok();
  std::string parentDir = path::parent(p, '/');
  if (!parentDir.empty() && parentDir != p) {
    Status s = createDirectories(parentDir);
    if (!s.ok()) return s;
  }
  if (::mkdir(p.c_str(), 0755) != 0 && errno != EEXIST) {
    return errnoStatus("não foi possível criar o diretório", p);
  }
  return Status::Ok();
}

Status PosixFileSystem::freeSpace(const std::string& p, uint64_t& bytes) {
  std::string target = p;
  // Sobe até um caminho existente (o destino pode ainda não existir).
  while (!target.empty() && !exists(target)) {
    std::string up = path::parent(target, '/');
    if (up == target) break;
    target = up;
  }
  struct statvfs st;
  if (::statvfs(target.empty() ? "." : target.c_str(), &st) != 0) {
    return errnoStatus("statvfs falhou para", p);
  }
  bytes = static_cast<uint64_t>(st.f_bavail) * static_cast<uint64_t>(st.f_frsize);
  return Status::Ok();
}

Status PosixFileSystem::removeFile(const std::string& p) {
  if (::unlink(p.c_str()) != 0) return errnoStatus("não foi possível remover", p);
  return Status::Ok();
}

Status PosixFileSystem::renameFile(const std::string& from,
                                   const std::string& to) {
  if (std::rename(from.c_str(), to.c_str()) != 0) {
    return errnoStatus("não foi possível renomear", from);
  }
  return Status::Ok();
}

uint64_t SystemClock::monotonicMs() {
  struct timespec ts;
  ::clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000 +
         static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

int64_t SystemClock::unixTimeSeconds() {
  return static_cast<int64_t>(::time(NULL));
}

}  // namespace nebula
