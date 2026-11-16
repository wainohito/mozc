// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_DESKBAR_MENU_H_
#define MOZC_HAIKU_INPUT_METHOD_DESKBAR_MENU_H_

#include <memory>

#include "haiku/input_method/key_translator.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

class BMenu;

namespace mozc {
namespace haiku {

struct MenuState {
  commands::CompositionMode mode = commands::DIRECT;
  // False while the conversion engine cannot be reached; the settings are
  // then left out.
  bool has_config = false;
  config::Config config;
  KanaLayout kana_layout = KanaLayout::kJis;
};

// Builds the menu the input_server shows for Mozc in the Deskbar: the input
// modes, the settings that matter day to day, and the tools.
std::unique_ptr<BMenu> BuildDeskbarMenu(const MenuState& state);

// Applies a kMsgSetConfig choice to `config`. Returns false if the message
// is not understood.
bool ApplyConfigChoice(int32_t field, int32_t value, config::Config* config);

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_DESKBAR_MENU_H_
