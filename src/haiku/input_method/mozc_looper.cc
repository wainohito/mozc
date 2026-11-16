// Copyright 2010-2016, Google Inc.
// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Ported-from: Mozc for Haiku (hanya/mozc v2.26.4451.1, src/haiku/input_method)
// Ported-from-license: BSD-3-Clause

#include "haiku/input_method/mozc_looper.h"

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <NodeMonitor.h>
#include <Input.h>
#include <Menu.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <add-ons/input_server/InputServerMethod.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/system_util.h"
#include "client/client.h"
#include "haiku/input_method/candidate_window.h"
#include "haiku/input_method/deskbar_menu.h"
#include "haiku/input_method/messages.h"
#include "haiku/input_method/mode_icon.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace haiku {
namespace {

using commands::CompositionMode;
using commands::Output;
using commands::SessionCommand;

enum : uint32 {
  kMsgCallback = 'MZcb',
};

constexpr bigtime_t kSyncInterval = 5 * 60 * 1000000LL;
constexpr char kSettingsFile[] = "haiku_input_method.settings";
// Where mozc_server keeps the configuration, in the user profile directory.
constexpr char kConfigFile[] = "config1.db";

void InitMozcOnce() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Logs go to mozc_input_method.log in the user profile directory.
    char arg0[] = "mozc_input_method";
    char* argv_storage[] = {arg0, nullptr};
    char** argv = argv_storage;
    int argc = 1;
    InitMozc(arg0, &argc, &argv);
  });
}

// Returns the number of bytes of the first `chars` characters of `text`.
size_t Utf8Bytes(const std::string& text, size_t chars) {
  size_t bytes = 0;
  while (bytes < text.size() && chars > 0) {
    ++bytes;
    while (bytes < text.size() && (text[bytes] & 0xc0) == 0x80) {
      ++bytes;
    }
    --chars;
  }
  return bytes;
}

std::string SettingsPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kSettingsFile);
}

}  // namespace

MozcLooper::MozcLooper(BInputServerMethod* method)
    : BLooper("Mozc input method"),
      method_(method),
      candidate_window_(nullptr),
      callback_runner_(nullptr),
      active_(false),
      composing_(false),
      has_preedit_(false),
      mode_(commands::DIRECT),
      shown_mode_(commands::NUM_OF_COMPOSITIONS),
      has_config_(false),
      kana_layout_(KanaLayout::kJis),
      has_candidates_(false),
      last_sync_(system_time()) {
  InitMozcOnce();
  client_ = client::ClientFactory::NewClient();
  // The input_server must never be stopped by an error dialog.
  client_->set_suppress_error_dialog(true);
  LoadSettings();
  translator_.set_kana_layout(kana_layout_);
  candidate_window_ = new CandidateWindow(BMessenger(this));

  // The tools change the configuration too (the keymap editor); follow it
  // so that the Deskbar menu shows what is in effect.
  const std::string profile = SystemUtil::GetUserProfileDirectory();
  FileUtil::CreateDirectory(profile).IgnoreError();
  node_ref profile_node;
  if (BEntry(profile.c_str()).GetNodeRef(&profile_node) == B_OK) {
    watch_node(&profile_node, B_WATCH_DIRECTORY, this);
  }
}

MozcLooper::~MozcLooper() {
  stop_watching(this);
  delete callback_runner_;
  if (candidate_window_->Lock()) {
    candidate_window_->Quit();
  }
}

bool MozcLooper::QuitRequested() {
  SyncData(true);
  return true;
}

void MozcLooper::MessageReceived(BMessage* message) {
  switch (message->what) {
    case B_KEY_DOWN:
    case B_UNMAPPED_KEY_DOWN:
      HandleKeyDown(message);
      break;
    case B_INPUT_METHOD_EVENT:
      HandleApplicationEvent(message);
      break;
    case B_NODE_MONITOR:
      if (message->GetString("name", "") == std::string_view(kConfigFile)) {
        RefreshConfig();
        UpdateDeskbar();
      }
      break;
    case kMsgMethodLoaded:
      UpdateDeskbar();
      break;
    case kMsgMethodActivated:
      HandleActivated(message->GetBool("active", false));
      break;
    case kMsgSetMode:
      SetMode(static_cast<CompositionMode>(
          message->GetInt32("mode", commands::HIRAGANA)));
      break;
    case kMsgSetConfig: {
      RefreshConfig();
      if (!has_config_) {
        break;
      }
      config::Config config = config_;
      if (ApplyConfigChoice(message->GetInt32("field", -1),
                            message->GetInt32("value", 0), &config)) {
        if (client_->SetConfig(config)) {
          ApplyConfig(config);
        }
      }
      UpdateDeskbar();
      break;
    }
    case kMsgSetKanaLayout:
      kana_layout_ = message->GetInt32("layout", 0) == 1 ? KanaLayout::kUs
                                                         : KanaLayout::kJis;
      translator_.set_kana_layout(kana_layout_);
      SaveSettings();
      UpdateDeskbar();
      break;
    case kMsgLaunchTool: {
      const char* mode = message->GetString("mode", nullptr);
      if (mode != nullptr) {
        client_->LaunchTool(mode, "");
      }
      // The Deskbar shows the menu it was given; give it again so that
      // it keeps working after this choice.
      UpdateDeskbar();
      break;
    }
    case kMsgSelectCandidate: {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(message->GetInt32("id", 0));
      Output output;
      if (client_->SendCommand(command, &output)) {
        HandleOutput(output);
      }
      break;
    }
    case kMsgCandidatePage: {
      Output output;
      if (SendCommand(message->GetBool("forward", true)
                          ? SessionCommand::CONVERT_NEXT_PAGE
                          : SessionCommand::CONVERT_PREV_PAGE,
                      &output)) {
        HandleOutput(output);
      }
      break;
    }
    case kMsgCallback: {
      const void* data = nullptr;
      ssize_t size = 0;
      SessionCommand command;
      if (message->FindData("command", B_RAW_TYPE, &data, &size) == B_OK &&
          command.ParseFromArray(data, size)) {
        Output output;
        if (client_->SendCommand(command, &output)) {
          HandleOutput(output);
        }
      }
      break;
    }
    default:
      BLooper::MessageReceived(message);
      break;
  }
}

void MozcLooper::HandleKeyDown(BMessage* message) {
  HaikuKey key;
  key.key = message->GetInt32("key", 0);
  key.modifiers = static_cast<uint32_t>(message->GetInt32("modifiers", 0));
  key.bytes = message->GetString("bytes", "");
  key.raw_char = message->GetInt32("raw_char", 0);

  // Shortcuts belong to the application unless they would change text
  // being composed.
  const bool shortcut = (key.modifiers & B_COMMAND_KEY) != 0;
  commands::KeyEvent event;
  Output output;
  if (!(shortcut && !has_preedit_) && translator_.Translate(key, &event) &&
      client_->SendKey(event, &output)) {
    if (output.consumed()) {
      HandleOutput(output);
      SyncData(false);
      if (!has_config_) {
        // The engine was not running when the method was activated.
        RefreshConfig();
        UpdateDeskbar();
      }
      return;
    }
    UpdateMode(output);
  }
  Enqueue(new BMessage(*message));
}

void MozcLooper::HandleActivated(bool active) {
  active_ = active;
  Output output;
  if (active) {
    // TURN_ON_IME starts the conversion engine if needed; the settings
    // can only be read from it after that.
    if (SendCommand(SessionCommand::TURN_ON_IME, &output)) {
      HandleOutput(output);
    } else {
      mode_ = commands::DIRECT;
    }
    RefreshConfig();
    UpdateDeskbar();
    return;
  }

  // Keep what was being typed when the user switches to another method.
  if (has_preedit_ && SendCommand(SessionCommand::SUBMIT, &output)) {
    HandleOutput(output);
  }
  if (composing_) {
    ClearPreedit();
    StopComposition();
  }
  candidate_window_->HideCandidates();
  has_candidates_ = false;
  SyncData(true);
}

void MozcLooper::HandleApplicationEvent(BMessage* message) {
  // Replies of the application to the messenger given in
  // B_INPUT_METHOD_STARTED.
  switch (message->GetInt32("be:opcode", 0)) {
    case B_INPUT_METHOD_LOCATION_REQUEST: {
      locations_.clear();
      heights_.clear();
      BPoint where;
      float height;
      for (int32 i = 0;
           message->FindPoint("be:location_reply", i, &where) == B_OK &&
           message->FindFloat("be:height_reply", i, &height) == B_OK;
           ++i) {
        locations_.push_back(where);
        heights_.push_back(height);
      }
      if (has_candidates_) {
        ShowCandidatesAtCaret();
      }
      break;
    }
    case B_INPUT_METHOD_STOPPED: {
      // The application ended the composition (the user clicked elsewhere,
      // for example). It keeps the text it shows, so Mozc forgets it.
      Output output;
      SendCommand(SessionCommand::REVERT, &output);
      composing_ = false;
      has_preedit_ = false;
      has_candidates_ = false;
      candidate_window_->HideCandidates();
      break;
    }
  }
}

void MozcLooper::UpdateMode(const Output& output) {
  if (!output.has_status()) {
    return;
  }
  const commands::Status& status = output.status();
  const CompositionMode mode =
      status.activated() ? status.mode() : commands::DIRECT;
  if (mode != mode_) {
    mode_ = mode;
    UpdateDeskbar();
  }
}

void MozcLooper::HandleOutput(const Output& output) {
  UpdateMode(output);

  // A result and a new preedit can come together (typing on after a
  // conversion): commit first, then compose again.
  if (output.has_result() &&
      output.result().type() != commands::Result::NONE) {
    Commit(output.result().value());
  }
  if (output.has_preedit() && output.preedit().segment_size() > 0) {
    UpdatePreedit(output.preedit());
  } else if (has_preedit_) {
    ClearPreedit();
    StopComposition();
  }

  UpdateCandidates(output);

  if (output.has_launch_tool_mode() &&
      output.launch_tool_mode() != Output::NO_TOOL) {
    client_->LaunchToolWithProtoBuf(output);
  }
  if (output.has_url() && !output.url().empty()) {
    client_->OpenBrowser(output.url());
  }
  if (output.has_callback()) {
    ScheduleCallback(output.callback());
  }
}

bool MozcLooper::SendCommand(SessionCommand::CommandType type,
                             Output* output) {
  SessionCommand command;
  command.set_type(type);
  return client_->SendCommand(command, output);
}

void MozcLooper::StartComposition() {
  if (composing_) {
    return;
  }
  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_STARTED);
  message->AddMessenger("be:reply_to", BMessenger(this));
  Enqueue(message);
  composing_ = true;
  locations_.clear();
  heights_.clear();
}

void MozcLooper::StopComposition() {
  if (!composing_) {
    return;
  }
  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
  Enqueue(message);
  composing_ = false;
  has_preedit_ = false;
}

void MozcLooper::Commit(const std::string& text) {
  StartComposition();
  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
  message->AddString("be:string", text.c_str());
  message->AddBool("be:confirmed", true);
  Enqueue(message);
  has_preedit_ = false;
  StopComposition();
}

void MozcLooper::UpdatePreedit(const commands::Preedit& preedit) {
  const bool was_composing = composing_;
  StartComposition();

  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
  std::string text;
  int32 selection_start = -1;
  int32 selection_end = -1;
  for (const auto& segment : preedit.segment()) {
    const int32 start = static_cast<int32>(text.size());
    text += segment.value();
    const int32 end = static_cast<int32>(text.size());
    message->AddInt32("be:clause_start", start);
    message->AddInt32("be:clause_end", end);
    if (segment.annotation() == commands::Preedit::Segment::HIGHLIGHT &&
        selection_start < 0) {
      selection_start = start;
      selection_end = end;
    }
  }
  if (selection_start < 0) {
    // No clause is being converted: the selection is the caret.
    selection_start = selection_end =
        static_cast<int32>(Utf8Bytes(text, preedit.cursor()));
  }
  message->AddInt32("be:selection", selection_start);
  message->AddInt32("be:selection", selection_end);
  message->AddString("be:string", text.c_str());
  Enqueue(message);
  has_preedit_ = true;

  if (!was_composing) {
    // Where to show candidates, asked once the preedit is on screen.
    RequestLocation();
  }
}

void MozcLooper::ClearPreedit() {
  if (!composing_) {
    return;
  }
  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
  message->AddString("be:string", "");
  Enqueue(message);
  has_preedit_ = false;
}

void MozcLooper::RequestLocation() {
  BMessage* message = new BMessage(B_INPUT_METHOD_EVENT);
  message->AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
  Enqueue(message);
}

void MozcLooper::Enqueue(BMessage* message) {
  if (method_->EnqueueMessage(message) != B_OK) {
    delete message;
  }
}

void MozcLooper::UpdateCandidates(const Output& output) {
  if (!output.has_candidate_window() ||
      output.candidate_window().candidate_size() == 0) {
    if (has_candidates_) {
      candidate_window_->HideCandidates();
      has_candidates_ = false;
    }
    return;
  }
  const uint32 old_position =
      has_candidates_ ? candidates_.position() : UINT32_MAX;
  candidates_ = output.candidate_window();
  has_candidates_ = true;
  if (locations_.empty() || candidates_.position() != old_position) {
    // The application tells where the text is; the window is shown when it
    // answers. In the meantime the last known place is good enough.
    RequestLocation();
  }
  if (!locations_.empty()) {
    ShowCandidatesAtCaret();
  }
}

void MozcLooper::ShowCandidatesAtCaret() {
  if (locations_.empty()) {
    return;
  }
  const size_t index =
      std::min<size_t>(candidates_.position(), locations_.size() - 1);
  candidate_window_->ShowCandidates(candidates_, locations_[index],
                                    heights_[index]);
}

void MozcLooper::SetMode(CompositionMode mode) {
  Output output;
  bool sent = false;
  if (mode == commands::DIRECT) {
    if (has_preedit_) {
      SendCommand(SessionCommand::SUBMIT, &output);
      HandleOutput(output);
      output.Clear();
    }
    sent = SendCommand(SessionCommand::TURN_OFF_IME, &output);
  } else {
    SessionCommand command;
    command.set_type(mode_ == commands::DIRECT ? SessionCommand::TURN_ON_IME
                                               : SessionCommand::SWITCH_COMPOSITION_MODE);
    command.set_composition_mode(mode);
    sent = client_->SendCommand(command, &output);
    if (sent && command.type() == SessionCommand::TURN_ON_IME &&
        output.status().mode() != mode) {
      // TURN_ON_IME comes back in the previous mode.
      command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
      output.Clear();
      sent = client_->SendCommand(command, &output);
    }
  }
  if (sent) {
    HandleOutput(output);
  }
  // Always give the menu back; see kMsgLaunchTool.
  UpdateDeskbar();
}

void MozcLooper::UpdateDeskbar() {
  // Before the first activation the icon shows what typing will give.
  const CompositionMode icon_mode = active_ ? mode_ : commands::HIRAGANA;
  if (shown_mode_ != icon_mode) {
    const ModeIcon icon = MakeModeIcon(icon_mode);
    method_->SetIcon(icon.data());
    shown_mode_ = icon_mode;
  }
  if (!active_) {
    return;
  }
  // The tools change the configuration too (the keymap editor).
  RefreshConfig();
  MenuState state;
  state.mode = mode_;
  state.has_config = has_config_;
  state.config = config_;
  state.kana_layout = kana_layout_;
  std::unique_ptr<BMenu> menu = BuildDeskbarMenu(state);
  // The input_server archives the menu; it does not keep it.
  method_->SetMenu(menu.get(), BMessenger(this));
}

void MozcLooper::RefreshConfig() {
  config::Config config;
  if (client_->GetConfig(&config)) {
    ApplyConfig(config);
  } else {
    has_config_ = false;
  }
}

void MozcLooper::ApplyConfig(const config::Config& config) {
  config_ = config;
  has_config_ = true;
  translator_.set_kana_input(config.preedit_method() == config::Config::KANA);
}

void MozcLooper::ScheduleCallback(const Output::Callback& callback) {
  delete callback_runner_;
  callback_runner_ = nullptr;
  if (!callback.has_session_command()) {
    return;
  }
  BMessage message(kMsgCallback);
  const std::string data = callback.session_command().SerializeAsString();
  message.AddData("command", B_RAW_TYPE, data.data(), data.size());
  callback_runner_ = new BMessageRunner(
      BMessenger(this), &message,
      static_cast<bigtime_t>(callback.delay_millisec()) * 1000, 1);
}

void MozcLooper::SyncData(bool force) {
  const bigtime_t now = system_time();
  if (force || now - last_sync_ > kSyncInterval) {
    client_->SyncData();
    last_sync_ = now;
  }
}

void MozcLooper::LoadSettings() {
  BFile file(SettingsPath().c_str(), B_READ_ONLY);
  BMessage settings;
  if (file.InitCheck() == B_OK && settings.Unflatten(&file) == B_OK) {
    kana_layout_ = settings.GetInt32("kana_layout", 0) == 1 ? KanaLayout::kUs
                                                             : KanaLayout::kJis;
  }
}

void MozcLooper::SaveSettings() const {
  BMessage settings;
  settings.AddInt32("kana_layout", kana_layout_ == KanaLayout::kUs ? 1 : 0);
  BFile file(SettingsPath().c_str(),
             B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  if (file.InitCheck() == B_OK) {
    settings.Flatten(&file);
  }
}

}  // namespace haiku
}  // namespace mozc
