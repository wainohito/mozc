// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_MESSAGES_H_
#define MOZC_HAIKU_INPUT_METHOD_MESSAGES_H_

#include <SupportDefs.h>

namespace mozc {
namespace haiku {

// Messages of the Deskbar menu, the candidate window and the method, all
// handled by the MozcLooper.
enum : uint32 {
  // Deskbar menu: "mode" (int32, commands::CompositionMode).
  kMsgSetMode = 'MZmd',
  // Deskbar menu: "field" (int32, ConfigField), "value" (int32).
  kMsgSetConfig = 'MZcf',
  // Deskbar menu: "layout" (int32, KanaLayout).
  kMsgSetKanaLayout = 'MZkl',
  // Deskbar menu: "mode" (string, the --mode of mozc_tool).
  kMsgLaunchTool = 'MZtl',
  // Candidate window: "id" (int32, the candidate id).
  kMsgSelectCandidate = 'MZsc',
  // Candidate window: the mouse wheel; "forward" (bool).
  kMsgCandidatePage = 'MZpg',
  // Method: the method was (de)activated; "active" (bool).
  kMsgMethodActivated = 'MZac',
  // Method: the add-on was loaded.
  kMsgMethodLoaded = 'MZld',
};

// Settings of the Mozc configuration offered in the Deskbar menu.
enum class ConfigField : int32 {
  kPreeditMethod,
  kPunctuationMethod,
  kSymbolMethod,
  kSpaceCharacterForm,
  kSessionKeymap,
  kSelectionShortcut,
  kUseHistorySuggest,
  kUseDictionarySuggest,
  kUseRealtimeConversion,
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_MESSAGES_H_
