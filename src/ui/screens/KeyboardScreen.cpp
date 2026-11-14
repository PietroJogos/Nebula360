// Teclado virtual navegável pelo controle (pesquisa e digitação de caminhos).
//
// A = tecla; X = apagar; Y = espaço; LB = maiúsculas; Start = concluir;
// B = cancelar (devolve o texto original).
#include "core/StringUtil.h"
#include "theme/Theme.h"
#include "ui/AppController.h"
#include "ui/Font.h"
#include "ui/Strings.h"
#include "ui/Widgets.h"
#include "ui/screens/Screens.h"

namespace nebula {

namespace {

enum KeyKind { kKeyChar, kKeyShift, kKeySpace, kKeyBackspace, kKeyClear, kKeyDone };

struct Key {
  std::string label;
  KeyKind kind;
  int units;  // largura em unidades (cada linha soma 10)
  Key(const std::string& l, KeyKind k, int u) : label(l), kind(k), units(u) {}
};

const size_t kMaxSearchLength = 64;
const size_t kMaxPathLength = 120;

class KeyboardScreen : public Screen {
 public:
  KeyboardScreen(IAppController& app, int requestId, const std::string& title,
                 const std::string& initial, KeyboardMode mode)
      : Screen(app), requestId_(requestId), title_(title), text_(initial), original_(initial),
        mode_(mode), row_(1), col_(0), shift_(false) {
    const char* const rows[] = {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm._:",
                                "\\/()!&'+,#"};
    for (size_t r = 0; r < 5; ++r) {
      std::vector<Key> keys;
      for (const char* p = rows[r]; *p; ++p) keys.push_back(Key(std::string(1, *p), kKeyChar, 1));
      rows_.push_back(keys);
    }
    std::vector<Key> last;
    last.push_back(Key("shift", kKeyShift, 2));
    last.push_back(Key("space", kKeySpace, 3));
    last.push_back(Key("backspace", kKeyBackspace, 2));
    last.push_back(Key("clear", kKeyClear, 1));
    last.push_back(Key("done", kKeyDone, 2));
    rows_.push_back(last);
  }

  virtual const char* name() const NB_OVERRIDE { return "keyboard"; }
  virtual bool isOverlay() const NB_OVERRIDE { return true; }

  const std::string& text() const { return text_; }

  virtual void handleAction(Action action) NB_OVERRIDE {
    switch (action) {
      case kActionLeft:
        col_ = col_ == 0 ? rows_[row_].size() - 1 : col_ - 1;
        break;
      case kActionRight:
        col_ = (col_ + 1) % rows_[row_].size();
        break;
      case kActionUp: moveRow(row_ == 0 ? rows_.size() - 1 : row_ - 1); break;
      case kActionDown: moveRow((row_ + 1) % rows_.size()); break;
      case kActionAccept: press(rows_[row_][col_]); break;
      case kActionX: backspace(); break;
      case kActionY: insert(" "); break;
      case kActionPrevTab: shift_ = !shift_; break;
      case kActionMenu: finish(true); break;
      case kActionBack: finish(false); break;
      default: break;
    }
  }

  virtual void render(UiContext& ctx) NB_OVERRIDE {
    const Theme& t = *ctx.theme;
    const Strings& s = *ctx.strings;
    IRenderer& r = *ctx.renderer;
    FontSet& f = *ctx.fonts;
    widgets::dimBackground(ctx);

    const int keyH = 44;
    const int gap = 8;
    const int unitW = 74;
    const int gridW = 10 * unitW + 9 * gap;
    // Título + campo (124 px) + teclas + faixa de dicas (64 px).
    const int panelH = 124 + static_cast<int>(rows_.size()) * (keyH + gap) + 64;
    Rect panel((kScreenWidth - gridW) / 2 - 28, (kScreenHeight - panelH) / 2, gridW + 56, panelH);
    widgets::drawPanel(ctx, panel);
    text::draw(r, f.bold, t.metrics.fontBody + 2, panel.x + 28, panel.y + 18, title_, t.colors.text);

    Rect field(panel.x + 28, panel.y + 56, gridW, 48);
    r.fillRoundedRect(field, 10, withAlpha(0xFF000000u, 0x55));
    r.strokeRoundedRect(field, 10, 2, t.colors.accent);
    std::string shown = text_;
    int maxTextW = field.w - 40;
    // Mostra o fim do texto se não couber (o cursor fica sempre visível).
    while (!shown.empty() && text::measure(f.regular, t.metrics.fontBody, shown) > maxTextW) {
      size_t pos = 0;
      uint32_t cp = 0;
      str::decodeUtf8(shown, pos, cp);
      shown.erase(0, pos);
    }
    text::draw(r, f.regular, t.metrics.fontBody, field.x + 16, field.y + 12, shown, t.colors.text);
    bool caretOn = (ctx.nowMs / 500) % 2 == 0;
    if (caretOn) {
      int cx = field.x + 16 + text::measure(f.regular, t.metrics.fontBody, shown) + 2;
      r.fillRect(Rect(cx, field.y + 10, 2, field.h - 20), t.colors.accent);
    }

    int y = field.bottom() + 20;
    for (size_t ri = 0; ri < rows_.size(); ++ri) {
      int x = field.x;
      for (size_t ci = 0; ci < rows_[ri].size(); ++ci) {
        const Key& k = rows_[ri][ci];
        int w = k.units * unitW + (k.units - 1) * gap;
        Rect kr(x, y, w, keyH);
        bool sel = ri == row_ && ci == col_;
        bool active = k.kind == kKeyShift && shift_;
        r.fillRoundedRect(kr, 10, sel ? t.colors.accent
                                      : (active ? t.colors.surfaceFocused : t.colors.surface));
        std::string label = keyLabel(k, s);
        text::drawAligned(r, sel ? f.bold : f.regular, t.metrics.fontBody, kr.x,
                          kr.y + (keyH - f.regular.lineHeight(t.metrics.fontBody)) / 2, kr.w, label,
                          sel ? t.colors.accentText : t.colors.text, text::kAlignCenter);
        x += w + gap;
      }
      y += keyH + gap;
    }

    std::vector<ButtonHint> hints;
    hints.push_back(ButtonHint("A", s.get("hint.select")));
    hints.push_back(ButtonHint("X", s.get("hint.delete")));
    hints.push_back(ButtonHint("Y", s.get("hint.space")));
    hints.push_back(ButtonHint("LB", s.get("keyboard.shift")));
    hints.push_back(ButtonHint("START", s.get("hint.done")));
    hints.push_back(ButtonHint("B", s.get("hint.cancel")));
    widgets::drawButtonHintsAt(ctx, hints, panel.right() - 28, panel.bottom() - 44);
  }

 private:
  std::string keyLabel(const Key& k, const Strings& s) const {
    switch (k.kind) {
      case kKeyChar: return shift_ ? str::toUpperAscii(k.label) : k.label;
      case kKeyShift: return s.get("keyboard.shift");
      case kKeySpace: return s.get("keyboard.space");
      case kKeyBackspace: return s.get("keyboard.backspace");
      case kKeyClear: return s.get("keyboard.clear");
      case kKeyDone: return s.get("keyboard.done");
    }
    return k.label;
  }

  int columnCenter(size_t row, size_t col) const {
    int units = 0;
    for (size_t i = 0; i < col; ++i) units += rows_[row][i].units;
    return units * 2 + rows_[row][col].units;  // em meias unidades
  }

  void moveRow(size_t target) {
    int center = columnCenter(row_, col_);
    size_t best = 0;
    int units = 0;
    for (size_t i = 0; i < rows_[target].size(); ++i) {
      int start = units * 2;
      int end = (units + rows_[target][i].units) * 2;
      if (center >= start && center < end) best = i;
      if (center >= end) best = i;
      units += rows_[target][i].units;
    }
    row_ = target;
    col_ = best;
  }

  void notify() { app_.onKeyboardText(requestId_, text_, false, false); }

  void insert(const std::string& s) {
    size_t maxLength = mode_ == kKeyboardModePath ? kMaxPathLength : kMaxSearchLength;
    if (text_.size() + s.size() > maxLength) return;
    text_ += s;
    notify();
  }

  void backspace() {
    if (text_.empty()) return;
    size_t cut = text_.size() - 1;
    while (cut > 0 && (static_cast<unsigned char>(text_[cut]) & 0xC0) == 0x80) --cut;
    text_.erase(cut);
    notify();
  }

  void press(const Key& k) {
    switch (k.kind) {
      case kKeyChar: insert(shift_ ? str::toUpperAscii(k.label) : k.label); break;
      case kKeyShift: shift_ = !shift_; break;
      case kKeySpace: insert(" "); break;
      case kKeyBackspace: backspace(); break;
      case kKeyClear:
        text_.clear();
        notify();
        break;
      case kKeyDone: finish(true); break;
    }
  }

  void finish(bool accepted) {
    std::string result = accepted ? text_ : original_;
    app_.popScreen();
    app_.onKeyboardText(requestId_, result, true, accepted);
  }

  int requestId_;
  std::string title_;
  std::string text_;
  std::string original_;
  KeyboardMode mode_;
  std::vector<std::vector<Key> > rows_;
  size_t row_;
  size_t col_;
  bool shift_;
};

}  // namespace

Screen* createKeyboardScreen(IAppController& app, int requestId, const std::string& title,
                             const std::string& initialText, KeyboardMode mode) {
  return new KeyboardScreen(app, requestId, title, initialText, mode);
}

}  // namespace nebula
