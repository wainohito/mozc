// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/tool/keymap_editor_window.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <StringView.h>

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_split.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "client/client.h"
#include "composer/key_parser.h"
#include "haiku/input_method/key_translator.h"
#include "haiku/tool/tool_strings.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/keymap.h"

namespace mozc {
namespace haiku {
namespace {

using commands::KeyEvent;
using config::Config;

enum : uint32 {
  kMsgSelected = 'Ksel',
  kMsgFormChanged = 'Kchg',
  kMsgAdd = 'Kadd',
  kMsgUpdate = 'Kupd',
  kMsgDelete = 'Kdel',
  kMsgPredefined = 'Kpre',
  kMsgImport = 'Kimp',
  kMsgExport = 'Kexp',
  kMsgSave = 'Ksav',
};

enum Column { kStatusColumn, kKeyColumn, kCommandColumn };

constexpr std::string_view kStatuses[] = {
    "DirectInput", "Precomposition", "Composition",
    "Conversion",  "Suggestion",     "Prediction",
};

struct Translation {
  std::string_view en;
  const char* ja;
};

constexpr Translation kKeymapStrings[] = {
#include "haiku/tool/keymap_strings.inc"
};

// The names of states and commands, as the user reads them.
const char* KeymapTr(const std::string& name) {
  if (PrefersJapanese()) {
    for (const Translation& translation : kKeymapStrings) {
      if (translation.en == name) {
        return translation.ja;
      }
    }
  }
  return name.c_str();
}

// Keys and commands the keymap editors of Mozc leave alone.
bool IsEditable(const std::string& key, const std::string& command) {
  if (command == "InsertCharacter" || command == "ReportBug" ||
      command == "EditInsert") {
    return false;
  }
  KeyEvent event;
  if (!KeyParser::ParseKey(key, &event)) {
    return false;
  }
  for (const int modifier : event.modifier_keys()) {
    if (modifier == KeyEvent::KEY_DOWN || modifier == KeyEvent::KEY_UP) {
      return false;
    }
  }
  if (event.has_special_key()) {
    switch (event.special_key()) {
      case KeyEvent::KANJI:
      case KeyEvent::ON:
      case KeyEvent::OFF:
      case KeyEvent::TEXT_INPUT:
        return false;
      default:
        break;
    }
  }
  return true;
}

std::vector<std::string> CommandNames() {
  absl::flat_hash_set<std::string> names;
  const keymap::KeyMapManager manager;
  manager.AppendAvailableCommandNameDirect(names);
  manager.AppendAvailableCommandNamePrecomposition(names);
  manager.AppendAvailableCommandNameComposition(names);
  manager.AppendAvailableCommandNameConversion(names);
  manager.AppendAvailableCommandNameZeroQuerySuggestion(names);
  manager.AppendAvailableCommandNameSuggestion(names);
  manager.AppendAvailableCommandNamePrediction(names);
  std::vector<std::string> result;
  for (const std::string& name : names) {
    if (IsEditable("a", name)) {
      result.push_back(name);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const std::string& a, const std::string& b) {
              return std::string_view(KeymapTr(a)) < KeymapTr(b);
            });
  return result;
}

void Alert(const char* text) {
  (new BAlert("Mozc", text, Tr("OK"), nullptr, nullptr, B_WIDTH_AS_USUAL,
              B_WARNING_ALERT))
      ->Go(nullptr);
}

// A row of the list; it remembers the entry it shows.
class EntryRow : public BRow {
 public:
  EntryRow(int index, const char* status, const char* key,
           const char* command)
      : index_(index) {
    SetField(new BStringField(status), kStatusColumn);
    SetField(new BStringField(key), kKeyColumn);
    SetField(new BStringField(command), kCommandColumn);
  }
  int index() const { return index_; }

 private:
  const int index_;
};

}  // namespace

// Shows a key, and takes the next key pressed while it has the focus.
class KeyCaptureView : public BView {
 public:
  KeyCaptureView()
      : BView("key", B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS) {
    SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetExplicitMinSize(BSize(160, be_plain_font->Size() * 2));
  }

  const std::string& key() const { return key_; }
  void SetKey(std::string key) {
    key_ = std::move(key);
    Invalidate();
  }

  void Draw(BRect update) override {
    BRect bounds = Bounds();
    SetHighUIColor(IsFocus() ? B_KEYBOARD_NAVIGATION_COLOR
                             : B_CONTROL_BORDER_COLOR);
    StrokeRect(bounds);
    font_height height;
    GetFontHeight(&height);
    const bool empty = key_.empty();
    SetHighUIColor(B_DOCUMENT_TEXT_COLOR,
                   empty ? B_DISABLED_LABEL_TINT : B_NO_TINT);
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    DrawString(empty ? Tr("Click, then press the key") : key_.c_str(),
               BPoint(5, (bounds.Height() + height.ascent - height.descent) /
                             2));
  }

  void MouseDown(BPoint) override { MakeFocus(true); }
  void MakeFocus(bool focus) override {
    BView::MakeFocus(focus);
    Invalidate();
  }

  void KeyDown(const char* bytes, int32 count) override {
    if (!Capture(Window()->CurrentMessage())) {
      BView::KeyDown(bytes, count);
    }
  }

  void MessageReceived(BMessage* message) override {
    if (message->what == B_UNMAPPED_KEY_DOWN && Capture(message)) {
      return;
    }
    BView::MessageReceived(message);
  }

 private:
  bool Capture(const BMessage* message) {
    if (message == nullptr) {
      return false;
    }
    HaikuKey key;
    key.key = message->GetInt32("key", 0);
    key.modifiers = static_cast<uint32_t>(message->GetInt32("modifiers", 0));
    key.bytes = message->GetString("bytes", "");
    key.raw_char = message->GetInt32("raw_char", 0);
    KeyEvent event;
    if (!KeyTranslator().Translate(key, &event)) {
      return false;
    }
    std::string text = ToKeymapString(event);
    if (text.empty()) {
      return false;
    }
    SetKey(std::move(text));
    Window()->PostMessage(kMsgFormChanged);
    return true;
  }

  std::string key_;
};

KeymapEditorWindow::KeymapEditorWindow()
    : BWindow(BRect(80, 80, 680, 560), Tr("Mozc key bindings"),
              B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
      client_(client::ClientFactory::NewClient()),
      open_panel_(nullptr),
      save_panel_(nullptr) {
  BMenuBar* menu_bar = new BMenuBar("menu");
  BMenu* menu = new BMenu(Tr("Key bindings"));
  BMenu* predefined = new BMenu(Tr("Start from"));
  for (const auto& [label, keymap] :
       {std::pair{"MS-IME", Config::MSIME}, std::pair{"ATOK", Config::ATOK},
        std::pair{"ことえり", Config::KOTOERI}}) {
    BMessage* message = new BMessage(kMsgPredefined);
    message->AddInt32("keymap", keymap);
    predefined->AddItem(new BMenuItem(label, message));
  }
  menu->AddItem(predefined);
  menu->AddSeparatorItem();
  menu->AddItem(new BMenuItem(Tr("Import\xE2\x80\xA6"),
                              new BMessage(kMsgImport), 'O'));
  menu->AddItem(new BMenuItem(Tr("Export\xE2\x80\xA6"),
                              new BMessage(kMsgExport), 'S'));
  menu->AddSeparatorItem();
  menu->AddItem(
      new BMenuItem(Tr("Close"), new BMessage(B_QUIT_REQUESTED), 'W'));
  menu_bar->AddItem(menu);

  list_ = new BColumnListView("bindings", B_NAVIGABLE, B_FANCY_BORDER);
  list_->AddColumn(new BStringColumn(Tr("State"), 150, 50, 300,
                                     B_TRUNCATE_END),
                   kStatusColumn);
  list_->AddColumn(
      new BStringColumn(Tr("Key"), 150, 50, 300, B_TRUNCATE_END),
      kKeyColumn);
  list_->AddColumn(new BStringColumn(Tr("Command"), 250, 50, 500,
                                     B_TRUNCATE_END),
                   kCommandColumn);
  list_->SetSelectionMessage(new BMessage(kMsgSelected));
  list_->SetSelectionMode(B_SINGLE_SELECTION_LIST);

  status_menu_ = new BPopUpMenu("state");
  for (const std::string_view status : kStatuses) {
    BMessage* message = new BMessage(kMsgFormChanged);
    message->AddString("name", std::string(status).c_str());
    status_menu_->AddItem(
        new BMenuItem(KeymapTr(std::string(status)), message));
  }
  status_menu_->ItemAt(1)->SetMarked(true);
  command_menu_ = new BPopUpMenu("command");
  for (const std::string& name : CommandNames()) {
    BMessage* message = new BMessage(kMsgFormChanged);
    message->AddString("name", name.c_str());
    command_menu_->AddItem(new BMenuItem(KeymapTr(name), message));
  }
  if (command_menu_->CountItems() > 0) {
    command_menu_->ItemAt(0)->SetMarked(true);
  }
  key_view_ = new KeyCaptureView();

  add_button_ = new BButton(Tr("Add"), new BMessage(kMsgAdd));
  update_button_ = new BButton(Tr("Update"), new BMessage(kMsgUpdate));
  delete_button_ = new BButton(Tr("Delete"), new BMessage(kMsgDelete));
  BButton* save = new BButton(Tr("Save"), new BMessage(kMsgSave));

  BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
      .Add(menu_bar)
      .AddGroup(B_VERTICAL)
      .SetInsets(B_USE_WINDOW_SPACING)
      .Add(list_, 1.0f)
      .AddGrid()
      .AddMenuField(new BMenuField(Tr("State:"), status_menu_), 0, 0)
      .Add(new BStringView("key label", Tr("Key:")), 0, 1)
      .Add(key_view_, 1, 1)
      .AddMenuField(new BMenuField(Tr("Command:"), command_menu_), 0, 2)
      .End()
      .AddGroup(B_HORIZONTAL)
      .Add(delete_button_)
      .Add(update_button_)
      .Add(add_button_)
      .AddGlue()
      .Add(new BButton(Tr("Cancel"), new BMessage(B_QUIT_REQUESTED)))
      .Add(save)
      .End()
      .End();
  SetDefaultButton(save);

  Config config;
  if (client_->GetConfig(&config) &&
      config.session_keymap() == Config::CUSTOM &&
      !config.custom_keymap_table().empty()) {
    Load(config.custom_keymap_table());
  } else {
    LoadPredefined(client_->GetConfig(&config) &&
                           config.session_keymap() != Config::CUSTOM &&
                           config.session_keymap() != Config::NONE
                       ? config.session_keymap()
                       : Config::MSIME);
  }
  CenterOnScreen();
}

KeymapEditorWindow::~KeymapEditorWindow() {
  delete open_panel_;
  delete save_panel_;
}

bool KeymapEditorWindow::QuitRequested() {
  be_app->PostMessage(B_QUIT_REQUESTED);
  return true;
}

void KeymapEditorWindow::MessageReceived(BMessage* message) {
  switch (message->what) {
    case kMsgSelected: {
      const int index = CurrentEntry();
      if (index >= 0) {
        const Entry& entry = entries_[index];
        for (int32 i = 0; i < status_menu_->CountItems(); ++i) {
          BMenuItem* item = status_menu_->ItemAt(i);
          if (entry.status == item->Message()->GetString("name", "")) {
            item->SetMarked(true);
          }
        }
        for (int32 i = 0; i < command_menu_->CountItems(); ++i) {
          BMenuItem* item = command_menu_->ItemAt(i);
          if (entry.command == item->Message()->GetString("name", "")) {
            item->SetMarked(true);
          }
        }
        key_view_->SetKey(entry.key);
      }
      UpdateButtons();
      break;
    }
    case kMsgFormChanged:
      UpdateButtons();
      break;
    case kMsgAdd: {
      Entry entry;
      if (ReadForm(&entry)) {
        entries_.push_back(entry);
        FillList(static_cast<int>(entries_.size()) - 1);
      }
      break;
    }
    case kMsgUpdate: {
      Entry entry;
      const int index = CurrentEntry();
      if (index >= 0 && ReadForm(&entry)) {
        entries_[index] = entry;
        FillList(index);
      }
      break;
    }
    case kMsgDelete: {
      const int index = CurrentEntry();
      if (index >= 0) {
        entries_.erase(entries_.begin() + index);
        FillList(-1);
      }
      break;
    }
    case kMsgPredefined:
      LoadPredefined(message->GetInt32("keymap", Config::MSIME));
      break;
    case kMsgImport:
      if (open_panel_ == nullptr) {
        open_panel_ = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
                                     nullptr, B_FILE_NODE, false);
      }
      open_panel_->Show();
      break;
    case kMsgExport:
      if (save_panel_ == nullptr) {
        save_panel_ = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));
        save_panel_->SetSaveText("keymap.txt");
      }
      save_panel_->Show();
      break;
    case B_REFS_RECEIVED: {
      entry_ref ref;
      if (message->FindRef("refs", &ref) == B_OK) {
        absl::StatusOr<std::string> data =
            FileUtil::GetContents(BPath(&ref).Path());
        if (data.ok()) {
          Load(*data);
        } else {
          Alert(Tr("The file could not be imported."));
        }
      }
      break;
    }
    case B_SAVE_REQUESTED: {
      entry_ref directory;
      const char* name = nullptr;
      if (message->FindRef("directory", &directory) == B_OK &&
          message->FindString("name", &name) == B_OK) {
        BPath path(&directory);
        path.Append(name);
        const std::string table = Serialize();
        BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        if (file.InitCheck() != B_OK ||
            file.Write(table.data(), table.size()) !=
                static_cast<ssize_t>(table.size())) {
          Alert(Tr("The file could not be saved."));
        }
      }
      break;
    }
    case kMsgSave:
      if (Save()) {
        PostMessage(B_QUIT_REQUESTED);
      }
      break;
    default:
      BWindow::MessageReceived(message);
      break;
  }
}

void KeymapEditorWindow::Load(const std::string& table) {
  entries_.clear();
  hidden_entries_.clear();
  std::istringstream stream(table);
  std::string line;
  bool first = true;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (first) {
      first = false;  // The header: status, key, command.
      continue;
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::SkipEmpty());
    if (fields.size() < 3) {
      continue;
    }
    if (IsEditable(fields[1], fields[2])) {
      entries_.push_back({fields[0], fields[1], fields[2]});
    } else {
      hidden_entries_ += fields[0] + "\t" + fields[1] + "\t" + fields[2] + "\n";
    }
  }
  FillList(-1);
}

void KeymapEditorWindow::LoadPredefined(int32 keymap) {
  const char* file = keymap::KeyMapManager::GetKeyMapFileName(
      static_cast<Config::SessionKeymap>(keymap));
  std::unique_ptr<std::istream> stream =
      file != nullptr ? ConfigFileStream::LegacyOpen(file) : nullptr;
  if (stream == nullptr) {
    return;
  }
  std::stringstream buffer;
  buffer << stream->rdbuf();
  Load(buffer.str());
}

std::string KeymapEditorWindow::Serialize() const {
  std::string table = "status\tkey\tcommand\n";
  for (const Entry& entry : entries_) {
    table += entry.status + "\t" + entry.key + "\t" + entry.command + "\n";
  }
  return table + hidden_entries_;
}

void KeymapEditorWindow::FillList(int select) {
  list_->Clear();
  for (size_t i = 0; i < entries_.size(); ++i) {
    const Entry& entry = entries_[i];
    EntryRow* row =
        new EntryRow(static_cast<int>(i), KeymapTr(entry.status),
                     entry.key.c_str(), KeymapTr(entry.command));
    list_->AddRow(row);
    if (static_cast<int>(i) == select) {
      list_->AddToSelection(row);
      list_->ScrollTo(row);
    }
  }
  UpdateButtons();
}

int KeymapEditorWindow::CurrentEntry() const {
  const EntryRow* row = static_cast<EntryRow*>(list_->CurrentSelection());
  return row != nullptr ? row->index() : -1;
}

bool KeymapEditorWindow::ReadForm(Entry* entry) {
  BMenuItem* status = status_menu_->FindMarked();
  BMenuItem* command = command_menu_->FindMarked();
  if (status == nullptr || command == nullptr || key_view_->key().empty()) {
    return false;
  }
  entry->status = status->Message()->GetString("name", "");
  entry->key = key_view_->key();
  entry->command = command->Message()->GetString("name", "");
  return true;
}

void KeymapEditorWindow::UpdateButtons() {
  const bool filled = !key_view_->key().empty();
  add_button_->SetEnabled(filled);
  update_button_->SetEnabled(filled && CurrentEntry() >= 0);
  delete_button_->SetEnabled(CurrentEntry() >= 0);
}

bool KeymapEditorWindow::Save() {
  if (entries_.empty()) {
    Alert(Tr("The key bindings are empty. Start from MS-IME, ATOK or "
             "Kotoeri first."));
    return false;
  }
  Config config;
  if (!client_->GetConfig(&config)) {
    Alert(Tr("The conversion engine (mozc_server) could not be started."));
    return false;
  }
  config.set_custom_keymap_table(Serialize());
  config.set_session_keymap(Config::CUSTOM);
  if (!client_->SetConfig(config)) {
    Alert(Tr("The settings could not be saved."));
    return false;
  }
  return true;
}

}  // namespace haiku
}  // namespace mozc
