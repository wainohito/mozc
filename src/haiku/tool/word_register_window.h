// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_TOOL_WORD_REGISTER_WINDOW_H_
#define MOZC_HAIKU_TOOL_WORD_REGISTER_WINDOW_H_

#include <Window.h>

#include "haiku/tool/dictionary_store.h"

class BButton;
class BMenuField;
class BPopUpMenu;
class BTextControl;

namespace mozc {
namespace haiku {

// Adds one word to a user dictionary.
class WordRegisterWindow : public BWindow {
 public:
  WordRegisterWindow();

  void MessageReceived(BMessage* message) override;
  bool QuitRequested() override;

 private:
  void Register();

  DictionaryStore store_;
  BTextControl* word_;
  BTextControl* reading_;
  BPopUpMenu* pos_menu_;
  BPopUpMenu* dictionary_menu_;
  BButton* add_button_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_TOOL_WORD_REGISTER_WINDOW_H_
