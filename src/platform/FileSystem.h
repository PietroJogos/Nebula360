// Abstração de sistema de arquivos.
//
// Os caminhos são sempre no formato nativo da plataforma: no Xbox 360,
// "Hdd1:\Games\Jogo\default.xex"; no PC, "/home/u/jogos/Jogo/default.xex".
// O separador é informado por separator() e usado por core/Path.h.
#pragma once

#include <string>
#include <vector>

#include "core/Status.h"
#include "core/Types.h"

namespace nebula {

struct DirEntry {
  std::string name;
  bool isDirectory;
  uint64_t size;

  DirEntry() : isDirectory(false), size(0) {}
  DirEntry(const std::string& n, bool dir, uint64_t s)
      : name(n), isDirectory(dir), size(s) {}
};

class IFileSystem {
 public:
  virtual ~IFileSystem() {}

  virtual char separator() const = 0;

  virtual bool exists(const std::string& path) = 0;
  virtual bool isDirectory(const std::string& path) = 0;

  // Lista o conteúdo (sem "." e ".."). A ordem não é garantida.
  virtual Status listDirectory(const std::string& path,
                               std::vector<DirEntry>& out) = 0;

  virtual Status fileSize(const std::string& path, uint64_t& out) = 0;

  // Lê o arquivo inteiro; falha com kLimitExceeded se passar de maxBytes.
  virtual Status readFile(const std::string& path, std::string& out,
                          uint64_t maxBytes) = 0;

  // Lê até "length" bytes a partir de "offset" (menos se o arquivo acabar).
  virtual Status readFileRange(const std::string& path, uint64_t offset,
                               size_t length, std::string& out) = 0;

  // Grava em arquivo temporário e substitui o destino, para nunca deixar
  // um arquivo pela metade se a energia cair no meio da gravação.
  virtual Status writeFileAtomic(const std::string& path,
                                 const std::string& data) = 0;

  virtual Status appendToFile(const std::string& path, const char* data,
                              size_t length) = 0;

  virtual Status createDirectories(const std::string& path) = 0;

  // Espaço livre (bytes) no volume que contém "path".
  virtual Status freeSpace(const std::string& path, uint64_t& bytes) = 0;
  virtual Status removeFile(const std::string& path) = 0;
  virtual Status renameFile(const std::string& from, const std::string& to) = 0;
};

}  // namespace nebula
