#include "core/Log.h"

#include <cstdio>

#include "core/StringUtil.h"
#include "platform/FileSystem.h"

namespace nebula {

namespace {

LogLevel gLevel = kLogInfo;
uint64_t (*gTimeSource)() = NULL;

std::vector<ILogSink*>& sinks() {
  static std::vector<ILogSink*> instance;
  return instance;
}

}  // namespace

const char* logLevelName(LogLevel level) {
  switch (level) {
    case kLogDebug: return "DEBUG";
    case kLogInfo: return "INFO";
    case kLogWarning: return "WARN";
    case kLogError: return "ERROR";
    case kLogNone: return "NONE";
  }
  return "?";
}

bool parseLogLevel(const std::string& text, LogLevel& out) {
  std::string t = str::toLowerAscii(str::trim(text));
  if (t == "debug") { out = kLogDebug; return true; }
  if (t == "info") { out = kLogInfo; return true; }
  if (t == "warn" || t == "warning") { out = kLogWarning; return true; }
  if (t == "error") { out = kLogError; return true; }
  if (t == "none") { out = kLogNone; return true; }
  return false;
}

void Log::setLevel(LogLevel level) { gLevel = level; }
LogLevel Log::level() { return gLevel; }

void Log::addSink(ILogSink* sink) {
  if (sink == NULL) return;
  std::vector<ILogSink*>& s = sinks();
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == sink) return;
  }
  s.push_back(sink);
}

void Log::removeSink(ILogSink* sink) {
  std::vector<ILogSink*>& s = sinks();
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == sink) {
      s.erase(s.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
}

void Log::setTimeSource(uint64_t (*now)()) { gTimeSource = now; }

void Log::write(LogLevel level, const char* tag, const std::string& message) {
  if (!enabled(level) || level == kLogNone) return;
  uint64_t t = gTimeSource ? gTimeSource() : 0;
  std::vector<ILogSink*>& s = sinks();
  for (size_t i = 0; i < s.size(); ++i) {
    s[i]->write(level, t, tag ? tag : "", message);
  }
}

std::string Log::formatLine(LogLevel level, uint64_t timeMs, const char* tag,
                            const std::string& message) {
  std::string secs = str::toString(static_cast<uint64_t>(timeMs / 1000));
  std::string ms = str::toString(static_cast<uint64_t>(timeMs % 1000));
  while (ms.size() < 3) ms.insert(ms.begin(), '0');
  while (secs.size() < 5) secs.insert(secs.begin(), ' ');
  std::string name = logLevelName(level);
  while (name.size() < 5) name += ' ';
  return "[" + secs + "." + ms + "] " + name + " " + tag + ": " + message;
}

void MemoryLogSink::write(LogLevel level, uint64_t, const char* tag,
                          const std::string& message) {
  if (entries_.size() >= maxEntries_ && !entries_.empty()) {
    entries_.erase(entries_.begin());
  }
  Entry e;
  e.level = level;
  e.tag = tag;
  e.message = message;
  entries_.push_back(e);
}

bool MemoryLogSink::contains(const std::string& needle) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].message.find(needle) != std::string::npos) return true;
  }
  return false;
}

size_t MemoryLogSink::count(LogLevel level) const {
  size_t n = 0;
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].level == level) ++n;
  }
  return n;
}

void StderrLogSink::write(LogLevel level, uint64_t timeMs, const char* tag,
                          const std::string& message) {
  std::string line = Log::formatLine(level, timeMs, tag, message);
  std::fprintf(stderr, "%s\n", line.c_str());
}

FileLogSink::FileLogSink(IFileSystem& fs, const std::string& path,
                         uint64_t maxBytes)
    : fs_(fs), path_(path), pendingLines_(0), failed_(false) {
  uint64_t size = 0;
  if (fs_.fileSize(path_, size).ok() && size > maxBytes) {
    std::string old = path_ + ".1";
    if (fs_.exists(old)) fs_.removeFile(old);
    fs_.renameFile(path_, old);
  }
}

FileLogSink::~FileLogSink() { flush(); }

void FileLogSink::write(LogLevel level, uint64_t timeMs, const char* tag,
                        const std::string& message) {
  pending_ += Log::formatLine(level, timeMs, tag, message);
  pending_ += "\r\n";
  ++pendingLines_;
  if (level >= kLogWarning || pendingLines_ >= 32) flush();
}

void FileLogSink::flush() {
  if (pending_.empty() || failed_) return;
  Status s = fs_.appendToFile(path_, pending_.data(), pending_.size());
  if (!s.ok()) {
    // Não dá para registrar a falha no próprio log: avisa em stderr uma vez.
    failed_ = true;
    std::fprintf(stderr, "FileLogSink: %s\n", s.toString().c_str());
  }
  pending_.clear();
  pendingLines_ = 0;
}

}  // namespace nebula
