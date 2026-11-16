// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Ported-from: Mozc for Haiku (hanya/mozc v2.26.4451.1, src/haiku/input_method)
// Ported-from-license: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_KEY_TRANSLATOR_H_
#define MOZC_HAIKU_INPUT_METHOD_KEY_TRANSLATOR_H_

#include <cstdint>
#include <string>

#include "protocol/commands.pb.h"

namespace mozc {
namespace haiku {

// What a B_KEY_DOWN or B_UNMAPPED_KEY_DOWN message of the input_server says
// about a key press.
struct HaikuKey {
  int32_t key = 0;         // "key": the raw key code (keyboard position).
  uint32_t modifiers = 0;  // "modifiers"
  std::string bytes;       // "bytes": UTF-8 produced by the keymap, if any.
  int32_t raw_char = 0;    // "raw_char": the character without modifiers.
};

// Layouts the kana of kana input are printed in.
enum class KanaLayout {
  kJis,  // Japanese (JIS) keyboard.
  kUs,   // US keyboard.
};

// Translates Haiku key presses into Mozc key events.
class KeyTranslator {
 public:
  KeyTranslator() = default;

  void set_kana_input(bool kana_input) { kana_input_ = kana_input; }
  void set_kana_layout(KanaLayout layout) { kana_layout_ = layout; }

  // Returns false if Mozc has no use for the key; the key should then be
  // passed to the application unchanged.
  bool Translate(const HaikuKey& key, commands::KeyEvent* event) const;

 private:
  bool kana_input_ = false;
  KanaLayout kana_layout_ = KanaLayout::kJis;
};

// Returns `event` in the notation of Mozc keymap tables ("Ctrl a",
// "Shift Space", "Hankaku/Zenkaku"), or an empty string if the table has
// no notation for it.
std::string ToKeymapString(const commands::KeyEvent& event);

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_KEY_TRANSLATOR_H_
