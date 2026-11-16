// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/mode_icon.h"

#include <Bitmap.h>
#include <Font.h>
#include <GraphicsDefs.h>
#include <InterfaceDefs.h>
#include <Screen.h>
#include <View.h>

#include "protocol/commands.pb.h"

namespace mozc {
namespace haiku {
namespace {

const char* LabelOf(commands::CompositionMode mode) {
  switch (mode) {
    case commands::HIRAGANA:
      return "あ";
    case commands::FULL_KATAKANA:
      return "ア";
    case commands::FULL_ASCII:
      return "Ａ";
    case commands::HALF_KATAKANA:
      return "ｱ";
    case commands::HALF_ASCII:
      return "_A";
    default:
      return "A";
  }
}

ModeIcon EmptyIcon() {
  ModeIcon icon;
  icon.fill(B_TRANSPARENT_MAGIC_CMAP8);
  return icon;
}

}  // namespace

ModeIcon MakeModeIcon(commands::CompositionMode mode) {
  const BRect bounds(0, 0, kModeIconSize - 1, kModeIconSize - 1);
  BBitmap bitmap(bounds, B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
  if (bitmap.InitCheck() != B_OK) {
    return EmptyIcon();
  }
  BView* view = new BView(bounds, "mode icon", B_FOLLOW_NONE, B_WILL_DRAW);
  bitmap.AddChild(view);
  if (!bitmap.Lock()) {
    return EmptyIcon();
  }

  // A key cap: the accent color while Japanese is input, gray otherwise.
  const bool direct = mode == commands::DIRECT;
  const rgb_color cap =
      direct ? tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT)
             : ui_color(B_KEYBOARD_NAVIGATION_COLOR);
  const rgb_color text = direct ? ui_color(B_PANEL_TEXT_COLOR)
                                : make_color(255, 255, 255, 255);

  view->SetDrawingMode(B_OP_COPY);
  view->SetHighColor(cap);
  view->FillRect(bounds);

  BFont font(be_bold_font);
  const char* label = LabelOf(mode);
  font.SetSize(12);
  // Wide labels (_A) are squeezed into the cap.
  while (font.StringWidth(label) > kModeIconSize - 2 && font.Size() > 7) {
    font.SetSize(font.Size() - 1);
  }
  font_height height;
  font.GetHeight(&height);
  view->SetFont(&font);
  view->SetHighColor(text);
  view->SetLowColor(cap);
  view->SetDrawingMode(B_OP_OVER);
  const float x = (kModeIconSize - font.StringWidth(label)) / 2;
  const float y =
      (kModeIconSize - (height.ascent + height.descent)) / 2 + height.ascent;
  view->DrawString(label, BPoint(x, y));
  view->Sync();

  // Convert to the palette of the input_server icons.
  ModeIcon icon = EmptyIcon();
  BScreen screen;
  const uint8* bits = static_cast<const uint8*>(bitmap.Bits());
  const int32 bytes_per_row = bitmap.BytesPerRow();
  for (int y = 0; y < kModeIconSize; ++y) {
    for (int x = 0; x < kModeIconSize; ++x) {
      const uint8* pixel = bits + y * bytes_per_row + x * 4;  // B, G, R, A
      icon[y * kModeIconSize + x] =
          screen.IndexForColor(pixel[2], pixel[1], pixel[0]);
    }
  }
  bitmap.Unlock();

  // Round the corners of the cap.
  constexpr int kLast = kModeIconSize - 1;
  for (const int index : {0, kLast, kLast * kModeIconSize,
                          kLast * kModeIconSize + kLast}) {
    icon[index] = B_TRANSPARENT_MAGIC_CMAP8;
  }
  return icon;
}

}  // namespace haiku
}  // namespace mozc
