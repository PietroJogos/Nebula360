// Cache LRU de capas decodificadas e reduzidas para o tamanho de exibição.
//
// Para manter a interface fluida, decodifica no máximo N imagens por quadro;
// enquanto isso get() devolve NULL e a tela desenha o marcador de lugar.
// Falhas também ficam em cache (não tenta decodificar a cada quadro).
#pragma once

#include <map>
#include <string>

#include "ui/Image.h"

namespace nebula {

class IFileSystem;

class TextureCache {
 public:
  TextureCache(IFileSystem& fs, size_t capacity = 96, size_t decodesPerFrame = 2);

  void beginFrame();
  const Image* get(const std::string& path, int width, int height);
  void clear();
  size_t size() const { return entries_.size(); }
  size_t failures() const;

 private:
  struct Entry {
    Image image;
    bool failed;
    uint64_t lastUsed;
    Entry() : failed(false), lastUsed(0) {}
  };
  void evictIfNeeded();

  IFileSystem& fs_;
  size_t capacity_;
  size_t decodesPerFrame_;
  size_t decodesThisFrame_;
  uint64_t frame_;
  std::map<std::string, Entry> entries_;
  NB_NONCOPYABLE(TextureCache);
};

}  // namespace nebula
