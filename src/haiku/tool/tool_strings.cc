// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/tool/tool_strings.h"

#include <LocaleRoster.h>
#include <Message.h>

#include <cstring>
#include <string_view>

namespace mozc {
namespace haiku {
namespace {

struct Entry {
  std::string_view en;
  const char* ja;
};

constexpr Entry kStrings[] = {
    {"Add word", "単語登録"},
    {"Word:", "単語:"},
    {"Reading:", "よみ:"},
    {"Part of speech:", "品詞:"},
    {"Dictionary:", "辞書:"},
    {"Comment:", "コメント:"},
    {"Add", "登録"},
    {"Cancel", "キャンセル"},
    {"OK", "OK"},
    {"Close", "閉じる"},
    {"Mozc dictionary tool", "Mozc 辞書ツール"},
    {"Dictionaries", "辞書"},
    {"New dictionary" "\xE2\x80\xA6", "新しい辞書" "\xE2\x80\xA6"},
    {"Rename dictionary" "\xE2\x80\xA6", "辞書の名前を変更" "\xE2\x80\xA6"},
    {"Delete dictionary", "辞書を削除"},
    {"Import" "\xE2\x80\xA6", "インポート" "\xE2\x80\xA6"},
    {"Export" "\xE2\x80\xA6", "エクスポート" "\xE2\x80\xA6"},
    {"Reading", "よみ"},
    {"Word", "単語"},
    {"Part of speech", "品詞"},
    {"Comment", "コメント"},
    {"Update", "更新"},
    {"Delete", "削除"},
    {"Dictionary name:", "辞書名:"},
    {"User dictionary", "ユーザー辞書"},
    {"Imported dictionary", "インポートした辞書"},
    {"The word was added.", "単語を登録しました。"},
    {"Cannot save the user dictionary.", "ユーザー辞書を保存できません。"},
    {"Cannot read the user dictionary.", "ユーザー辞書を読み込めません。"},
    {"Another program is editing the user dictionary.",
     "ほかのプログラムがユーザー辞書を編集しています。"},
    {"The reading is empty or contains characters that cannot be used.",
     "よみが空か、使えない文字が含まれています。"},
    {"The word is empty or too long.", "単語が空か、長すぎます。"},
    {"The dictionary is full.", "辞書がいっぱいです。"},
    {"Delete the dictionary \"%s\" and all its words?",
     "辞書「%s」とそのすべての単語を削除しますか？"},
    {"Some lines could not be imported.",
     "一部の行はインポートできませんでした。"},
    {"The file could not be imported.", "ファイルをインポートできません。"},
    {"Japanese input method", "日本語入力システム"},
    {"Version", "バージョン"},
    {"Mozc is open source software of Google, ported to RenkuOS.",
     "Mozc は Google のオープンソースソフトウェアを RenkuOS に移植したもの"
     "です。"},
    {"The conversion engine (mozc_server) could not be started.",
     "変換エンジン（mozc_server）を起動できませんでした。"},
    {"The conversion engine is of a different version. Log out and in "
     "again, or restart the input_server.",
     "変換エンジンのバージョンが異なります。ログインし直すか、input_server "
     "を再起動してください。"},
    {"The conversion engine stopped unexpectedly.",
     "変換エンジンが異常終了しました。"},
};

bool PrefersJapanese() {
  BMessage languages;
  const char* language = nullptr;
  return BLocaleRoster::Default()->GetPreferredLanguages(&languages) ==
             B_OK &&
         languages.FindString("language", 0, &language) == B_OK &&
         std::strncmp(language, "ja", 2) == 0;
}

}  // namespace

const char* Tr(const char* english) {
  static const bool japanese = PrefersJapanese();
  if (japanese) {
    for (const Entry& entry : kStrings) {
      if (entry.en == english) {
        return entry.ja;
      }
    }
  }
  return english;
}

}  // namespace haiku
}  // namespace mozc
