// Sistema de arquivos com a API Win32 (ANSI). Usa só funções presentes tanto
// no Windows quanto no XDK do Xbox 360 (CreateFile, ReadFile, WriteFile,
// FindFirstFile, MoveFile, DeleteFile, CreateDirectory, GetFileAttributes).
//
// No Xbox 360 os caminhos são "Hdd1:\...", "Usb0:\..." etc. Esses nomes de
// dispositivo precisam ser montados antes (ver platform/xbox360).
//
// Verificado apenas por compilação contra os cabeçalhos do MinGW (Windows).
// Não foi executado em um console.
#pragma once

#include "platform/FileSystem.h"

namespace nebula {

class Win32FileSystem : public IFileSystem {
 public:
  virtual char separator() const NB_OVERRIDE { return '\\'; }
  virtual bool exists(const std::string& path) NB_OVERRIDE;
  virtual bool isDirectory(const std::string& path) NB_OVERRIDE;
  virtual Status listDirectory(const std::string& path,
                               std::vector<DirEntry>& out) NB_OVERRIDE;
  virtual Status fileSize(const std::string& path, uint64_t& out) NB_OVERRIDE;
  virtual Status readFile(const std::string& path, std::string& out,
                          uint64_t maxBytes) NB_OVERRIDE;
  virtual Status readFileRange(const std::string& path, uint64_t offset,
                               size_t length, std::string& out) NB_OVERRIDE;
  virtual Status writeFileAtomic(const std::string& path,
                                 const std::string& data) NB_OVERRIDE;
  virtual Status appendToFile(const std::string& path, const char* data,
                              size_t length) NB_OVERRIDE;
  virtual Status createDirectories(const std::string& path) NB_OVERRIDE;
  virtual Status removeFile(const std::string& path) NB_OVERRIDE;
  virtual Status renameFile(const std::string& from,
                            const std::string& to) NB_OVERRIDE;
};

}  // namespace nebula
