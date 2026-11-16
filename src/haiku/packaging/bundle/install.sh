#!/bin/sh
# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Installs Mozc for RenkuOS from this folder: Mozc, TerminalJ+, and, for Qt 6
# applications, the qt6_base with inline input. Can be run from Tracker.

dir=$(dirname "$0")
log=/tmp/mozc-install.log
packages="$(ls "$dir"/mozc-*.hpkg "$dir"/terminalj_plus-*.hpkg)"

qt_package=$(ls "$dir"/qt6_base-*.hpkg 2>/dev/null)
if [ -n "$qt_package" ]; then
	installed=$(ls /boot/system/packages/qt6_base-*.hpkg \
		/boot/home/config/packages/qt6_base-*.hpkg 2>/dev/null)
	if [ -n "$installed" ]; then
		# Qt 6 is there: give it inline input.
		packages="$packages $qt_package"
	else
		alert --info "Qt 6 アプリでもインライン変換を使えるようにしますか？

Qt 6 がまだ入っていないので、Qt 6 本体（数十 MB）も一緒にダウンロードされます。Qt 6 アプリを使わない場合は不要です。" \
			"入れない" "入れる"
		if [ $? = 1 ]; then
			packages="$packages $qt_package"
		fi
	fi
fi

if pkgman install -y $packages > "$log" 2>&1; then
	alert --idea "Mozc をインストールしました。

Command+Space（通常は Alt+Space）で Mozc に切り替えます。入力モードと設定は Deskbar のアイコンから選べます。

ターミナルでのインライン変換には、アプリケーションメニューの「TerminalJ+」を使ってください。" "OK"
else
	alert --stop "インストールできませんでした。

詳しくは $log を見てください。" "OK"
	exit 1
fi
