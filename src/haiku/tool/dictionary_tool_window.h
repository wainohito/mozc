// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_TOOL_DICTIONARY_TOOL_WINDOW_H_
#define MOZC_HAIKU_TOOL_DICTIONARY_TOOL_WINDOW_H_

#include <Window.h>

#include "haiku/tool/dictionary_store.h"

class BButton;
class BColumnListView;
class BFilePanel;
class BListView;
class BPopUpMenu;
class BTextControl;

namespace mozc {
namespace haiku {

// Lists, edits, imports and exports the user dictionaries.
class DictionaryToolWindow : public BWindow {
 public:
  DictionaryToolWindow();
  ~DictionaryToolWindow() override;

  void MessageReceived(BMessage* message) override;
  bool QuitRequested() override;

 private:
  int CurrentDictionary() const;
  int CurrentEntry() const;
  void FillDictionaries(int select);
  void FillEntries();
  void EntrySelected();
  void UpdateButtons();
  bool ReadForm(DictionaryStore::Entry* entry);
  void Report(const absl::Status& status);

  DictionaryStore store_;
  BListView* dictionaries_;
  BColumnListView* entries_;
  BTextControl* reading_;
  BTextControl* word_;
  BPopUpMenu* pos_menu_;
  BTextControl* comment_;
  BButton* add_button_;
  BButton* update_button_;
  BButton* delete_button_;
  BFilePanel* open_panel_;
  BFilePanel* save_panel_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_TOOL_DICTIONARY_TOOL_WINDOW_H_
