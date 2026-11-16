// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_TOOL_TOOL_STRINGS_H_
#define MOZC_HAIKU_TOOL_TOOL_STRINGS_H_

namespace mozc {
namespace haiku {

// Returns the Japanese translation of `english` when Japanese is the
// preferred language of the user, `english` otherwise.
const char* Tr(const char* english);

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_TOOL_TOOL_STRINGS_H_
