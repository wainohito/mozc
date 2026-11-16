// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// mozc_tool for Haiku: the windows the input method opens from the Deskbar
// menu, and the error messages of the client library.
//
//   mozc_tool --mode=word_register_dialog
//   mozc_tool --mode=dictionary_tool
//   mozc_tool --mode=about_dialog
//   mozc_tool --mode=error_message_dialog --error_type=<type>

#include <Alert.h>
#include <Application.h>
#include <Roster.h>
#include <String.h>

#include <string>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "base/version.h"
#include "haiku/tool/dictionary_tool_window.h"
#include "haiku/tool/tool_strings.h"
#include "haiku/tool/word_register_window.h"

ABSL_FLAG(std::string, mode, "about_dialog", "mozc_tool mode");
ABSL_FLAG(std::string, error_type, "", "type of the error message");

namespace mozc {
namespace haiku {
namespace {

constexpr char kSignaturePrefix[] = "application/x-vnd.Mozc-Tool-";

class ToolApp : public BApplication {
 public:
  ToolApp(const char* signature, std::string mode)
      : BApplication(signature), mode_(std::move(mode)) {}

  void ReadyToRun() override {
    if (mode_ == "word_register_dialog") {
      (new WordRegisterWindow())->Show();
    } else if (mode_ == "dictionary_tool") {
      (new DictionaryToolWindow())->Show();
    } else if (mode_ == "error_message_dialog") {
      ShowError(absl::GetFlag(FLAGS_error_type));
      Quit();
    } else {
      ShowAbout();
      Quit();
    }
  }

 private:
  static void ShowAbout() {
    BString text("Mozc\n");
    text << Tr("Japanese input method") << "\n\n"
         << Tr("Version") << " " << Version::GetMozcVersion().c_str()
         << "\n\n"
         << Tr("Mozc is open source software of Google, ported to RenkuOS.")
         << "\n\nhttps://github.com/google/mozc";
    BAlert* alert = new BAlert("Mozc", text, Tr("OK"));
    alert->Go();
  }

  static void ShowError(const std::string& type) {
    const char* message = Tr("The conversion engine stopped unexpectedly.");
    if (type == "server_timeout" || type == "server_broken_message") {
      message = Tr("The conversion engine (mozc_server) could not be started.");
    } else if (type == "server_version_mismatch") {
      message = Tr(
          "The conversion engine is of a different version. Log out and in "
          "again, or restart the input_server.");
    }
    BAlert* alert = new BAlert("Mozc", message, Tr("OK"), nullptr, nullptr,
                               B_WIDTH_AS_USUAL, B_STOP_ALERT);
    alert->Go();
  }

  const std::string mode_;
};

}  // namespace
}  // namespace haiku
}  // namespace mozc

int main(int argc, char* argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);
  const std::string mode = absl::GetFlag(FLAGS_mode);

  // One window per tool: bring an open one to the front instead.
  const std::string signature = mozc::haiku::kSignaturePrefix + mode;
  if (be_roster->IsRunning(signature.c_str())) {
    be_roster->ActivateApp(be_roster->TeamFor(signature.c_str()));
    return 0;
  }
  mozc::haiku::ToolApp app(signature.c_str(), mode);
  app.Run();
  return 0;
}
