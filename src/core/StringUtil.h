// Utilitários de texto (UTF-8, normalização para busca, conversões).
#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

namespace nebula {
namespace str {

// Decodifica o próximo code point a partir de pos (avança pos). Retorna false
// no fim. Bytes inválidos viram U+FFFD e avançam 1 byte.
bool decodeUtf8(const std::string& s, size_t& pos, uint32_t& codepoint);
void appendUtf8(std::string& out, uint32_t codepoint);
size_t utf8Length(const std::string& s);

// Converte UTF-16 big-endian (formato dos cabeçalhos do Xbox 360) para UTF-8.
// Para no primeiro NUL ou ao fim de byteLength.
std::string utf16beToUtf8(const uint8_t* data, size_t byteLength);

// Normaliza para busca: minúsculas, sem acentos (Latin-1 e Latin Extended-A),
// pontuação vira espaço e espaços repetidos são colapsados.
std::string foldForSearch(const std::string& utf8);

std::string toLowerAscii(const std::string& s);
std::string toUpperAscii(const std::string& s);
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);
std::vector<std::string> splitWhitespace(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);
bool iequalsAscii(const std::string& a, const std::string& b);
bool istartsWithAscii(const std::string& s, const std::string& prefix);
std::string replaceAll(const std::string& s, const std::string& from,
                       const std::string& to);

std::string toString(int64_t value);
std::string toString(uint64_t value);
std::string toString(int value);
std::string toString(uint32_t value);

// "0000ABCD" (8 dígitos, maiúsculos).
std::string toHex32(uint32_t value);
std::string toHexBytes(const uint8_t* data, size_t length);
bool isHexString(const std::string& s);
bool parseHex32(const std::string& s, uint32_t& out);
bool parseInt64(const std::string& s, int64_t& out);

// Data UTC no formato "dd/mm/aaaa" (sem depender de localtime).
std::string formatDate(int64_t unixSeconds);

// Tamanho legível no padrão pt-BR: "1,5 MB".
std::string formatBytes(uint64_t bytes);

}  // namespace str
}  // namespace nebula
