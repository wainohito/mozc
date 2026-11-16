// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/candidate_window.h"

#include <Font.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Screen.h>
#include <String.h>
#include <View.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "haiku/input_method/messages.h"
#include "protocol/candidate_window.pb.h"

namespace mozc {
namespace haiku {
namespace {

enum : uint32 {
  kMsgShow = 'MZcs',
  kMsgMove = 'MZcm',
  kMsgHide = 'MZch',
};

constexpr float kPadding = 3;
constexpr float kColumnGap = 8;

}  // namespace

class CandidateView : public BView {
 public:
  CandidateView(BRect frame, BMessenger target)
      : BView(frame, "candidates", B_FOLLOW_ALL,
              B_WILL_DRAW | B_FRAME_EVENTS),
        target_(target) {
    SetViewUIColor(B_MENU_BACKGROUND_COLOR);
    SetLowUIColor(B_MENU_BACKGROUND_COLOR);
    small_font_ = *be_plain_font;
    small_font_.SetSize(be_plain_font->Size() * 0.85f);
  }

  void SetCandidates(const commands::CandidateWindow& candidates) {
    candidates_ = candidates;
    Invalidate();
  }

  BSize PreferredSize() const {
    font_height height;
    be_plain_font->GetHeight(&height);
    const float row = RowHeight();
    float shortcut_width = 0;
    float value_width = 0;
    float description_width = 0;
    for (const auto& candidate : candidates_.candidate()) {
      const auto& annotation = candidate.annotation();
      shortcut_width = std::max(
          shortcut_width, be_plain_font->StringWidth(
                              annotation.shortcut().c_str()));
      value_width = std::max(value_width,
                             be_plain_font->StringWidth(Value(candidate)));
      description_width = std::max(
          description_width,
          small_font_.StringWidth(annotation.description().c_str()));
    }
    float width = shortcut_width + (shortcut_width > 0 ? kColumnGap : 0) +
                  value_width +
                  (description_width > 0 ? kColumnGap + description_width : 0);
    float rows = candidates_.candidate_size();
    const BString footer = Footer();
    if (!footer.IsEmpty()) {
      width = std::max(width, small_font_.StringWidth(footer));
      rows += 1;
    }
    return BSize(std::max(width + 2 * kPadding, 40.f),
                 rows * row + 2 * kPadding);
  }

  void Draw(BRect update) override {
    font_height height;
    be_plain_font->GetHeight(&height);
    const float row = RowHeight();
    float shortcut_width = 0;
    for (const auto& candidate : candidates_.candidate()) {
      shortcut_width = std::max(
          shortcut_width, be_plain_font->StringWidth(
                              candidate.annotation().shortcut().c_str()));
    }
    const float value_x =
        kPadding + shortcut_width + (shortcut_width > 0 ? kColumnGap : 0);
    const BRect bounds = Bounds();

    for (int i = 0; i < candidates_.candidate_size(); ++i) {
      const auto& candidate = candidates_.candidate(i);
      const BRect frame(bounds.left, kPadding + i * row, bounds.right,
                        kPadding + (i + 1) * row - 1);
      const bool focused = candidates_.has_focused_index() &&
                           candidate.index() == candidates_.focused_index();
      SetHighUIColor(focused ? B_MENU_SELECTED_BACKGROUND_COLOR
                             : B_MENU_BACKGROUND_COLOR);
      FillRect(frame);
      SetLowUIColor(focused ? B_MENU_SELECTED_BACKGROUND_COLOR
                            : B_MENU_BACKGROUND_COLOR);
      const float baseline = frame.top + height.ascent + 1;

      const auto& annotation = candidate.annotation();
      SetFont(be_plain_font);
      if (!annotation.shortcut().empty()) {
        SetHighUIColor(B_MENU_ITEM_TEXT_COLOR, B_DISABLED_LABEL_TINT);
        DrawString(annotation.shortcut().c_str(),
                   BPoint(kPadding, baseline));
      }
      SetHighUIColor(focused ? B_MENU_SELECTED_ITEM_TEXT_COLOR
                             : B_MENU_ITEM_TEXT_COLOR);
      DrawString(Value(candidate), BPoint(value_x, baseline));
      if (!annotation.description().empty()) {
        SetFont(&small_font_);
        SetHighUIColor(focused ? B_MENU_SELECTED_ITEM_TEXT_COLOR
                               : B_MENU_ITEM_TEXT_COLOR,
                       focused ? B_NO_TINT : B_DISABLED_LABEL_TINT);
        const char* description = annotation.description().c_str();
        DrawString(description,
                   BPoint(bounds.right - kPadding -
                              small_font_.StringWidth(description),
                          baseline));
      }
    }

    const BString footer = Footer();
    if (!footer.IsEmpty()) {
      SetFont(&small_font_);
      SetLowUIColor(B_MENU_BACKGROUND_COLOR);
      SetHighUIColor(B_MENU_ITEM_TEXT_COLOR, B_DISABLED_LABEL_TINT);
      const float top = kPadding + candidates_.candidate_size() * row;
      DrawString(footer, BPoint(bounds.right - kPadding -
                                    small_font_.StringWidth(footer),
                                top + height.ascent + 1));
    }
    SetHighUIColor(B_MENU_BACKGROUND_COLOR, B_DARKEN_2_TINT);
    StrokeRect(bounds);
  }

  void MouseDown(BPoint where) override {
    const int index =
        static_cast<int>((where.y - kPadding) / RowHeight());
    if (index < 0 || index >= candidates_.candidate_size()) {
      return;
    }
    const auto& candidate = candidates_.candidate(index);
    if (!candidate.has_id()) {
      return;
    }
    BMessage message(kMsgSelectCandidate);
    message.AddInt32("id", candidate.id());
    target_.SendMessage(&message);
  }

  void MessageReceived(BMessage* message) override {
    if (message->what == B_MOUSE_WHEEL_CHANGED) {
      float delta = 0;
      if (message->FindFloat("be:wheel_delta_y", &delta) == B_OK &&
          delta != 0) {
        BMessage page(kMsgCandidatePage);
        page.AddBool("forward", delta > 0);
        target_.SendMessage(&page);
      }
      return;
    }
    BView::MessageReceived(message);
  }

 private:
  float RowHeight() const {
    font_height height;
    be_plain_font->GetHeight(&height);
    return std::ceil(height.ascent + height.descent + height.leading) + 2;
  }

  static BString Value(const commands::CandidateWindow::Candidate& candidate) {
    const auto& annotation = candidate.annotation();
    BString value(annotation.prefix().c_str());
    value << candidate.value().c_str() << annotation.suffix().c_str();
    return value;
  }

  BString Footer() const {
    BString footer;
    if (candidates_.has_focused_index() &&
        candidates_.size() > static_cast<uint32>(candidates_.candidate_size())) {
      footer << (candidates_.focused_index() + 1) << "/" << candidates_.size();
    }
    return footer;
  }

  BMessenger target_;
  BFont small_font_;
  commands::CandidateWindow candidates_;
};

CandidateWindow::CandidateWindow(BMessenger target)
    : BWindow(BRect(0, 0, 10, 10), "Mozc candidates", B_NO_BORDER_WINDOW_LOOK,
              B_FLOATING_ALL_WINDOW_FEEL,
              B_NOT_MOVABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE |
                  B_NOT_MINIMIZABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS |
                  B_NOT_ANCHORED_ON_ACTIVATE,
              B_ALL_WORKSPACES),
      view_(new CandidateView(Bounds(), target)) {
  AddChild(view_);
  // Run the window's thread, hidden.
  Hide();
  Show();
}

void CandidateWindow::ShowCandidates(
    const commands::CandidateWindow& candidates, BPoint caret,
    float line_height) {
  BMessage message(kMsgShow);
  const std::string data = candidates.SerializeAsString();
  message.AddData("candidates", B_RAW_TYPE, data.data(), data.size());
  message.AddPoint("caret", caret);
  message.AddFloat("height", line_height);
  PostMessage(&message);
}

void CandidateWindow::MoveToCaret(BPoint caret, float line_height) {
  BMessage message(kMsgMove);
  message.AddPoint("caret", caret);
  message.AddFloat("height", line_height);
  PostMessage(&message);
}

void CandidateWindow::HideCandidates() { PostMessage(kMsgHide); }

void CandidateWindow::MessageReceived(BMessage* message) {
  switch (message->what) {
    case kMsgShow: {
      const void* data = nullptr;
      ssize_t size = 0;
      commands::CandidateWindow candidates;
      if (message->FindData("candidates", B_RAW_TYPE, &data, &size) != B_OK ||
          !candidates.ParseFromArray(data, size)) {
        break;
      }
      view_->SetCandidates(candidates);
      const BSize size_wanted = view_->PreferredSize();
      ResizeTo(size_wanted.width, size_wanted.height);
      Place(message->GetPoint("caret", BPoint()),
            message->GetFloat("height", 0));
      if (IsHidden()) {
        Show();
      }
      break;
    }
    case kMsgMove:
      Place(message->GetPoint("caret", BPoint()),
            message->GetFloat("height", 0));
      break;
    case kMsgHide:
      if (!IsHidden()) {
        Hide();
      }
      break;
    default:
      BWindow::MessageReceived(message);
      break;
  }
}

void CandidateWindow::Place(BPoint caret, float line_height) {
  const BRect screen = BScreen(this).Frame();
  const BRect frame = Frame();
  float x = caret.x;
  float y = caret.y + line_height + 2;
  if (y + frame.Height() > screen.bottom) {
    // No room below the line: show the list above it.
    y = caret.y - frame.Height() - 2;
  }
  x = std::clamp(x, screen.left, std::max(screen.left,
                                           screen.right - frame.Width()));
  y = std::clamp(y, screen.top, std::max(screen.top,
                                          screen.bottom - frame.Height()));
  MoveTo(x, y);
}

}  // namespace haiku
}  // namespace mozc
