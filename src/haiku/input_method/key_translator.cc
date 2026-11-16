// Copyright 2010-2016, Google Inc.
// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Ported-from: Mozc for Haiku (hanya/mozc v2.26.4451.1, src/haiku/input_method)
// Ported-from-license: BSD-3-Clause

#include "haiku/input_method/key_translator.h"

#include <cstdint>
#include <string>

#include "protocol/commands.pb.h"

#ifdef __HAIKU__
#include <InterfaceDefs.h>
#endif  // __HAIKU__

namespace mozc {
namespace haiku {
namespace {

using commands::KeyEvent;

// Values of <InterfaceDefs.h>. They are part of the Haiku ABI; having them
// here lets the translator be tested on the build host.
constexpr uint8_t kBackspace = 0x08;
constexpr uint8_t kReturn = 0x0a;
constexpr uint8_t kTab = 0x09;
constexpr uint8_t kEscape = 0x1b;
constexpr uint8_t kSpace = 0x20;
constexpr uint8_t kLeftArrow = 0x1c;
constexpr uint8_t kRightArrow = 0x1d;
constexpr uint8_t kUpArrow = 0x1e;
constexpr uint8_t kDownArrow = 0x1f;
constexpr uint8_t kInsert = 0x05;
constexpr uint8_t kDelete = 0x7f;
constexpr uint8_t kHome = 0x01;
constexpr uint8_t kEnd = 0x04;
constexpr uint8_t kPageUp = 0x0b;
constexpr uint8_t kPageDown = 0x0c;
constexpr uint8_t kFunctionKey = 0x10;
constexpr int32_t kF1Key = 0x02;
constexpr int32_t kF12Key = 0x0d;

constexpr uint32_t kShiftKey = 0x00000001;
constexpr uint32_t kCommandKey = 0x00000002;
constexpr uint32_t kControlKey = 0x00000004;
constexpr uint32_t kLeftShiftKey = 0x00000100;
constexpr uint32_t kRightShiftKey = 0x00000200;

#ifdef __HAIKU__
static_assert(kBackspace == B_BACKSPACE && kReturn == B_RETURN &&
              kTab == B_TAB && kEscape == B_ESCAPE && kSpace == B_SPACE);
static_assert(kLeftArrow == B_LEFT_ARROW && kRightArrow == B_RIGHT_ARROW &&
              kUpArrow == B_UP_ARROW && kDownArrow == B_DOWN_ARROW);
static_assert(kInsert == B_INSERT && kDelete == B_DELETE && kHome == B_HOME &&
              kEnd == B_END && kPageUp == B_PAGE_UP &&
              kPageDown == B_PAGE_DOWN && kFunctionKey == B_FUNCTION_KEY);
static_assert(kF1Key == B_F1_KEY && kF12Key == B_F12_KEY);
static_assert(kShiftKey == B_SHIFT_KEY && kCommandKey == B_COMMAND_KEY &&
              kControlKey == B_CONTROL_KEY &&
              kLeftShiftKey == B_LEFT_SHIFT_KEY &&
              kRightShiftKey == B_RIGHT_SHIFT_KEY);
#endif  // __HAIKU__

// Raw key codes of keys found on Japanese keyboards. The Japanese keymap
// produces 0xf3 for Hankaku/Zenkaku and 0xf2 for Katakana/Hiragana, and
// nothing for Muhenkan and Henkan (B_UNMAPPED_KEY_DOWN).
constexpr int32_t kKeyHankakuZenkaku = 0x11;
constexpr uint8_t kByteHankakuZenkaku = 0xf3;
constexpr int32_t kKeyYen = 0x6a;
constexpr int32_t kKeyRo = 0x6b;
constexpr int32_t kKeyMuhenkan = 0x6c;
constexpr int32_t kKeyHenkan = 0x6d;
constexpr int32_t kKeyKatakanaHiragana = 0x6e;

struct KeypadKey {
  int32_t key;
  char byte;  // What the key produces while Num Lock is on.
  KeyEvent::SpecialKey special_key;
};

constexpr KeypadKey kKeypadKeys[] = {
    {0x23, '/', KeyEvent::DIVIDE},   {0x24, '*', KeyEvent::MULTIPLY},
    {0x25, '-', KeyEvent::SUBTRACT}, {0x3a, '+', KeyEvent::ADD},
    {0x37, '7', KeyEvent::NUMPAD7},  {0x38, '8', KeyEvent::NUMPAD8},
    {0x39, '9', KeyEvent::NUMPAD9},  {0x48, '4', KeyEvent::NUMPAD4},
    {0x49, '5', KeyEvent::NUMPAD5},  {0x4a, '6', KeyEvent::NUMPAD6},
    {0x58, '1', KeyEvent::NUMPAD1},  {0x59, '2', KeyEvent::NUMPAD2},
    {0x5a, '3', KeyEvent::NUMPAD3},  {0x64, '0', KeyEvent::NUMPAD0},
    {0x65, '.', KeyEvent::DECIMAL},
};
constexpr int32_t kKeypadEnter = 0x5b;

struct KanaKey {
  int32_t key;
  const char* kana;
  const char* shifted_kana;
};

// Kana printed on Japanese (JIS) keyboards, by raw key code.
constexpr KanaKey kJisKana[] = {
    {0x12, "ぬ", "ぬ"}, {0x13, "ふ", "ふ"}, {0x14, "あ", "ぁ"},
    {0x15, "う", "ぅ"}, {0x16, "え", "ぇ"}, {0x17, "お", "ぉ"},
    {0x18, "や", "ゃ"}, {0x19, "ゆ", "ゅ"}, {0x1a, "よ", "ょ"},
    {0x1b, "わ", "を"}, {0x1c, "ほ", "ほ"}, {0x1d, "へ", "へ"},
    {0x6a, "ー", "ー"}, {0x27, "た", "た"}, {0x28, "て", "て"},
    {0x29, "い", "ぃ"}, {0x2a, "す", "す"}, {0x2b, "か", "か"},
    {0x2c, "ん", "ん"}, {0x2d, "な", "な"}, {0x2e, "に", "に"},
    {0x2f, "ら", "ら"}, {0x30, "せ", "せ"}, {0x31, "゛", "゛"},
    {0x32, "゜", "「"}, {0x3c, "ち", "ち"}, {0x3d, "と", "と"},
    {0x3e, "し", "し"}, {0x3f, "は", "は"}, {0x40, "き", "き"},
    {0x41, "く", "く"}, {0x42, "ま", "ま"}, {0x43, "の", "の"},
    {0x44, "り", "り"}, {0x45, "れ", "れ"}, {0x46, "け", "け"},
    {0x33, "む", "」"}, {0x4c, "つ", "っ"}, {0x4d, "さ", "さ"},
    {0x4e, "そ", "そ"}, {0x4f, "ひ", "ひ"}, {0x50, "こ", "こ"},
    {0x51, "み", "み"}, {0x52, "も", "も"}, {0x53, "ね", "、"},
    {0x54, "る", "。"}, {0x55, "め", "・"}, {0x6b, "ろ", "ろ"},
};

// Kana input on US keyboards, which have neither Yen nor Ro: Ro moves to
// the grave accent key, and the prolonged sound mark to Shift and minus.
constexpr KanaKey kUsKana[] = {
    {0x11, "ろ", "ろ"}, {0x12, "ぬ", "ぬ"}, {0x13, "ふ", "ふ"},
    {0x14, "あ", "ぁ"}, {0x15, "う", "ぅ"}, {0x16, "え", "ぇ"},
    {0x17, "お", "ぉ"}, {0x18, "や", "ゃ"}, {0x19, "ゆ", "ゅ"},
    {0x1a, "よ", "ょ"}, {0x1b, "わ", "を"}, {0x1c, "ほ", "ー"},
    {0x1d, "へ", "へ"}, {0x27, "た", "た"}, {0x28, "て", "て"},
    {0x29, "い", "ぃ"}, {0x2a, "す", "す"}, {0x2b, "か", "か"},
    {0x2c, "ん", "ん"}, {0x2d, "な", "な"}, {0x2e, "に", "に"},
    {0x2f, "ら", "ら"}, {0x30, "せ", "せ"}, {0x31, "゛", "゛"},
    {0x32, "゜", "「"}, {0x33, "む", "」"}, {0x3c, "ち", "ち"},
    {0x3d, "と", "と"}, {0x3e, "し", "し"}, {0x3f, "は", "は"},
    {0x40, "き", "き"}, {0x41, "く", "く"}, {0x42, "ま", "ま"},
    {0x43, "の", "の"}, {0x44, "り", "り"}, {0x45, "れ", "れ"},
    {0x46, "け", "け"}, {0x4c, "つ", "っ"}, {0x4d, "さ", "さ"},
    {0x4e, "そ", "そ"}, {0x4f, "ひ", "ひ"}, {0x50, "こ", "こ"},
    {0x51, "み", "み"}, {0x52, "も", "も"}, {0x53, "ね", "、"},
    {0x54, "る", "。"}, {0x55, "め", "・"},
};

template <size_t N>
const KanaKey* FindKana(const KanaKey (&table)[N], int32_t key) {
  for (const KanaKey& entry : table) {
    if (entry.key == key) {
      return &entry;
    }
  }
  return nullptr;
}

bool IsPrintableAscii(uint32_t c) { return c > 0x20 && c < 0x7f; }

KeyEvent::SpecialKey SpecialKeyFromByte(uint8_t byte) {
  switch (byte) {
    case kBackspace:
      return KeyEvent::BACKSPACE;
    case kReturn:
      return KeyEvent::ENTER;
    case kTab:
      return KeyEvent::TAB;
    case kEscape:
      return KeyEvent::ESCAPE;
    case kSpace:
      return KeyEvent::SPACE;
    case kLeftArrow:
      return KeyEvent::LEFT;
    case kRightArrow:
      return KeyEvent::RIGHT;
    case kUpArrow:
      return KeyEvent::UP;
    case kDownArrow:
      return KeyEvent::DOWN;
    case kInsert:
      return KeyEvent::INSERT;
    case kDelete:
      return KeyEvent::DEL;
    case kHome:
      return KeyEvent::HOME;
    case kEnd:
      return KeyEvent::END;
    case kPageUp:
      return KeyEvent::PAGE_UP;
    case kPageDown:
      return KeyEvent::PAGE_DOWN;
    default:
      return KeyEvent::NO_SPECIALKEY;
  }
}

}  // namespace

bool KeyTranslator::Translate(const HaikuKey& key, KeyEvent* event) const {
  event->Clear();
  const uint8_t byte =
      key.bytes.empty() ? 0 : static_cast<uint8_t>(key.bytes[0]);
  const bool shift = (key.modifiers & kShiftKey) != 0;
  const bool control = (key.modifiers & kControlKey) != 0;
  const bool command = (key.modifiers & kCommandKey) != 0;
  // Shift is part of a printable character, but is a modifier of a special
  // key (Shift+Enter, Shift+Space, Shift+Henkan ...).
  bool shift_is_modifier = true;

  // Keys of Japanese keyboards, by position.
  if (key.key == kKeyHankakuZenkaku && byte == kByteHankakuZenkaku) {
    event->set_special_key(KeyEvent::HANKAKU);
  } else if (key.key == kKeyMuhenkan) {
    event->set_special_key(KeyEvent::MUHENKAN);
  } else if (key.key == kKeyHenkan) {
    event->set_special_key(KeyEvent::HENKAN);
  } else if (key.key == kKeyKatakanaHiragana) {
    if (shift) {
      event->set_special_key(KeyEvent::KATAKANA);
      shift_is_modifier = false;
    } else {
      event->set_special_key(KeyEvent::KANA);
    }
  } else if (key.key == kKeypadEnter) {
    event->set_special_key(KeyEvent::ENTER);
  } else if (byte == kFunctionKey) {
    if (key.key < kF1Key || key.key > kF12Key) {
      return false;  // Print Screen, Scroll Lock, Pause.
    }
    event->set_special_key(static_cast<KeyEvent::SpecialKey>(
        KeyEvent::F1 + (key.key - kF1Key)));
  } else {
    // The keypad produces digits and operators only while Num Lock is on;
    // otherwise it produces cursor keys, which are handled below.
    for (const KeypadKey& keypad : kKeypadKeys) {
      if (keypad.key == key.key && keypad.byte == byte) {
        event->set_special_key(keypad.special_key);
        break;
      }
    }
  }

  if (!event->has_special_key()) {
    const KeyEvent::SpecialKey special_key = SpecialKeyFromByte(byte);
    if (special_key != KeyEvent::NO_SPECIALKEY && !control) {
      event->set_special_key(special_key);
    } else if ((control || command) && IsPrintableAscii(key.raw_char)) {
      // The keymap turns Control+letter into control characters; Mozc wants
      // the letter.
      event->set_key_code(key.raw_char);
    } else if (control && special_key != KeyEvent::NO_SPECIALKEY) {
      event->set_special_key(special_key);
    } else if (key.bytes.size() == 1 && IsPrintableAscii(byte)) {
      event->set_key_code(byte);
      shift_is_modifier = false;
      if (kana_input_) {
        const KanaKey* kana =
            kana_layout_ == KanaLayout::kJis ? FindKana(kJisKana, key.key)
                                             : FindKana(kUsKana, key.key);
        if (kana != nullptr) {
          event->set_key_string(shift ? kana->shifted_kana : kana->kana);
        }
      }
    } else {
      // Non ASCII characters of other keymaps, dead keys, or nothing at all.
      return false;
    }
  }

  if (shift && shift_is_modifier) {
    event->add_modifier_keys(KeyEvent::SHIFT);
  }
  if (control) {
    event->add_modifier_keys(KeyEvent::CTRL);
  }
  if (command) {
    // The Command key of Haiku is what other systems call Alt.
    event->add_modifier_keys(KeyEvent::ALT);
  }
  return true;
}

}  // namespace haiku
}  // namespace mozc
