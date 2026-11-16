# Mozc for RenkuOS

This directory holds the port of Mozc to RenkuOS (a fork of Haiku): an
input_server method written against the BeAPI, the tools it opens, and the
cross build.

## What is here

| Directory        | Contents |
|------------------|----------|
| `input_method/`  | The input_server add-on (`Mozc`). Sends keys to `mozc_server`, composes text inline with `B_INPUT_METHOD_EVENT`, shows the candidate window, and keeps the input mode and the settings in the Deskbar. |
| `tool/`          | `mozc_tool`: word registration, the dictionary tool, the key bindings editor, the about box and error messages, with the Interface Kit. |
| `build/`         | `gen_sources.py`, which turns a host Bazel build into the source lists of the cross build, and patches for third party code. |
| `packaging/`     | `PackageInfo.in` of the `mozc` hpkg, and the installer and notes of the bundle. |
| `patches/`       | Changes for other projects: RenkuOS (`renkuos/`, apply with `git am`) and the Qt 5 platform plugin (`qthaikuplugins/`). The Qt 6 one is in `build/patches/`. |

`mozc_server` itself is the unchanged Mozc server; the platform code it needs
is under `#if defined(__HAIKU__)` / `port::IsHaiku()` in `base/` and `ipc/`.

## Building

Bazel cannot target Haiku, so the host build is the source of truth and a
Makefile does the cross build:

    make -C haiku gen BAZEL=bazelisk   # once, and after updating Mozc
    make -C haiku -j8                  # mozc_server, Mozc, mozc_tool
    make -C haiku hpkg                 # out_haiku/mozc-<version>-x86_64.hpkg
    make -C haiku bundle               # zip with Mozc, TerminalJ+, Qt packages
                                       # and an installer

`make gen` builds Mozc for Linux with Bazel, reads the compile actions with
`bazel aquery`, and copies the generated files (protobuf code, the embedded
dictionary) and abseil/protobuf into `out_haiku/`. The cross build needs a
built RenkuOS tree (`RENKU`, default `~/renku`, with `generated/`).

The unit tests that run on the host:

    bazelisk test --config oss_linux //haiku/input_method:key_translator_test

## Installed files

    add-ons/input_server/methods/Mozc
    lib/mozc/mozc_server, lib/mozc/mozc_tool, lib/mozc/documents/
    data/licenses/Mozc

The add-on finds `lib/mozc` next to itself with `find_path()`, so the
package works installed in the system or in the home folder. User data is
kept in `~/config/settings/mozc`.

## Using it

Command+Space switches between Roman and Mozc, as for any input method.
Hankaku/Zenkaku, Henkan, Muhenkan and Katakana/Hiragana of Japanese keyboards
work with the Japanese keymap.

Everything else is in the Deskbar: the icon shows the input mode (あ ア Ａ ｱ
_A, or A for direct input), and its menu has the modes, the settings used day
to day (input style, kana layout, punctuation, symbols, space, key bindings,
candidate selection keys, suggestions) and the tools. A setting shows its
value; choosing it moves to the next value. "Edit key bindings…" opens the
key bindings editor, which saves a custom keymap; the menu follows changes
of the configuration made by the tools.

## Inline input in other toolkits

Text is composed inline wherever the focused view is `B_INPUT_METHOD_AWARE`
and handles `B_INPUT_METHOD_EVENT`; elsewhere the input_server shows its own
bottom-line window and sends the result as key presses.

* BeAPI: every `BTextView` (StyledEdit, Mail, text fields, ...).
* GTK 3: through the Wayland compatibility layer (`wayland_server`), which
  turns the input method events into text-input-v3.
* Qt 6: needs the Haiku platform plugin with the patch in
  `build/patches/qt6-haikuplugins-*`. `build/build_qt6_base.sh` makes a
  qt6_base package that is the HaikuPorts one with only that plugin
  replaced (version 6.10.3.1-1, so it is kept over 6.10.3-3):

      haiku/build/build_qt6_base.sh <work dir> out_haiku
      pkgman install qt6_base-6.10.3.1-1-x86_64.hpkg

  It needs the moc of Qt 6.10 on the host. When HaikuPorts updates
  qt6_base, the package has to be made again for the new version.
* Qt 5: the same change for qthaikuplugins is in `patches/qthaikuplugins`.
  `build/build_qthaikuplugins.sh` makes a qthaikuplugins package that is the
  HaikuPorts one with only the platform plugin replaced (version
  5.15.16.0.1-1, kept over 5.15.16.0-1):

      haiku/build/build_qthaikuplugins.sh <work dir> out_haiku
      pkgman install qthaikuplugins-5.15.16.0.1-1-x86_64.hpkg

  It needs the moc of Qt 5.15 on the host. Tested with Falkon, in its own
  fields and in web pages.
* Terminal: needs `patches/renkuos/0002-...` (its inline input was disabled).
  TerminalJ+ (https://github.com/wainohito/terminalj-plus) is Terminal with
  that change, as a separate application.

## Known limits

* The Deskbar icon only follows the input mode with
  `patches/renkuos/0001-...`; without it the icon shows the mode the method
  was activated in.
* The handwriting pad, the character palette and the full configuration
  dialog of other platforms are not ported; the romaji table can only be
  changed in the configuration file.
