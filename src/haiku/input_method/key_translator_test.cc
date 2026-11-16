// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/key_translator.h"

#include <string>

#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace haiku {
namespace {

using commands::KeyEvent;

constexpr uint32_t kShift = 0x1;
constexpr uint32_t kCommand = 0x2;
constexpr uint32_t kControl = 0x4;

HaikuKey Key(int32_t key, std::string bytes, uint32_t modifiers = 0,
             int32_t raw_char = 0) {
  HaikuKey result;
  result.key = key;
  result.bytes = std::move(bytes);
  result.modifiers = modifiers;
  result.raw_char = raw_char != 0 ? raw_char
                    : result.bytes.empty()
                        ? 0
                        : static_cast<unsigned char>(result.bytes[0]);
  return result;
}

bool HasModifier(const KeyEvent& event, KeyEvent::ModifierKey modifier) {
  for (int m : event.modifier_keys()) {
    if (m == modifier) return true;
  }
  return false;
}

TEST(KeyTranslatorTest, PrintableAscii) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x3c, "a"), &event));
  EXPECT_EQ(event.key_code(), 'a');
  EXPECT_EQ(event.modifier_keys_size(), 0);

  // Shift is part of the character, not a modifier.
  ASSERT_TRUE(translator.Translate(Key(0x3c, "A", kShift, 'a'), &event));
  EXPECT_EQ(event.key_code(), 'A');
  EXPECT_EQ(event.modifier_keys_size(), 0);
  EXPECT_FALSE(event.has_key_string());
}

TEST(KeyTranslatorTest, SpecialKeys) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x5e, " "), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::SPACE);
  ASSERT_TRUE(translator.Translate(Key(0x47, "\n"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::ENTER);
  ASSERT_TRUE(translator.Translate(Key(0x1e, "\x08"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::BACKSPACE);
  ASSERT_TRUE(translator.Translate(Key(0x61, "\x1c"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::LEFT);
  ASSERT_TRUE(translator.Translate(Key(0x01, "\x1b"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::ESCAPE);

  // Shift stays a modifier of special keys.
  ASSERT_TRUE(translator.Translate(Key(0x5e, " ", kShift), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::SPACE);
  EXPECT_TRUE(HasModifier(event, KeyEvent::SHIFT));
}

TEST(KeyTranslatorTest, FunctionKeys) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x07, "\x10"), &event));  // F6
  EXPECT_EQ(event.special_key(), KeyEvent::F6);
  ASSERT_TRUE(translator.Translate(Key(0x0d, "\x10"), &event));  // F12
  EXPECT_EQ(event.special_key(), KeyEvent::F12);
  // Print Screen is not for Mozc.
  EXPECT_FALSE(translator.Translate(Key(0x0e, "\x10"), &event));
}

TEST(KeyTranslatorTest, JapaneseKeys) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x11, "\xf3"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::HANKAKU);
  // The grave accent of a US keymap is a character.
  ASSERT_TRUE(translator.Translate(Key(0x11, "`"), &event));
  EXPECT_EQ(event.key_code(), '`');
  // Muhenkan and Henkan produce nothing in the Japanese keymap.
  ASSERT_TRUE(translator.Translate(Key(0x6c, ""), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::MUHENKAN);
  ASSERT_TRUE(translator.Translate(Key(0x6d, ""), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::HENKAN);
  ASSERT_TRUE(translator.Translate(Key(0x6e, "\xf2"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::KANA);
  ASSERT_TRUE(translator.Translate(Key(0x6e, "\xf2", kShift), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::KATAKANA);
  EXPECT_FALSE(HasModifier(event, KeyEvent::SHIFT));
}

TEST(KeyTranslatorTest, Keypad) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x58, "1"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::NUMPAD1);
  ASSERT_TRUE(translator.Translate(Key(0x3a, "+"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::ADD);
  ASSERT_TRUE(translator.Translate(Key(0x5b, "\n"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::ENTER);
  // Without Num Lock the keypad moves the cursor.
  ASSERT_TRUE(translator.Translate(Key(0x58, "\x04"), &event));
  EXPECT_EQ(event.special_key(), KeyEvent::END);
}

TEST(KeyTranslatorTest, ControlAndCommand) {
  KeyTranslator translator;
  KeyEvent event;
  // The keymap turns Control+H into a backspace character.
  ASSERT_TRUE(translator.Translate(Key(0x41, "\x08", kControl, 'h'), &event));
  EXPECT_EQ(event.key_code(), 'h');
  EXPECT_TRUE(HasModifier(event, KeyEvent::CTRL));
  EXPECT_FALSE(event.has_special_key());

  ASSERT_TRUE(translator.Translate(Key(0x4e, "c", kCommand, 'c'), &event));
  EXPECT_EQ(event.key_code(), 'c');
  EXPECT_TRUE(HasModifier(event, KeyEvent::ALT));
}

TEST(KeyTranslatorTest, KanaInput) {
  KeyTranslator translator;
  translator.set_kana_input(true);
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x3c, "a"), &event));
  EXPECT_EQ(event.key_code(), 'a');
  EXPECT_EQ(event.key_string(), "ち");
  ASSERT_TRUE(translator.Translate(Key(0x14, "#", kShift, '3'), &event));
  EXPECT_EQ(event.key_string(), "ぁ");
  ASSERT_TRUE(translator.Translate(Key(0x6b, "\\"), &event));
  EXPECT_EQ(event.key_string(), "ろ");
  ASSERT_TRUE(translator.Translate(Key(0x6a, "\\"), &event));
  EXPECT_EQ(event.key_string(), "ー");

  translator.set_kana_layout(KanaLayout::kUs);
  ASSERT_TRUE(translator.Translate(Key(0x11, "`"), &event));
  EXPECT_EQ(event.key_string(), "ろ");
  ASSERT_TRUE(translator.Translate(Key(0x1c, "_", kShift, '-'), &event));
  EXPECT_EQ(event.key_string(), "ー");
}

TEST(KeyTranslatorTest, NonAsciiIsPassedThrough) {
  KeyTranslator translator;
  KeyEvent event;
  // é of a French keymap.
  EXPECT_FALSE(translator.Translate(Key(0x13, "\xc3\xa9"), &event));
  // Nothing at all.
  EXPECT_FALSE(translator.Translate(Key(0x66, ""), &event));
}

TEST(KeyTranslatorTest, KeymapString) {
  KeyTranslator translator;
  KeyEvent event;
  ASSERT_TRUE(translator.Translate(Key(0x41, "\x08", kControl, 'h'), &event));
  EXPECT_EQ(ToKeymapString(event), "Ctrl h");
  ASSERT_TRUE(translator.Translate(Key(0x5e, " ", kShift), &event));
  EXPECT_EQ(ToKeymapString(event), "Shift Space");
  ASSERT_TRUE(translator.Translate(Key(0x11, "\xf3"), &event));
  EXPECT_EQ(ToKeymapString(event), "Hankaku/Zenkaku");
  ASSERT_TRUE(translator.Translate(Key(0x0a, "\x10"), &event));
  EXPECT_EQ(ToKeymapString(event), "F9");

  // What is written can be read back by Mozc.
  for (const char* key : {"Ctrl a", "Shift Space", "Hankaku/Zenkaku", "F9",
                          "Ctrl Shift Space", "Muhenkan", "PageDown",
                          "Ctrl Alt Shift Henkan", "Hiragana", "ESC"}) {
    KeyEvent parsed;
    ASSERT_TRUE(KeyParser::ParseKey(key, &parsed)) << key;
    EXPECT_EQ(ToKeymapString(parsed), key);
  }
}

}  // namespace
}  // namespace haiku
}  // namespace mozc
