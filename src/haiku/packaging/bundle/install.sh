#!/bin/sh
# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Installs Mozc for RenkuOS from this folder: Mozc, TerminalJ+, and, for Qt 6
# and Qt 5 applications, the qt6_base and qthaikuplugins with inline input.
# Can be run from Tracker.

dir=$(dirname "$0")
log=/tmp/mozc-install.log
# Mozc and TerminalJ+ may be in the system already (the VAIO P image has
# them), and then only the Qt packages are in this folder.
packages="$(ls "$dir"/mozc-*.hpkg "$dir"/terminalj_plus-*.hpkg 2>/dev/null)"

# On x86_gcc2 hybrids Qt is built for the x86 secondary architecture, and its
# packages carry its name: qt6_base_x86, qthaikuplugins_x86.
case "$(getarch 2>/dev/null)" in
	x86_gcc2)
		qt=_x86
		;;
	*)
		qt=
		;;
esac

# Mozc 2.x of HaikuPorts crashes the input_server when it is removed while
# running (a double free in its MozcLooper), so it has to go before a reboot.
old_mozc=$(ls /boot/system/packages/mozc-2.*.hpkg \
	/boot/system/packages/mozc_x86-2.*.hpkg \
	/boot/home/config/packages/mozc-2.*.hpkg \
	/boot/home/config/packages/mozc_x86-2.*.hpkg 2>/dev/null)
if [ -n "$old_mozc" ]; then
	alert --warning "古い Mozc（2.x）が入っています。

先に HaikuDepot などで古い Mozc をアンインストールし、再起動してから、もう一度このインストーラーを実行してください。

アンインストールの直後にデバッガが開いてキーボードが効かなくなることがありますが、再起動すれば直ります。" "OK"
	exit 1
fi

qt_package=$(ls "$dir"/qt6_base$qt-*.hpkg 2>/dev/null)
if [ -n "$qt_package" ]; then
	installed=$(ls /boot/system/packages/qt6_base$qt-*.hpkg \
		/boot/home/config/packages/qt6_base$qt-*.hpkg 2>/dev/null)
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

# Qt 5 is only given inline input when it is there already.
qt5_package=$(ls "$dir"/qthaikuplugins$qt-*.hpkg 2>/dev/null)
if [ -n "$qt5_package" ]; then
	installed=$(ls /boot/system/packages/qthaikuplugins$qt-*.hpkg \
		/boot/home/config/packages/qthaikuplugins$qt-*.hpkg 2>/dev/null)
	if [ -n "$installed" ]; then
		packages="$packages $qt5_package"
	fi
fi

if [ -z "$(echo $packages)" ]; then
	alert --info "インストールするものはありません。

Qt 6 / Qt 5 対応は、Qt 6 / Qt 5 が入っているときに入れられます。" "OK"
	exit 0
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
