// Categorias da biblioteca. Identificadores em texto (estáveis no JSON).
#pragma once

#include <string>
#include <vector>

namespace nebula {

namespace category {

extern const char* const kXbox360;      // "xbox360"
extern const char* const kArcade;       // "arcade"  (Xbox Live Arcade)
extern const char* const kHomebrew;     // "homebrew"
extern const char* const kEmulator;     // "emulator"
extern const char* const kXboxClassic;  // "xbox_classic"
extern const char* const kDemo;         // "demo"
extern const char* const kAuto;         // "auto" (só em pastas de varredura)

// Ordem de exibição das abas.
const std::vector<std::string>& all();
bool isValid(const std::string& id);
// Chave de tradução: "category.<id>".
std::string labelKey(const std::string& id);

}  // namespace category

namespace gameformat {

extern const char* const kXex;   // pasta com default.xex (extraído/homebrew)
extern const char* const kGod;   // contêiner Games on Demand
extern const char* const kXbla;  // contêiner Xbox Live Arcade
extern const char* const kXbe;   // jogo do Xbox clássico (default.xbe)

}  // namespace gameformat

}  // namespace nebula
