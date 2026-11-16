Mozc for RenkuOS
================

RenkuOS で日本語を入力するための Mozc（Google 日本語入力のオープンソース版）です。

■ 古い Mozc（HaikuDepot の Mozc 2.x）が入っている場合
  先に古い Mozc をアンインストールして、再起動してください。
    pkgman uninstall mozc
  古い Mozc は外されるときに input_server ごと落ちるため、アンインストールの
  直後にデバッガが開き、キーボードが効かなくなることがあります。再起動すれば
  直ります。新しい Mozc を上書きでインストールした場合も同じで、再起動後は
  新しい Mozc で動きます。

■ インストール
  「インストール」をダブルクリックしてください。次のものが入ります。
    - Mozc            入力メソッド本体
    - TerminalJ+      カーソル位置で日本語を変換できるターミナル
    - Qt 6 対応       Qt 6 アプリ（FeatherPad など）でカーソル位置で変換できるようにする
                      qt6_base（Qt 6 が入っているときは自動で、入っていないときは確認します）

  ターミナルから入れる場合:
    pkgman install mozc-*.hpkg terminalj_plus-*.hpkg [qt6_base-*.hpkg]

■ 使い方
  - Command+Space（通常は Alt+Space）で Roman と Mozc を切り替えます。
    日本語キーボードなら 半角/全角・変換・無変換・カタカナひらがな キーも使えます。
  - Deskbar のアイコン（あ ア Ａ ｱ _A A）が入力モードを表します。
    アイコンをクリックして「Mozc ▸」から、入力モード・設定・キー設定の編集・
    単語登録・辞書ツールを選べます。

■ カーソル位置で変換できるアプリ
  - StyledEdit、Mail などのアプリ、GTK 3 アプリ: そのまま
  - ターミナル: TerminalJ+ を使ってください（標準の Terminal は画面下の小窓で変換）
  - Qt 6 アプリ: 同梱の qt6_base を入れると対応
  それ以外のアプリでも、画面下の小窓で変換して入力できます。

■ アンインストール
    pkgman uninstall mozc terminalj_plus
  qt6_base を元に戻すには、pkgman uninstall qt6_base のあと pkgman install qt6_base
  （Qt 6 アプリを使っている場合）。

■ 注意
  - 同梱の qt6_base は、HaikuPorts の qt6_base 6.10.3 の Haiku 用プラグインだけを
    差し替えたものです。HaikuPorts で qt6_base が新しくなると置き換わり、
    Qt 6 アプリでは画面下の小窓での変換に戻ります。
  - ユーザー辞書と設定は ~/config/settings/mozc にあります。

■ ライセンス
  Mozc: BSD 3 条項、TerminalJ+: MIT、qt6_base: LGPL v3 / GPL v3
  （変更点のパッチは /boot/system/data/doc/qt6_base にあります）
