Mozc の Qt 6 / Qt 5 対応
=======================

このフォルダには、Qt 6 と Qt 5 のアプリでもカーソル位置で日本語を変換できる
ようにするパッケージが入っています。Mozc 本体と TerminalJ+ はすでにシステムに
入っているので、ここで入れるのは Qt 用のものだけです。

■ 入れ方
  Qt 6 や Qt 5 のアプリ（FeatherPad、Falkon など）を入れたあとで、
  「install」をダブルクリックしてください。
    - Qt 6 が入っていれば qt6_base_x86（Qt 6 対応）を入れます。
      Qt 6 がまだ入っていないときは、入れるかどうかを確認します
      （Qt 6 本体もダウンロードされます）。
    - Qt 5 が入っていれば qthaikuplugins_x86（Qt 5 対応）を入れます。

  ターミナルから入れる場合:
    pkgman install qt6_base_x86-6.10.3.2-1-x86_gcc2.hpkg
    pkgman install qthaikuplugins_x86-5.15.16.0.3-1-x86_gcc2.hpkg

■ 注意
  - どちらも HaikuPorts の qt6_base_x86 6.10.3 / qthaikuplugins_x86 5.15.16.0 の
    Haiku 用プラットフォームプラグインだけを差し替えたものです。HaikuPorts 側が
    新しくなって置き換わると、画面下の小窓での変換に戻ります。
  - 元に戻すには pkgman uninstall qt6_base_x86 のあと pkgman install qt6_base_x86
    （qthaikuplugins_x86 も同じ）。

■ ライセンス
  qt6_base: LGPL v3 / GPL v3、qthaikuplugins: LGPL v2.1 / v3
  （変更点のパッチは /boot/system/data/doc/qt6_base_x86 と
  /boot/system/data/doc/qthaikuplugins_x86 にあります）
