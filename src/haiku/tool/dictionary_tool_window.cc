// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/tool/dictionary_tool_window.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <String.h>
#include <TextControl.h>

#include <string>

#include "absl/status/status.h"
#include "dictionary/user_pos.h"
#include "haiku/tool/tool_strings.h"

namespace mozc {
namespace haiku {
namespace {

enum : uint32 {
  kMsgDictionarySelected = 'Dsel',
  kMsgEntrySelected = 'Esel',
  kMsgNewDictionary = 'Dnew',
  kMsgRenameDictionary = 'Dren',
  kMsgDeleteDictionary = 'Ddel',
  kMsgNameEntered = 'Dnam',
  kMsgImport = 'Dimp',
  kMsgExport = 'Dexp',
  kMsgAddEntry = 'Eadd',
  kMsgUpdateEntry = 'Eupd',
  kMsgDeleteEntry = 'Edel',
  kMsgFormChanged = 'Fchg',
};

enum Column { kReadingColumn, kWordColumn, kPosColumn, kCommentColumn };

// A row of the entry list; it remembers the entry it shows, whatever the
// order the list is sorted in.
class EntryRow : public BRow {
 public:
  EntryRow(int index, const DictionaryStore::Entry& entry) : index_(index) {
    SetField(new BStringField(entry.key().c_str()), kReadingColumn);
    SetField(new BStringField(entry.value().c_str()), kWordColumn);
    SetField(new BStringField(
                 std::string(dictionary::UserPos::GetStringPosType(
                                 entry.pos()))
                     .c_str()),
             kPosColumn);
    SetField(new BStringField(entry.comment().c_str()), kCommentColumn);
  }
  int index() const { return index_; }

 private:
  const int index_;
};

// Asks for the name of a dictionary.
class NameWindow : public BWindow {
 public:
  NameWindow(BWindow* parent, const char* name, BMessage* reply)
      : BWindow(BRect(0, 0, 300, 80), "Mozc", B_MODAL_WINDOW_LOOK,
                B_MODAL_SUBSET_WINDOW_FEEL,
                B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
                    B_CLOSE_ON_ESCAPE),
        target_(parent),
        reply_(reply) {
    AddToSubset(parent);
    name_ = new BTextControl(Tr("Dictionary name:"), name, nullptr);
    BButton* ok = new BButton(Tr("OK"), new BMessage(B_OK));
    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(name_)
        .AddGroup(B_HORIZONTAL)
        .AddGlue()
        .Add(new BButton(Tr("Cancel"), new BMessage(B_QUIT_REQUESTED)))
        .Add(ok)
        .End();
    SetDefaultButton(ok);
    name_->MakeFocus(true);
    CenterIn(parent->Frame());
  }

  ~NameWindow() override { delete reply_; }

  void MessageReceived(BMessage* message) override {
    if (message->what == B_OK) {
      reply_->AddString("name", name_->Text());
      target_.SendMessage(reply_);
      Quit();
      return;
    }
    BWindow::MessageReceived(message);
  }

 private:
  BMessenger target_;
  BMessage* reply_;
  BTextControl* name_;
};

}  // namespace

DictionaryToolWindow::DictionaryToolWindow()
    : BWindow(BRect(80, 80, 760, 520), Tr("Mozc dictionary tool"),
              B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
      open_panel_(nullptr),
      save_panel_(nullptr) {
  BMenuBar* menu_bar = new BMenuBar("menu");
  BMenu* menu = new BMenu(Tr("Dictionaries"));
  menu->AddItem(new BMenuItem(Tr("New dictionary\xE2\x80\xA6"),
                              new BMessage(kMsgNewDictionary), 'N'));
  menu->AddItem(new BMenuItem(Tr("Rename dictionary\xE2\x80\xA6"),
                              new BMessage(kMsgRenameDictionary)));
  menu->AddItem(new BMenuItem(Tr("Delete dictionary"),
                              new BMessage(kMsgDeleteDictionary)));
  menu->AddSeparatorItem();
  menu->AddItem(new BMenuItem(Tr("Import\xE2\x80\xA6"),
                              new BMessage(kMsgImport), 'O'));
  menu->AddItem(new BMenuItem(Tr("Export\xE2\x80\xA6"),
                              new BMessage(kMsgExport), 'S'));
  menu->AddSeparatorItem();
  menu->AddItem(
      new BMenuItem(Tr("Close"), new BMessage(B_QUIT_REQUESTED), 'W'));
  menu_bar->AddItem(menu);

  dictionaries_ = new BListView("dictionaries");
  dictionaries_->SetSelectionMessage(new BMessage(kMsgDictionarySelected));

  entries_ = new BColumnListView("entries", B_NAVIGABLE, B_FANCY_BORDER);
  entries_->AddColumn(new BStringColumn(Tr("Reading"), 140, 50, 400,
                                        B_TRUNCATE_END),
                      kReadingColumn);
  entries_->AddColumn(
      new BStringColumn(Tr("Word"), 160, 50, 400, B_TRUNCATE_END),
      kWordColumn);
  entries_->AddColumn(new BStringColumn(Tr("Part of speech"), 110, 50, 300,
                                        B_TRUNCATE_END),
                      kPosColumn);
  entries_->AddColumn(
      new BStringColumn(Tr("Comment"), 150, 50, 600, B_TRUNCATE_END),
      kCommentColumn);
  entries_->SetSelectionMessage(new BMessage(kMsgEntrySelected));
  entries_->SetSelectionMode(B_SINGLE_SELECTION_LIST);

  reading_ = new BTextControl(Tr("Reading:"), "", nullptr);
  word_ = new BTextControl(Tr("Word:"), "", nullptr);
  comment_ = new BTextControl(Tr("Comment:"), "", nullptr);
  for (BTextControl* control : {reading_, word_, comment_}) {
    control->SetModificationMessage(new BMessage(kMsgFormChanged));
  }
  pos_menu_ = new BPopUpMenu("pos");
  for (const std::string& pos : DictionaryStore::PosNames()) {
    pos_menu_->AddItem(new BMenuItem(pos.c_str(), nullptr));
  }
  if (BMenuItem* item =
          pos_menu_->ItemAt(DictionaryStore::DefaultPosIndex())) {
    item->SetMarked(true);
  }
  add_button_ = new BButton(Tr("Add"), new BMessage(kMsgAddEntry));
  update_button_ = new BButton(Tr("Update"), new BMessage(kMsgUpdateEntry));
  delete_button_ = new BButton(Tr("Delete"), new BMessage(kMsgDeleteEntry));

  BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
      .Add(menu_bar)
      .AddGroup(B_VERTICAL)
      .SetInsets(B_USE_WINDOW_SPACING)
      .AddSplit(B_HORIZONTAL, B_USE_SMALL_SPACING, 1.0f)
      .Add(new BScrollView("dictionaries scroll", dictionaries_, 0, false,
                           true),
           0.25f)
      .Add(entries_, 0.75f)
      .End()
      .AddGrid()
      .AddTextControl(reading_, 0, 0)
      .AddTextControl(word_, 2, 0)
      .AddMenuField(new BMenuField(Tr("Part of speech:"), pos_menu_), 0, 1)
      .AddTextControl(comment_, 2, 1)
      .End()
      .AddGroup(B_HORIZONTAL)
      .AddGlue()
      .Add(delete_button_)
      .Add(update_button_)
      .Add(add_button_)
      .End()
      .End();

  if (absl::Status status = store_.Load(); !status.ok()) {
    Report(status);
    PostMessage(B_QUIT_REQUESTED);
    return;
  }
  FillDictionaries(0);
  UpdateButtons();
  CenterOnScreen();
}

DictionaryToolWindow::~DictionaryToolWindow() {
  delete open_panel_;
  delete save_panel_;
}

bool DictionaryToolWindow::QuitRequested() {
  be_app->PostMessage(B_QUIT_REQUESTED);
  return true;
}

void DictionaryToolWindow::MessageReceived(BMessage* message) {
  switch (message->what) {
    case kMsgDictionarySelected:
      FillEntries();
      break;
    case kMsgEntrySelected:
      EntrySelected();
      break;
    case kMsgFormChanged:
      UpdateButtons();
      break;
    case kMsgAddEntry: {
      DictionaryStore::Entry entry;
      if (ReadForm(&entry)) {
        Report(store_.AddEntry(CurrentDictionary(), entry));
        FillEntries();
      }
      break;
    }
    case kMsgUpdateEntry: {
      DictionaryStore::Entry entry;
      if (CurrentEntry() >= 0 && ReadForm(&entry)) {
        Report(store_.UpdateEntry(CurrentDictionary(), CurrentEntry(), entry));
        FillEntries();
      }
      break;
    }
    case kMsgDeleteEntry:
      if (CurrentEntry() >= 0) {
        Report(store_.DeleteEntry(CurrentDictionary(), CurrentEntry()));
        FillEntries();
      }
      break;
    case kMsgNewDictionary: {
      BMessage* reply = new BMessage(kMsgNameEntered);
      reply->AddBool("new", true);
      (new NameWindow(this, "", reply))->Show();
      break;
    }
    case kMsgRenameDictionary: {
      if (CurrentDictionary() < 0) {
        break;
      }
      BMessage* reply = new BMessage(kMsgNameEntered);
      reply->AddBool("new", false);
      (new NameWindow(this,
                      store_.dictionary(CurrentDictionary()).name().c_str(),
                      reply))
          ->Show();
      break;
    }
    case kMsgNameEntered: {
      const std::string name = message->GetString("name", "");
      if (message->GetBool("new", true)) {
        absl::StatusOr<int> index = store_.CreateDictionary(name);
        Report(index.status());
        FillDictionaries(index.ok() ? *index : CurrentDictionary());
      } else {
        const int current = CurrentDictionary();
        Report(store_.RenameDictionary(current, name));
        FillDictionaries(current);
      }
      break;
    }
    case kMsgDeleteDictionary: {
      const int current = CurrentDictionary();
      if (current < 0 || store_.size() <= 1) {
        break;
      }
      BString question(
          Tr("Delete the dictionary \"%s\" and all its words?"));
      question.ReplaceFirst("%s", store_.dictionary(current).name().c_str());
      BAlert* alert = new BAlert("Mozc", question, Tr("Cancel"), Tr("Delete"),
                                 nullptr, B_WIDTH_AS_USUAL,
                                 B_WARNING_ALERT);
      if (alert->Go() == 1) {
        Report(store_.DeleteDictionary(current));
        FillDictionaries(0);
      }
      break;
    }
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
      }
      if (CurrentDictionary() >= 0) {
        BString name(store_.dictionary(CurrentDictionary()).name().c_str());
        name << ".txt";
        save_panel_->SetSaveText(name);
        save_panel_->Show();
      }
      break;
    case B_REFS_RECEIVED: {
      entry_ref ref;
      if (message->FindRef("refs", &ref) != B_OK) {
        break;
      }
      BPath path(&ref);
      std::string name = ref.name;
      if (const size_t dot = name.rfind('.'); dot != std::string::npos &&
                                              dot > 0) {
        name.resize(dot);
      }
      absl::StatusOr<int> index = store_.Import(path.Path(), name);
      if (!index.ok()) {
        Report(index.status());
        // Lines were left out, but the dictionary was made.
        FillDictionaries(absl::IsDataLoss(index.status()) ? store_.size() - 1
                                                          : 0);
      } else {
        FillDictionaries(*index);
      }
      break;
    }
    case B_SAVE_REQUESTED: {
      entry_ref directory;
      const char* name = nullptr;
      if (message->FindRef("directory", &directory) != B_OK ||
          message->FindString("name", &name) != B_OK) {
        break;
      }
      BPath path(&directory);
      path.Append(name);
      Report(store_.Export(CurrentDictionary(), path.Path()));
      break;
    }
    default:
      BWindow::MessageReceived(message);
      break;
  }
}

int DictionaryToolWindow::CurrentDictionary() const {
  return dictionaries_->CurrentSelection();
}

int DictionaryToolWindow::CurrentEntry() const {
  const EntryRow* row = static_cast<EntryRow*>(entries_->CurrentSelection());
  return row != nullptr ? row->index() : -1;
}

void DictionaryToolWindow::FillDictionaries(int select) {
  while (BListItem* item = dictionaries_->RemoveItem(int32(0))) {
    delete item;
  }
  for (int i = 0; i < store_.size(); ++i) {
    dictionaries_->AddItem(
        new BStringItem(store_.dictionary(i).name().c_str()));
  }
  if (select < 0 || select >= store_.size()) {
    select = 0;
  }
  dictionaries_->Select(select);
  FillEntries();
}

void DictionaryToolWindow::FillEntries() {
  entries_->Clear();
  const int current = CurrentDictionary();
  if (current >= 0) {
    const auto& dictionary = store_.dictionary(current);
    for (int i = 0; i < dictionary.entries_size(); ++i) {
      entries_->AddRow(new EntryRow(i, dictionary.entries(i)));
    }
  }
  UpdateButtons();
}

void DictionaryToolWindow::EntrySelected() {
  const int index = CurrentEntry();
  if (index >= 0) {
    const auto& entry = store_.dictionary(CurrentDictionary()).entries(index);
    reading_->SetText(entry.key().c_str());
    word_->SetText(entry.value().c_str());
    comment_->SetText(entry.comment().c_str());
    const std::string pos(dictionary::UserPos::GetStringPosType(entry.pos()));
    if (BMenuItem* item = pos_menu_->FindItem(pos.c_str())) {
      item->SetMarked(true);
    }
  }
  UpdateButtons();
}

void DictionaryToolWindow::UpdateButtons() {
  const bool form_filled =
      reading_->Text()[0] != '\0' && word_->Text()[0] != '\0';
  add_button_->SetEnabled(form_filled && CurrentDictionary() >= 0);
  update_button_->SetEnabled(form_filled && CurrentEntry() >= 0);
  delete_button_->SetEnabled(CurrentEntry() >= 0);
}

bool DictionaryToolWindow::ReadForm(DictionaryStore::Entry* entry) {
  BMenuItem* pos = pos_menu_->FindMarked();
  absl::Status status = DictionaryStore::MakeEntry(
      reading_->Text(), word_->Text(), pos != nullptr ? pos->Label() : "",
      comment_->Text(), entry);
  Report(status);
  return status.ok();
}

void DictionaryToolWindow::Report(const absl::Status& status) {
  if (status.ok()) {
    return;
  }
  BAlert* alert = new BAlert("Mozc", std::string(status.message()).c_str(),
                             Tr("OK"), nullptr, nullptr, B_WIDTH_AS_USUAL,
                             B_STOP_ALERT);
  alert->Go(nullptr);
}

}  // namespace haiku
}  // namespace mozc
