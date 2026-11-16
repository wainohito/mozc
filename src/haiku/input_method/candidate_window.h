// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_CANDIDATE_WINDOW_H_
#define MOZC_HAIKU_INPUT_METHOD_CANDIDATE_WINDOW_H_

#include <Messenger.h>
#include <Point.h>
#include <Window.h>

#include "protocol/candidate_window.pb.h"

namespace mozc {
namespace haiku {

class CandidateView;

// The list of candidates, shown next to the text being converted. It never
// takes the focus away from the application the user types in.
//
// All methods may be called from any thread.
class CandidateWindow : public BWindow {
 public:
  // Selections and page changes are sent to `target`.
  explicit CandidateWindow(BMessenger target);

  // Shows `candidates` below (or above) the text at `caret`, whose line is
  // `line_height` high, in screen coordinates.
  void ShowCandidates(const commands::CandidateWindow& candidates,
                      BPoint caret, float line_height);
  // Moves the window once the application has told where the text is.
  void MoveToCaret(BPoint caret, float line_height);
  void HideCandidates();

  void MessageReceived(BMessage* message) override;

 private:
  void Place(BPoint caret, float line_height);

  CandidateView* view_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_CANDIDATE_WINDOW_H_
