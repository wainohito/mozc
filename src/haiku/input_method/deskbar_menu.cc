// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/deskbar_menu.h"

#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <String.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "haiku/input_method/messages.h"
#include "haiku/input_method/strings.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace haiku {
namespace {

using config::Config;

// The input_server gives the target of the method only to the items of its
// menu, not to those of submenus, so every choice is an item of the menu
// itself: a setting shows its value and choosing it moves to the next one.
using Choices = std::vector<std::pair<const char*, int32_t>>;

BMenuItem* ModeItem(Str label, commands::CompositionMode mode,
                    commands::CompositionMode current) {
  BMessage* message = new BMessage(kMsgSetMode);
  message->AddInt32("mode", mode);
  BMenuItem* item = new BMenuItem(GetString(label), message);
  item->SetMarked(mode == current);
  return item;
}

BMenuItem* CycleItem(Str label, ConfigField field, const Choices& choices,
                     int32_t current) {
  size_t index = 0;
  while (index < choices.size() && choices[index].second != current) {
    ++index;
  }
  if (index == choices.size()) {
    index = 0;  // A value the menu does not offer (a custom keymap).
  }
  BString text(GetString(label));
  text << ": " << choices[index].first;
  BMessage* message = new BMessage(kMsgSetConfig);
  message->AddInt32("field", static_cast<int32>(field));
  message->AddInt32("value", choices[(index + 1) % choices.size()].second);
  return new BMenuItem(text, message);
}

BMenuItem* ToggleItem(Str label, ConfigField field, bool current) {
  BMessage* message = new BMessage(kMsgSetConfig);
  message->AddInt32("field", static_cast<int32>(field));
  message->AddInt32("value", current ? 0 : 1);
  BMenuItem* item = new BMenuItem(GetString(label), message);
  item->SetMarked(current);
  return item;
}

}  // namespace

std::unique_ptr<BMenu> BuildDeskbarMenu(const MenuState& state) {
  auto menu = std::make_unique<BMenu>("Mozc");
  // Underlined trigger characters do not suit labels such as "、。".
  menu->SetTriggersEnabled(false);

  const commands::CompositionMode mode = state.mode;
  menu->AddItem(ModeItem(Str::kHiragana, commands::HIRAGANA, mode));
  menu->AddItem(ModeItem(Str::kFullKatakana, commands::FULL_KATAKANA, mode));
  menu->AddItem(ModeItem(Str::kFullAscii, commands::FULL_ASCII, mode));
  menu->AddItem(ModeItem(Str::kHalfKatakana, commands::HALF_KATAKANA, mode));
  menu->AddItem(ModeItem(Str::kHalfAscii, commands::HALF_ASCII, mode));
  menu->AddItem(ModeItem(Str::kDirect, commands::DIRECT, mode));
  menu->AddSeparatorItem();

  if (!state.has_config) {
    BMenuItem* item =
        new BMenuItem(GetString(Str::kServerNotRunning), nullptr);
    item->SetEnabled(false);
    menu->AddItem(item);
    menu->AddSeparatorItem();
  } else {
    const Config& config = state.config;
    menu->AddItem(CycleItem(Str::kInputStyle, ConfigField::kPreeditMethod,
                            {{GetString(Str::kRomaji), Config::ROMAN},
                             {GetString(Str::kKana), Config::KANA}},
                            config.preedit_method()));
    if (config.preedit_method() == Config::KANA) {
      BMessage* message = new BMessage(kMsgSetKanaLayout);
      const bool jis = state.kana_layout == KanaLayout::kJis;
      message->AddInt32("layout", static_cast<int32>(jis ? KanaLayout::kUs
                                                          : KanaLayout::kJis));
      BString text(GetString(Str::kKanaLayout));
      text << ": "
           << GetString(jis ? Str::kKanaLayoutJis : Str::kKanaLayoutUs);
      menu->AddItem(new BMenuItem(text, message));
    }
    menu->AddItem(CycleItem(Str::kPunctuation, ConfigField::kPunctuationMethod,
                            {{"、。", Config::TOUTEN_KUTEN},
                             {"，．", Config::COMMA_PERIOD},
                             {"、．", Config::TOUTEN_PERIOD},
                             {"，。", Config::COMMA_KUTEN}},
                            config.punctuation_method()));
    menu->AddItem(CycleItem(Str::kSymbols, ConfigField::kSymbolMethod,
                            {{"「」・", Config::CORNER_BRACKET_MIDDLE_DOT},
                             {"[]／", Config::SQUARE_BRACKET_SLASH},
                             {"「」／", Config::CORNER_BRACKET_SLASH},
                             {"[]・", Config::SQUARE_BRACKET_MIDDLE_DOT}},
                            config.symbol_method()));
    menu->AddItem(CycleItem(
        Str::kSpace, ConfigField::kSpaceCharacterForm,
        {{GetString(Str::kSpaceFollowMode), Config::FUNDAMENTAL_INPUT_MODE},
         {GetString(Str::kSpaceFull), Config::FUNDAMENTAL_FULL_WIDTH},
         {GetString(Str::kSpaceHalf), Config::FUNDAMENTAL_HALF_WIDTH}},
        config.space_character_form()));
    Choices keymaps = {{"MS-IME", Config::MSIME},
                       {"ATOK", Config::ATOK},
                       {"ことえり", Config::KOTOERI}};
    if (config.session_keymap() == Config::CUSTOM) {
      // A custom keymap is edited elsewhere; keep it among the choices.
      keymaps.insert(keymaps.begin(),
                     {GetString(Str::kKeymapCustom), Config::CUSTOM});
    }
    menu->AddItem(CycleItem(Str::kKeymap, ConfigField::kSessionKeymap,
                            keymaps, config.session_keymap()));
    BMessage* edit_keymap = new BMessage(kMsgLaunchTool);
    edit_keymap->AddString("mode", "keymap_editor");
    menu->AddItem(new BMenuItem(GetString(Str::kEditKeymap), edit_keymap));
    menu->AddItem(CycleItem(
        Str::kSelectionShortcut, ConfigField::kSelectionShortcut,
        {{"1 – 9", Config::SHORTCUT_123456789},
         {"A – L", Config::SHORTCUT_ASDFGHJKL},
         {GetString(Str::kSelectionShortcutNone), Config::NO_SHORTCUT}},
        config.selection_shortcut()));
    menu->AddSeparatorItem();
    menu->AddItem(ToggleItem(Str::kSuggestHistory,
                             ConfigField::kUseHistorySuggest,
                             config.use_history_suggest()));
    menu->AddItem(ToggleItem(Str::kSuggestDictionary,
                             ConfigField::kUseDictionarySuggest,
                             config.use_dictionary_suggest()));
    menu->AddItem(ToggleItem(Str::kSuggestRealtime,
                             ConfigField::kUseRealtimeConversion,
                             config.use_realtime_conversion()));
    menu->AddSeparatorItem();
  }

  for (const auto& [label, tool] :
       {std::pair{Str::kWordRegister, "word_register_dialog"},
        std::pair{Str::kDictionaryTool, "dictionary_tool"},
        std::pair{Str::kAbout, "about_dialog"}}) {
    BMessage* message = new BMessage(kMsgLaunchTool);
    message->AddString("mode", tool);
    menu->AddItem(new BMenuItem(GetString(label), message));
    if (tool == std::string_view("dictionary_tool")) {
      menu->AddSeparatorItem();
    }
  }
  return menu;
}

bool ApplyConfigChoice(int32_t field, int32_t value, Config* config) {
  switch (static_cast<ConfigField>(field)) {
    case ConfigField::kPreeditMethod:
      if (!Config::PreeditMethod_IsValid(value)) return false;
      config->set_preedit_method(static_cast<Config::PreeditMethod>(value));
      return true;
    case ConfigField::kPunctuationMethod:
      if (!Config::PunctuationMethod_IsValid(value)) return false;
      config->set_punctuation_method(
          static_cast<Config::PunctuationMethod>(value));
      return true;
    case ConfigField::kSymbolMethod:
      if (!Config::SymbolMethod_IsValid(value)) return false;
      config->set_symbol_method(static_cast<Config::SymbolMethod>(value));
      return true;
    case ConfigField::kSpaceCharacterForm:
      if (!Config::FundamentalCharacterForm_IsValid(value)) return false;
      config->set_space_character_form(
          static_cast<Config::FundamentalCharacterForm>(value));
      return true;
    case ConfigField::kSessionKeymap:
      if (!Config::SessionKeymap_IsValid(value)) return false;
      config->set_session_keymap(static_cast<Config::SessionKeymap>(value));
      return true;
    case ConfigField::kSelectionShortcut:
      if (!Config::SelectionShortcut_IsValid(value)) return false;
      config->set_selection_shortcut(
          static_cast<Config::SelectionShortcut>(value));
      return true;
    case ConfigField::kUseHistorySuggest:
      config->set_use_history_suggest(value != 0);
      return true;
    case ConfigField::kUseDictionarySuggest:
      config->set_use_dictionary_suggest(value != 0);
      return true;
    case ConfigField::kUseRealtimeConversion:
      config->set_use_realtime_conversion(value != 0);
      return true;
  }
  return false;
}

}  // namespace haiku
}  // namespace mozc
