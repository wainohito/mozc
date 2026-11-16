// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/tool/word_register_window.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>

#include <string>

#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/environ.h"
#include "haiku/tool/tool_strings.h"

namespace mozc {
namespace haiku {
namespace {

enum : uint32 {
  kMsgRegister = 'Wreg',
  kMsgChanged = 'Wchg',
};

void ShowError(const absl::Status& status) {
  BAlert* alert = new BAlert("Mozc", std::string(status.message()).c_str(),
                             Tr("OK"), nullptr, nullptr, B_WIDTH_AS_USUAL,
                             B_STOP_ALERT);
  alert->Go();
}

}  // namespace

WordRegisterWindow::WordRegisterWindow()
    : BWindow(BRect(100, 100, 420, 260), Tr("Add word"), B_TITLED_WINDOW,
              B_NOT_ZOOMABLE | B_NOT_V_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS |
                  B_CLOSE_ON_ESCAPE) {
  word_ = new BTextControl(Tr("Word:"), "", new BMessage(kMsgChanged));
  reading_ = new BTextControl(Tr("Reading:"), "", new BMessage(kMsgChanged));
  word_->SetModificationMessage(new BMessage(kMsgChanged));
  reading_->SetModificationMessage(new BMessage(kMsgChanged));

  pos_menu_ = new BPopUpMenu("pos");
  for (const std::string& pos : DictionaryStore::PosNames()) {
    pos_menu_->AddItem(new BMenuItem(pos.c_str(), nullptr));
  }
  if (BMenuItem* item =
          pos_menu_->ItemAt(DictionaryStore::DefaultPosIndex())) {
    item->SetMarked(true);
  }
  dictionary_menu_ = new BPopUpMenu("dictionary");
  BMenuField* pos_field = new BMenuField(Tr("Part of speech:"), pos_menu_);
  BMenuField* dictionary_field =
      new BMenuField(Tr("Dictionary:"), dictionary_menu_);

  add_button_ = new BButton(Tr("Add"), new BMessage(kMsgRegister));
  add_button_->SetEnabled(false);
  BButton* cancel = new BButton(Tr("Cancel"), new BMessage(B_QUIT_REQUESTED));

  BLayoutBuilder::Group<>(this, B_VERTICAL)
      .SetInsets(B_USE_WINDOW_SPACING)
      .AddGrid()
      .AddTextControl(word_, 0, 0)
      .AddTextControl(reading_, 0, 1)
      .AddMenuField(pos_field, 0, 2)
      .AddMenuField(dictionary_field, 0, 3)
      .End()
      .AddGroup(B_HORIZONTAL)
      .AddGlue()
      .Add(cancel)
      .Add(add_button_)
      .End();
  SetDefaultButton(add_button_);

  // The conversion engine passes the selected text, if any.
  word_->SetText(Environ::GetEnv(kWordRegisterEnvironmentName).c_str());
  reading_->SetText(
      Environ::GetEnv(kWordRegisterEnvironmentReadingName).c_str());
  (word_->Text()[0] == '\0' ? word_ : reading_)->MakeFocus(true);

  if (absl::Status status = store_.Load(); !status.ok()) {
    ShowError(status);
    PostMessage(B_QUIT_REQUESTED);
    return;
  }
  for (int i = 0; i < store_.size(); ++i) {
    dictionary_menu_->AddItem(
        new BMenuItem(store_.dictionary(i).name().c_str(), nullptr));
  }
  dictionary_menu_->ItemAt(0)->SetMarked(true);
  PostMessage(kMsgChanged);
  CenterOnScreen();
}

void WordRegisterWindow::MessageReceived(BMessage* message) {
  switch (message->what) {
    case kMsgChanged:
      add_button_->SetEnabled(word_->Text()[0] != '\0' &&
                              reading_->Text()[0] != '\0');
      break;
    case kMsgRegister:
      Register();
      break;
    default:
      BWindow::MessageReceived(message);
      break;
  }
}

bool WordRegisterWindow::QuitRequested() {
  be_app->PostMessage(B_QUIT_REQUESTED);
  return true;
}

void WordRegisterWindow::Register() {
  BMenuItem* pos = pos_menu_->FindMarked();
  const int dictionary =
      dictionary_menu_->IndexOf(dictionary_menu_->FindMarked());
  DictionaryStore::Entry entry;
  absl::Status status = DictionaryStore::MakeEntry(
      reading_->Text(), word_->Text(), pos != nullptr ? pos->Label() : "", "",
      &entry);
  if (status.ok()) {
    status = store_.AddEntry(dictionary < 0 ? 0 : dictionary, entry);
  }
  if (!status.ok()) {
    ShowError(status);
    return;
  }
  PostMessage(B_QUIT_REQUESTED);
}

}  // namespace haiku
}  // namespace mozc
