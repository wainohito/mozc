// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_MODE_ICON_H_
#define MOZC_HAIKU_INPUT_METHOD_MODE_ICON_H_

#include <SupportDefs.h>

#include <array>

#include "protocol/commands.pb.h"

namespace mozc {
namespace haiku {

// The input_server wants 16x16 B_CMAP8 icons for input methods.
inline constexpr int kModeIconSize = 16;
using ModeIcon = std::array<uint8, kModeIconSize * kModeIconSize>;

// Draws the Deskbar icon of a composition mode: a key cap with the mode's
// character on it (あ, ア, Ａ, ｱ, _A), or a plain A for direct input.
// Must be called from a thread that may talk to the app_server.
ModeIcon MakeModeIcon(commands::CompositionMode mode);

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_MODE_ICON_H_
