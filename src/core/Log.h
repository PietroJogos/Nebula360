// Sistema de logs com níveis e destinos (sinks) plugáveis.
//
// Não é thread-safe: o Nébula roda num laço único (scanner e downloads são
// incrementais, avançados a cada quadro), então isso é suficiente.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "core/Types.h"

namespace nebula {

class IFileSystem;

enum LogLevel {
  kLogDebug = 0,
  kLogInfo = 1,
  kLogWarning = 2,
  kLogError = 3,
  kLogNone = 4
};

const char* logLevelName(LogLevel level);
bool parseLogLevel(const std::string& text, LogLevel& out);

class ILogSink {
 public:
  virtual ~ILogSink() {}
  virtual void write(LogLevel level, uint64_t timeMs, const char* tag,
                     const std::string& message) = 0;
};

class Log {
 public:
  static void setLevel(LogLevel level);
  static LogLevel level();
  static bool enabled(LogLevel level) { return level >= Log::level(); }

  // Os sinks não pertencem ao Log; quem registra deve remover antes de destruir.
  static void addSink(ILogSink* sink);
  static void removeSink(ILogSink* sink);

  // Fonte de tempo (ms) usada no carimbo de cada linha. Padrão: sempre 0.
  static void setTimeSource(uint64_t (*now)());

  static void write(LogLevel level, const char* tag, const std::string& message);

  // Formata uma linha: "[   1.234] WARN  tag: mensagem".
  static std::string formatLine(LogLevel level, uint64_t timeMs,
                                const char* tag, const std::string& message);
};

// Guarda as mensagens em memória (testes e tela de diagnóstico).
class MemoryLogSink : public ILogSink {
 public:
  struct Entry {
    LogLevel level;
    std::string tag;
    std::string message;
  };

  explicit MemoryLogSink(size_t maxEntries = 512) : maxEntries_(maxEntries) {}
  virtual void write(LogLevel level, uint64_t timeMs, const char* tag,
                     const std::string& message) NB_OVERRIDE;

  const std::vector<Entry>& entries() const { return entries_; }
  bool contains(const std::string& needle) const;
  size_t count(LogLevel level) const;
  void clear() { entries_.clear(); }

 private:
  size_t maxEntries_;
  std::vector<Entry> entries_;
};

// Escreve em stderr.
class StderrLogSink : public ILogSink {
 public:
  virtual void write(LogLevel level, uint64_t timeMs, const char* tag,
                     const std::string& message) NB_OVERRIDE;
};

// Escreve em arquivo através do IFileSystem, com rotação simples por tamanho
// (o arquivo atual vira ".1" ao abrir se passar de maxBytes). Avisos e erros
// são gravados imediatamente; o resto é agrupado para reduzir escritas.
class FileLogSink : public ILogSink {
 public:
  FileLogSink(IFileSystem& fs, const std::string& path, uint64_t maxBytes);
  virtual ~FileLogSink();
  virtual void write(LogLevel level, uint64_t timeMs, const char* tag,
                     const std::string& message) NB_OVERRIDE;
  void flush();

 private:
  IFileSystem& fs_;
  std::string path_;
  std::string pending_;
  size_t pendingLines_;
  bool failed_;
  NB_NONCOPYABLE(FileLogSink);
};

}  // namespace nebula

#define NB_LOG(lvl, tag, expr)                                   \
  do {                                                           \
    if (::nebula::Log::enabled(lvl)) {                           \
      std::ostringstream nb_log_os_;                             \
      nb_log_os_ << expr;                                        \
      ::nebula::Log::write(lvl, tag, nb_log_os_.str());          \
    }                                                            \
  } while (0)

#define NB_DEBUG(tag, expr) NB_LOG(::nebula::kLogDebug, tag, expr)
#define NB_INFO(tag, expr) NB_LOG(::nebula::kLogInfo, tag, expr)
#define NB_WARN(tag, expr) NB_LOG(::nebula::kLogWarning, tag, expr)
#define NB_ERROR(tag, expr) NB_LOG(::nebula::kLogError, tag, expr)
