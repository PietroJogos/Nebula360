#include "platform/MemoryFileSystem.h"

#include "core/Path.h"
#include "core/StringUtil.h"

namespace nebula {

MemoryFileSystem::MemoryFileSystem(char separator) : sep_(separator) {}

std::string MemoryFileSystem::key(const std::string& p) const {
  return str::toLowerAscii(path::stripTrailingSeparator(p, sep_));
}

bool MemoryFileSystem::failing(const std::string& p) const {
  for (std::set<std::string>::const_iterator it = failures_.begin();
       it != failures_.end(); ++it) {
    if (path::isWithin(p, *it, sep_)) return true;
  }
  return false;
}

void MemoryFileSystem::ensureParents(const std::string& p) {
  std::string cur = path::parent(p, sep_);
  while (!cur.empty()) {
    std::string k = key(cur);
    if (dirs_.find(k) != dirs_.end()) break;
    dirs_[k] = path::stripTrailingSeparator(cur, sep_);
    std::string next = path::parent(cur, sep_);
    if (next == cur) break;
    cur = next;
  }
}

void MemoryFileSystem::addFile(const std::string& p, const std::string& data) {
  ensureParents(p);
  FileNode node;
  node.name = p;
  node.data = data;
  files_[key(p)] = node;
}

void MemoryFileSystem::addDirectory(const std::string& p) {
  ensureParents(p);
  dirs_[key(p)] = path::stripTrailingSeparator(p, sep_);
}

void MemoryFileSystem::failPath(const std::string& p) { failures_.insert(p); }
void MemoryFileSystem::clearFailures() { failures_.clear(); }

bool MemoryFileSystem::fileContents(const std::string& p, std::string& out) const {
  std::map<std::string, FileNode>::const_iterator it = files_.find(key(p));
  if (it == files_.end()) return false;
  out = it->second.data;
  return true;
}

bool MemoryFileSystem::exists(const std::string& p) {
  if (failing(p)) return false;
  std::string k = key(p);
  return files_.find(k) != files_.end() || dirs_.find(k) != dirs_.end();
}

bool MemoryFileSystem::isDirectory(const std::string& p) {
  if (failing(p)) return false;
  return dirs_.find(key(p)) != dirs_.end();
}

Status MemoryFileSystem::listDirectory(const std::string& p,
                                       std::vector<DirEntry>& out) {
  out.clear();
  if (failing(p)) return Status(kIoError, "falha simulada ao listar " + p);
  std::string k = key(p);
  if (dirs_.find(k) == dirs_.end()) {
    return Status(kNotFound, "diretório não encontrado: " + p);
  }
  for (std::map<std::string, std::string>::const_iterator it = dirs_.begin();
       it != dirs_.end(); ++it) {
    if (it->first != k && key(path::parent(it->second, sep_)) == k) {
      out.push_back(DirEntry(path::fileName(it->second, sep_), true, 0));
    }
  }
  for (std::map<std::string, FileNode>::const_iterator it = files_.begin();
       it != files_.end(); ++it) {
    if (key(path::parent(it->second.name, sep_)) == k) {
      out.push_back(DirEntry(path::fileName(it->second.name, sep_), false,
                             static_cast<uint64_t>(it->second.data.size())));
    }
  }
  return Status::Ok();
}

Status MemoryFileSystem::fileSize(const std::string& p, uint64_t& out) {
  if (failing(p)) return Status(kIoError, "falha simulada: " + p);
  std::map<std::string, FileNode>::const_iterator it = files_.find(key(p));
  if (it == files_.end()) return Status(kNotFound, "arquivo não encontrado: " + p);
  out = it->second.data.size();
  return Status::Ok();
}

Status MemoryFileSystem::readFile(const std::string& p, std::string& out,
                                  uint64_t maxBytes) {
  if (failing(p)) return Status(kIoError, "falha simulada ao ler " + p);
  std::map<std::string, FileNode>::const_iterator it = files_.find(key(p));
  if (it == files_.end()) return Status(kNotFound, "arquivo não encontrado: " + p);
  if (it->second.data.size() > maxBytes) {
    return Status(kLimitExceeded, "arquivo maior que o limite: " + p);
  }
  out = it->second.data;
  return Status::Ok();
}

Status MemoryFileSystem::readFileRange(const std::string& p, uint64_t offset,
                                       size_t length, std::string& out) {
  if (failing(p)) return Status(kIoError, "falha simulada ao ler " + p);
  std::map<std::string, FileNode>::const_iterator it = files_.find(key(p));
  if (it == files_.end()) return Status(kNotFound, "arquivo não encontrado: " + p);
  const std::string& d = it->second.data;
  if (offset >= d.size()) {
    out.clear();
    return Status::Ok();
  }
  out = d.substr(static_cast<size_t>(offset), length);
  return Status::Ok();
}

Status MemoryFileSystem::writeFileAtomic(const std::string& p,
                                         const std::string& data) {
  if (failing(p)) return Status(kIoError, "falha simulada ao gravar " + p);
  std::string parentDir = path::parent(p, sep_);
  if (!parentDir.empty() && dirs_.find(key(parentDir)) == dirs_.end()) {
    return Status(kNotFound, "diretório inexistente: " + parentDir);
  }
  FileNode node;
  node.name = p;
  node.data = data;
  files_[key(p)] = node;
  return Status::Ok();
}

Status MemoryFileSystem::appendToFile(const std::string& p, const char* data,
                                      size_t length) {
  if (failing(p)) return Status(kIoError, "falha simulada ao gravar " + p);
  std::string parentDir = path::parent(p, sep_);
  if (!parentDir.empty() && dirs_.find(key(parentDir)) == dirs_.end()) {
    return Status(kNotFound, "diretório inexistente: " + parentDir);
  }
  std::string k = key(p);
  if (files_.find(k) == files_.end()) {
    FileNode node;
    node.name = p;
    files_[k] = node;
  }
  files_[k].data.append(data, length);
  return Status::Ok();
}

Status MemoryFileSystem::createDirectories(const std::string& p) {
  if (failing(p)) return Status(kIoError, "falha simulada ao criar " + p);
  if (files_.find(key(p)) != files_.end()) {
    return Status(kIoError, "já existe um arquivo com esse nome: " + p);
  }
  addDirectory(p);
  return Status::Ok();
}

Status MemoryFileSystem::removeFile(const std::string& p) {
  if (failing(p)) return Status(kIoError, "falha simulada ao remover " + p);
  if (files_.erase(key(p)) == 0) return Status(kNotFound, "arquivo não encontrado: " + p);
  return Status::Ok();
}

Status MemoryFileSystem::renameFile(const std::string& from,
                                    const std::string& to) {
  if (failing(from) || failing(to)) {
    return Status(kIoError, "falha simulada ao renomear " + from);
  }
  std::map<std::string, FileNode>::iterator it = files_.find(key(from));
  if (it == files_.end()) return Status(kNotFound, "arquivo não encontrado: " + from);
  FileNode node = it->second;
  files_.erase(it);
  node.name = to;
  ensureParents(to);
  files_[key(to)] = node;
  return Status::Ok();
}

}  // namespace nebula
