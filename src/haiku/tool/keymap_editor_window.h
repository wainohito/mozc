// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_TOOL_KEYMAP_EDITOR_WINDOW_H_
#define MOZC_HAIKU_TOOL_KEYMAP_EDITOR_WINDOW_H_

#include <Window.h>

#include <memory>
#include <string>
#include <vector>

#include "client/client_interface.h"

class BButton;
class BColumnListView;
class BFilePanel;
class BPopUpMenu;

namespace mozc {
namespace haiku {

class KeyCaptureView;

// Edits the key bindings: which command a key runs in which state of the
// input. The result is saved as the custom keymap of the configuration.
class KeymapEditorWindow : public BWindow {
 public:
  KeymapEditorWindow();
  ~KeymapEditorWindow() override;

  void MessageReceived(BMessage* message) override;
  bool QuitRequested() override;

 private:
  struct Entry {
    std::string status;
    std::string key;
    std::string command;
  };

  void Load(const std::string& table);
  void LoadPredefined(int32 keymap);
  std::string Serialize() const;
  void FillList(int select);
  int CurrentEntry() const;
  bool ReadForm(Entry* entry);
  void UpdateButtons();
  bool Save();

  std::unique_ptr<client::ClientInterface> client_;
  std::vector<Entry> entries_;
  // Entries that cannot be edited here (text input, IME on/off keys); they
  // are kept as they are.
  std::string hidden_entries_;

  BColumnListView* list_;
  BPopUpMenu* status_menu_;
  KeyCaptureView* key_view_;
  BPopUpMenu* command_menu_;
  BButton* add_button_;
  BButton* update_button_;
  BButton* delete_button_;
  BFilePanel* open_panel_;
  BFilePanel* save_panel_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_TOOL_KEYMAP_EDITOR_WINDOW_H_
