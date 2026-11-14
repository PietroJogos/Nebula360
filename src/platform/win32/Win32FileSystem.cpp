#include "platform/win32/Win32FileSystem.h"

#if defined(_XBOX)
#include <xtl.h>
#else
#include <windows.h>
#endif

#include "core/Path.h"
#include "core/StringUtil.h"

namespace nebula {

namespace {

Status lastError(const std::string& what, const std::string& p) {
  DWORD e = GetLastError();
  ErrorCode code = (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) ? kNotFound : kIoError;
  return Status(code, what + " " + p + " (erro " + str::toString(static_cast<uint32_t>(e)) + ")");
}

// Fecha o HANDLE ao sair do escopo.
class FileHandle {
 public:
  explicit FileHandle(HANDLE h) : h_(h) {}
  ~FileHandle() {
    if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
  }
  HANDLE get() const { return h_; }
  bool valid() const { return h_ != INVALID_HANDLE_VALUE; }

 private:
  HANDLE h_;
  FileHandle(const FileHandle&);
  FileHandle& operator=(const FileHandle&);
};

bool writeAll(HANDLE h, const char* data, size_t length) {
  while (length > 0) {
    DWORD chunk = length > 0x100000 ? 0x100000 : static_cast<DWORD>(length);
    DWORD written = 0;
    if (!WriteFile(h, data, chunk, &written, NULL) || written != chunk) return false;
    data += chunk;
    length -= chunk;
  }
  return true;
}

}  // namespace

bool Win32FileSystem::exists(const std::string& p) {
  return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Win32FileSystem::isDirectory(const std::string& p) {
  DWORD a = GetFileAttributesA(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

Status Win32FileSystem::listDirectory(const std::string& p, std::vector<DirEntry>& out) {
  out.clear();
  std::string pattern = path::join(p, "*", '\\');
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) return Status::Ok();  // pasta vazia
    return lastError("não foi possível listar", p);
  }
  do {
    std::string name = fd.cFileName;
    if (name == "." || name == "..") continue;
    bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    uint64_t size = dir ? 0 : ((static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow);
    out.push_back(DirEntry(name, dir, size));
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return Status::Ok();
}

Status Win32FileSystem::fileSize(const std::string& p, uint64_t& out) {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &data)) {
    return lastError("não foi possível ler atributos de", p);
  }
  if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    return Status(kInvalidArgument, "é um diretório: " + p);
  }
  out = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return Status::Ok();
}

Status Win32FileSystem::readFile(const std::string& p, std::string& out, uint64_t maxBytes) {
  uint64_t size = 0;
  Status s = fileSize(p, size);
  if (!s.ok()) return s;
  if (size > maxBytes) return Status(kLimitExceeded, "arquivo maior que o limite: " + p);
  return readFileRange(p, 0, static_cast<size_t>(size), out);
}

Status Win32FileSystem::readFileRange(const std::string& p, uint64_t offset, size_t length,
                                      std::string& out) {
  out.clear();
  FileHandle f(CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL));
  if (!f.valid()) return lastError("não foi possível abrir", p);
  LONG high = static_cast<LONG>(offset >> 32);
  DWORD low = SetFilePointer(f.get(), static_cast<LONG>(offset & 0xFFFFFFFFu), &high, FILE_BEGIN);
  if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
    return lastError("seek falhou em", p);
  }
  out.resize(length);
  size_t total = 0;
  while (total < length) {
    DWORD want = (length - total) > 0x100000 ? 0x100000 : static_cast<DWORD>(length - total);
    DWORD got = 0;
    if (!ReadFile(f.get(), &out[total], want, &got, NULL)) {
      out.clear();
      return lastError("erro de leitura em", p);
    }
    if (got == 0) break;  // fim do arquivo
    total += got;
  }
  out.resize(total);
  return Status::Ok();
}

Status Win32FileSystem::writeFileAtomic(const std::string& p, const std::string& data) {
  // FATX não tem rename atômico por cima de um arquivo existente: grava em
  // .tmp, move o antigo para .bak, coloca o novo no lugar e só então apaga
  // o .bak. Uma falha no meio deixa o .tmp ou o .bak recuperáveis.
  std::string tmp = p + ".tmp";
  std::string bak = p + ".bak";
  {
    FileHandle f(CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL));
    if (!f.valid()) return lastError("não foi possível criar", tmp);
    if (!writeAll(f.get(), data.data(), data.size()) || !FlushFileBuffers(f.get())) {
      Status s = lastError("falha ao gravar", tmp);
      return s;
    }
  }
  bool hadOld = exists(p);
  if (hadOld) {
    DeleteFileA(bak.c_str());
    if (!MoveFileA(p.c_str(), bak.c_str())) return lastError("falha ao preparar substituição de", p);
  }
  if (!MoveFileA(tmp.c_str(), p.c_str())) {
    Status s = lastError("falha ao substituir", p);
    if (hadOld) MoveFileA(bak.c_str(), p.c_str());
    return s;
  }
  if (hadOld) DeleteFileA(bak.c_str());
  return Status::Ok();
}

Status Win32FileSystem::appendToFile(const std::string& p, const char* data, size_t length) {
  FileHandle f(CreateFileA(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL));
  if (!f.valid()) return lastError("não foi possível abrir para acréscimo", p);
  LONG high = 0;
  if (SetFilePointer(f.get(), 0, &high, FILE_END) == INVALID_SET_FILE_POINTER &&
      GetLastError() != NO_ERROR) {
    return lastError("seek falhou em", p);
  }
  if (!writeAll(f.get(), data, length)) return lastError("falha ao gravar em", p);
  return Status::Ok();
}

Status Win32FileSystem::createDirectories(const std::string& p) {
  std::string clean = path::stripTrailingSeparator(p, '\\');
  if (clean.empty()) return Status(kInvalidArgument, "caminho vazio");
  if (isDirectory(clean)) return Status::Ok();
  // Raiz de dispositivo ("Hdd1:" / "C:") não se cria.
  if (clean[clean.size() - 1] == ':') return Status(kNotFound, "dispositivo não montado: " + clean);
  std::string parentDir = path::parent(clean, '\\');
  if (!parentDir.empty() && parentDir != clean) {
    Status s = createDirectories(parentDir);
    if (!s.ok()) return s;
  }
  if (!CreateDirectoryA(clean.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
    return lastError("não foi possível criar o diretório", clean);
  }
  return Status::Ok();
}

Status Win32FileSystem::removeFile(const std::string& p) {
  if (!DeleteFileA(p.c_str())) return lastError("não foi possível remover", p);
  return Status::Ok();
}

Status Win32FileSystem::renameFile(const std::string& from, const std::string& to) {
  if (!MoveFileA(from.c_str(), to.c_str())) return lastError("não foi possível renomear", from);
  return Status::Ok();
}

}  // namespace nebula
