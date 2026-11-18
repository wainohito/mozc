// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_MOZC_LOOPER_H_
#define MOZC_HAIKU_INPUT_METHOD_MOZC_LOOPER_H_

#include <Looper.h>
#include <OS.h>
#include <Point.h>

#include <memory>
#include <string>
#include <vector>

#include "client/client_interface.h"
#include "haiku/input_method/key_translator.h"
#include "haiku/input_method/method_host.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

class BMessageRunner;

namespace mozc {
namespace haiku {

class CandidateWindow;

// Does the work of the Mozc input method, in its own thread so that the
// input_server never waits for the conversion engine.
//
// Key presses come from MozcMethod::Filter(). They are sent to mozc_server;
// what it answers is turned into B_INPUT_METHOD_EVENT messages for the
// application, the candidate window, and the Deskbar icon and menu. Keys
// Mozc has no use for go back to the input_server unchanged.
class MozcLooper : public BLooper {
 public:
  explicit MozcLooper(MethodHost* method);
  ~MozcLooper() override;

  void MessageReceived(BMessage* message) override;
  bool QuitRequested() override;

 private:
  void HandleKeyDown(BMessage* message);
  void HandleActivated(bool active);
  void HandleApplicationEvent(BMessage* message);
  void HandleOutput(const commands::Output& output);
  void UpdateMode(const commands::Output& output);
  bool SendCommand(commands::SessionCommand::CommandType type,
                   commands::Output* output);

  // B_INPUT_METHOD_EVENT messages for the application.
  void StartComposition();
  void StopComposition();
  void Commit(const std::string& text);
  void UpdatePreedit(const commands::Preedit& preedit);
  void ClearPreedit();
  void RequestLocation();
  void Enqueue(BMessage* message);

  void UpdateCandidates(const commands::Output& output);
  void ShowCandidatesAtCaret();

  void SetMode(commands::CompositionMode mode);
  void UpdateDeskbar();
  void RefreshConfig();
  void ApplyConfig(const config::Config& config);
  void ScheduleCallback(const commands::Output::Callback& callback);
  void SyncData(bool force);
  void LoadSettings();
  void SaveSettings() const;

  MethodHost* method_;  // Not owned.
  std::unique_ptr<client::ClientInterface> client_;
  KeyTranslator translator_;
  CandidateWindow* candidate_window_;
  BMessageRunner* callback_runner_;

  bool active_;     // The method is the active one of the input_server.
  bool composing_;  // B_INPUT_METHOD_STARTED was sent and not stopped.
  bool has_preedit_;
  commands::CompositionMode mode_;
  commands::CompositionMode shown_mode_;  // The mode the Deskbar shows.

  bool has_config_;
  config::Config config_;
  KanaLayout kana_layout_;

  // Where the application draws the characters of the preedit, as told by
  // the latest B_INPUT_METHOD_LOCATION_REQUEST reply.
  std::vector<BPoint> locations_;
  std::vector<float> heights_;
  bool has_candidates_;
  commands::CandidateWindow candidates_;

  bigtime_t last_sync_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_MOZC_LOOPER_H_
