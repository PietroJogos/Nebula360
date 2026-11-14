#include "ui/TextureCache.h"

#include "core/Log.h"
#include "core/StringUtil.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {
const uint64_t kMaxImageFileBytes = 8 * 1024 * 1024;
}

TextureCache::TextureCache(IFileSystem& fs, size_t capacity, size_t decodesPerFrame)
    : fs_(fs), capacity_(capacity == 0 ? 1 : capacity),
      decodesPerFrame_(decodesPerFrame == 0 ? 1 : decodesPerFrame), decodesThisFrame_(0),
      frame_(0) {}

void TextureCache::beginFrame() {
  ++frame_;
  decodesThisFrame_ = 0;
}

size_t TextureCache::failures() const {
  size_t n = 0;
  for (std::map<std::string, Entry>::const_iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    if (it->second.failed) ++n;
  }
  return n;
}

void TextureCache::clear() { entries_.clear(); }

void TextureCache::evictIfNeeded() {
  while (entries_.size() > capacity_) {
    std::map<std::string, Entry>::iterator oldest = entries_.begin();
    for (std::map<std::string, Entry>::iterator it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
    }
    entries_.erase(oldest);
  }
}

const Image* TextureCache::get(const std::string& path, int width, int height) {
  if (path.empty()) return NULL;
  std::string key = path + "@" + str::toString(width) + "x" + str::toString(height);
  std::map<std::string, Entry>::iterator it = entries_.find(key);
  if (it != entries_.end()) {
    it->second.lastUsed = frame_;
    return it->second.failed ? NULL : &it->second.image;
  }
  if (decodesThisFrame_ >= decodesPerFrame_) return NULL;
  ++decodesThisFrame_;

  Entry e;
  e.lastUsed = frame_;
  std::string bytes;
  Status s = fs_.readFile(path, bytes, kMaxImageFileBytes);
  Image decoded;
  if (s.ok()) s = decodeImage(bytes, decoded);
  if (s.ok()) {
    e.image = downscaleToCover(decoded, width, height);
  } else {
    e.failed = true;
    NB_WARN("textures", "capa ignorada: " << path << " (" << s.toString() << ")");
  }
  entries_.insert(std::make_pair(key, e));
  evictIfNeeded();
  // Se todas as entradas foram usadas neste quadro, a remoção LRU pode ter
  // levado a recém-inserida; por isso procura de novo em vez de guardar o
  // ponteiro de antes.
  it = entries_.find(key);
  if (it == entries_.end()) return NULL;
  return it->second.failed ? NULL : &it->second.image;
}

}  // namespace nebula
