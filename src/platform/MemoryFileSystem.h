// Sistema de arquivos em memória, portável. Usado nos testes para simular
// árvores do Xbox 360 ("Hdd1:\Content\...") em qualquer máquina, e permite
// injetar falhas de E/S.
#pragma once

#include <map>
#include <set>
#include <string>

#include "platform/FileSystem.h"

namespace nebula {

class MemoryFileSystem : public IFileSystem {
 public:
  explicit MemoryFileSystem(char separator = '/');

  // Cria o arquivo (e os diretórios pais).
  void addFile(const std::string& path, const std::string& data);
  void addDirectory(const std::string& path);

  // Faz operações no caminho (ou dentro dele) falharem com kIoError.
  void failPath(const std::string& path);
  void clearFailures();

  bool fileContents(const std::string& path, std::string& out) const;
  size_t fileCount() const { return files_.size(); }

  virtual char separator() const NB_OVERRIDE { return sep_; }
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

 private:
  // Chave normalizada: sem separador final e em minúsculas (FATX não
  // diferencia maiúsculas). O nome original é preservado para listagem.
  std::string key(const std::string& path) const;
  bool failing(const std::string& path) const;
  void ensureParents(const std::string& path);

  struct FileNode {
    std::string name;  // caminho original
    std::string data;
  };

  char sep_;
  std::map<std::string, FileNode> files_;
  std::map<std::string, std::string> dirs_;  // chave -> caminho original
  std::set<std::string> failures_;
};

}  // namespace nebula
