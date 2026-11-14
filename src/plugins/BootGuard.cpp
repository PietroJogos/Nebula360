#include "plugins/BootGuard.h"

#include "core/Json.h"
#include "core/Log.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {
const char* const kTag = "bootguard";
}

BootGuard::BootGuard(IFileSystem& fs, const std::string& path, uint32_t threshold)
    : fs_(fs), path_(path), threshold_(threshold == 0 ? 1 : threshold), consecutive_(0),
      previousCrashed_(false) {}

Status BootGuard::write(const char* state) {
  JsonValue o = JsonValue::object();
  o.set("schema", 1);
  o.set("state", state);
  o.set("consecutiveCrashes", consecutive_);
  return fs_.writeFileAtomic(path_, o.serialize(false));
}

Status BootGuard::begin() {
  previousCrashed_ = false;
  consecutive_ = 0;
  if (fs_.exists(path_)) {
    std::string text;
    JsonValue json;
    Status s = fs_.readFile(path_, text, 64 * 1024);
    if (s.ok()) s = parseJson(text, json);
    if (s.ok()) {
      int64_t previous = json.get("consecutiveCrashes").asInt(0);
      if (previous < 0) previous = 0;
      if (json.get("state").asString() == "running") {
        previousCrashed_ = true;
        consecutive_ = static_cast<uint32_t>(previous) + 1;
      }
    } else {
      // Arquivo ilegível: não dá para afirmar que houve queda; recomeça.
      NB_WARN(kTag, "estado de sessão ilegível, ignorado: " << s.toString());
    }
  }
  if (previousCrashed_) {
    NB_WARN(kTag, "a sessão anterior não terminou normalmente (" << consecutive_
                                                                  << " vez(es) seguida(s))");
  }
  Status w = write("running");
  if (!w.ok()) NB_ERROR(kTag, "não foi possível gravar o estado de sessão: " << w.toString());
  return w;
}

Status BootGuard::markCleanExit() {
  consecutive_ = 0;
  return write("clean");
}

Status BootGuard::markRunning() { return write("running"); }

Status BootGuard::resetCrashCount() {
  consecutive_ = 0;
  previousCrashed_ = false;
  return write("running");
}

}  // namespace nebula
