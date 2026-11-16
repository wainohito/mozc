// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/strings.h"

#include <InterfaceDefs.h>
#include <LocaleRoster.h>
#include <Message.h>

#include <cstring>
#include <iterator>

namespace mozc {
namespace haiku {
namespace {

struct Entry {
  const char* en;
  const char* ja;
};

constexpr Entry kStrings[] = {
    {"Hiragana", "ひらがな"},
    {"Full-width Katakana", "全角カタカナ"},
    {"Full-width alphanumeric", "全角英数"},
    {"Half-width Katakana", "半角カタカナ"},
    {"Half-width alphanumeric", "半角英数"},
    {"Direct input", "直接入力"},
    {"Input style", "入力方式"},
    {"Romaji", "ローマ字入力"},
    {"Kana", "かな入力"},
    {"Kana layout", "かな配列"},
    {"Japanese keyboard", "日本語キーボード"},
    {"US keyboard", "英語キーボード"},
    {"Punctuation", "句読点"},
    {"Symbols", "記号"},
    {"Space", "スペースの入力"},
    {"Follow input mode", "入力モードに従う"},
    {"Always full-width", "常に全角"},
    {"Always half-width", "常に半角"},
    {"Key bindings", "キー設定"},
    {"Custom", "カスタム"},
    {"Candidate selection keys", "候補選択キー"},
    {"None", "なし"},
    {"Suggest from input history", "入力履歴からサジェスト"},
    {"Suggest from system dictionary", "システム辞書からサジェスト"},
    {"Realtime conversion", "リアルタイム変換"},
    {"Add word" B_UTF8_ELLIPSIS, "単語登録" B_UTF8_ELLIPSIS},
    {"Dictionary tool" B_UTF8_ELLIPSIS, "辞書ツール" B_UTF8_ELLIPSIS},
    {"About Mozc" B_UTF8_ELLIPSIS, "Mozc について" B_UTF8_ELLIPSIS},
    {"Conversion engine is not running", "変換エンジンが動いていません"},
};
static_assert(std::size(kStrings) == static_cast<size_t>(Str::kCount));

bool PrefersJapanese() {
  BMessage languages;
  if (BLocaleRoster::Default()->GetPreferredLanguages(&languages) != B_OK) {
    return false;
  }
  const char* language = nullptr;
  return languages.FindString("language", 0, &language) == B_OK &&
         std::strncmp(language, "ja", 2) == 0;
}

}  // namespace

const char* GetString(Str id) {
  // The preferred language rarely changes; the menu is rebuilt often.
  static const bool japanese = PrefersJapanese();
  const Entry& entry = kStrings[static_cast<size_t>(id)];
  return japanese ? entry.ja : entry.en;
}

}  // namespace haiku
}  // namespace mozc
