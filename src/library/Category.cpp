#include "library/Category.h"

namespace nebula {

namespace category {

const char* const kXbox360 = "xbox360";
const char* const kArcade = "arcade";
const char* const kHomebrew = "homebrew";
const char* const kEmulator = "emulator";
const char* const kXboxClassic = "xbox_classic";
const char* const kDemo = "demo";
const char* const kAuto = "auto";

const std::vector<std::string>& all() {
  static std::vector<std::string> list;
  if (list.empty()) {
    list.push_back(kXbox360);
    list.push_back(kArcade);
    list.push_back(kHomebrew);
    list.push_back(kEmulator);
    list.push_back(kXboxClassic);
    list.push_back(kDemo);
  }
  return list;
}

bool isValid(const std::string& id) {
  const std::vector<std::string>& list = all();
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i] == id) return true;
  }
  return false;
}

std::string labelKey(const std::string& id) { return "category." + id; }

}  // namespace category

namespace gameformat {

const char* const kXex = "xex";
const char* const kGod = "god";
const char* const kXbla = "xbla";
const char* const kXbe = "xbe";

}  // namespace gameformat

}  // namespace nebula
