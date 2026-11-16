// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_STRINGS_H_
#define MOZC_HAIKU_INPUT_METHOD_STRINGS_H_

namespace mozc {
namespace haiku {

// User visible strings of the input method. The add-on lives in the
// input_server, which has no catalog of its own for them, so the Japanese
// translations are kept here and picked when Japanese is the preferred
// language of the user.
enum class Str {
  kHiragana,
  kFullKatakana,
  kFullAscii,
  kHalfKatakana,
  kHalfAscii,
  kDirect,
  kInputStyle,
  kRomaji,
  kKana,
  kKanaLayout,
  kKanaLayoutJis,
  kKanaLayoutUs,
  kPunctuation,
  kSymbols,
  kSpace,
  kSpaceFollowMode,
  kSpaceFull,
  kSpaceHalf,
  kKeymap,
  kKeymapCustom,
  kEditKeymap,
  kSelectionShortcut,
  kSelectionShortcutNone,
  kSuggestHistory,
  kSuggestDictionary,
  kSuggestRealtime,
  kWordRegister,
  kDictionaryTool,
  kAbout,
  kServerNotRunning,
  kCount,
};

const char* GetString(Str id);

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_STRINGS_H_
