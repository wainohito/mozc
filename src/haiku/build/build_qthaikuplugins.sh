#!/bin/bash
# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Builds a qthaikuplugins package that is the HaikuPorts one with the Haiku
# platform plugin of Qt 5 (add-ons/Qt5/platforms/libqhaiku.so) rebuilt with
# the inline input method patch of this port (../patches/qthaikuplugins), so
# that Mozc composes text inline in Qt 5 applications.
#
# Only that file and the package info change; everything else, attributes
# included, is the HaikuPorts package as it is.
#
# Usage: build_qthaikuplugins.sh <work dir> <output dir>
# Needs: a built RenkuOS tree in $RENKU (default ~/renku), git, curl, and the
# moc of Qt 5.15 on the host (Debian: qtbase5-dev-tools).

set -euo pipefail

WORK=$(realpath -m "$1")
OUT=$(realpath -m "$2")
HERE=$(dirname "$(realpath "$0")")
PATCH=$(realpath "$HERE/../patches/qthaikuplugins/0001-platform-compose-input-method-text-inline.patch")

QT_VERSION=5.15.19
QT_REVISION=2
PLUGINS_VERSION=5.15.16.0
PLUGINS_REVISION=1
PLUGINS_REV=8af8224255962a707d8c6d87d8f5082824b1d819
NEW_VERSION=$PLUGINS_VERSION.1-1
# Libraries the plugin links with that are not in the RenkuOS build.
RUNTIME_PKGS="glib2-2.90.1-9-x86_64.hpkg gettext_libintl-1.0-1-x86_64.hpkg"

RENKU=${RENKU:-$HOME/renku}
G=$RENKU/generated
CXX=$G/cross-tools-x86_64/bin/x86_64-unknown-haiku-g++
STRIP=$G/cross-tools-x86_64/bin/x86_64-unknown-haiku-strip
PACKAGE="env LD_LIBRARY_PATH=$G/objects/linux/lib $G/objects/linux/x86_64/release/tools/package/package"
DEVEL=$G/objects/haiku/x86_64/packaging/packages_build/regular/hpkg_-haiku_devel.hpkg/contents/develop
REL=$G/objects/haiku/x86_64/release
MOC=${MOC:-/usr/lib/qt5/bin/moc}
REPO=https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages

mkdir -p "$WORK" "$OUT"
cd "$WORK"

# ---- the HaikuPorts packages ----------------------------------------------------
PLUGINS_PKG=qthaikuplugins-$PLUGINS_VERSION-$PLUGINS_REVISION-x86_64.hpkg
QT_PKG=qt5-$QT_VERSION-$QT_REVISION-x86_64.hpkg
QT_DEVEL_PKG=qt5_devel-$QT_VERSION-$QT_REVISION-x86_64.hpkg
for p in $PLUGINS_PKG $QT_PKG $QT_DEVEL_PKG $RUNTIME_PKGS; do
	[ -f "$p" ] || curl -fsSL -o "$p" "$REPO/$p"
done
extract() {
	[ -d "x/$2" ] || { mkdir -p "x/$2"; $PACKAGE extract -C "x/$2" "$1"; }
}
extract "$PLUGINS_PKG" plugins
extract "$QT_PKG" qt
extract "$QT_DEVEL_PKG" qt_devel
for p in $RUNTIME_PKGS; do
	extract "$p" "${p%%-*}"
done
for p in freetype_devel fontconfig fontconfig_devel mesa_devel glu glu_devel; do
	extract "$(ls "$G"/download/$p-*-x86_64.hpkg | head -1)" "$p"
done

# ---- the plugin sources, as HaikuPorts builds them, plus our patch ---------------
if [ ! -d src ]; then
	git clone -q https://github.com/threedeyes/qthaikuplugins.git src
	git -C src checkout -q $PLUGINS_REV
	git -C src apply "$PATCH"
fi

# ---- build ------------------------------------------------------------------------
Q=$WORK/x/qt_devel/develop/headers
QI="-I$Q"
for m in QtCore QtGui QtWidgets QtEventDispatcherSupport QtFontDatabaseSupport \
		QtThemeSupport; do
	QI="$QI -I$Q/$m -I$Q/$m/$QT_VERSION -I$Q/$m/$QT_VERSION/$m"
done
QI="$QI -I$WORK/x/mesa_devel/develop/headers"
QI="$QI -I$WORK/x/mesa_devel/develop/headers/opengl"
QI="$QI -I$WORK/x/glu_devel/develop/headers"
QI="$QI -I$WORK/x/freetype_devel/develop/headers/freetype2"
QI="$QI -I$WORK/x/fontconfig_devel/develop/headers"
QI="$QI -I$WORK/x/qt_devel/data/Qt5/mkspecs/haiku-g++"
H=$DEVEL/headers
HI="-idirafter $H -idirafter $H/posix -idirafter $H/os"
for d in "$H"/os/*/; do HI="$HI -idirafter $d"; done
HI="$HI -idirafter $H/os/add-ons/graphics -idirafter $H/private/interface"
HI="$HI -idirafter $H/private/shared -idirafter $H/private/app -idirafter $H/glibc"
FLAGS="-DQT_NO_DEBUG -DQT_PLUGIN -DQT_GUI_LIB -DQT_CORE_LIB -DQT_WIDGETS_LIB"
FLAGS="$FLAGS -fPIC -std=c++17 -O2"

OBJ=$WORK/obj
rm -rf "$OBJ"
mkdir -p "$OBJ"
cd "$WORK/src/platforms/qhaikuplatform/haiku"
for h in *.h; do
	if grep -q Q_OBJECT "$h"; then
		"$MOC" -D__HAIKU__ $QI "$h" -o "$OBJ/moc_${h%.h}.cpp"
	fi
done
"$MOC" -D__HAIKU__ $QI main.cpp -o "$OBJ/main.moc"
for f in *.cpp "$OBJ"/moc_*.cpp ../../../3rdparty/simplecrypt/simplecrypt.cpp; do
	echo "  CXX  $(basename "$f")"
	"$CXX" $FLAGS -I. -I"$OBJ" -I../../../3rdparty/simplecrypt $QI $HI -w \
		-c "$f" -o "$OBJ/$(basename "${f%.cpp}").o"
done
PLUGIN=$OBJ/libqhaiku.so
QL=$WORK/x/qt_devel/develop/lib
"$CXX" -shared -Wl,-z,defs -Wl,-soname,libqhaiku.so -o "$PLUGIN" "$OBJ"/*.o \
	-B"$DEVEL/lib" -L"$DEVEL/lib" -L"$WORK/x/qt/lib" -L"$REL/kits" \
	-L"$REL/kits/tracker" -L"$REL/kits/game" -L"$REL/system/libroot" \
	-L"$(ls -d "$G"/build_packages/mesa-*/lib)" \
	-L"$(ls -d "$G"/build_packages/gcc_syslibs-*/lib)" \
	"$QL"/libQt5EventDispatcherSupport.a "$QL"/libQt5FontDatabaseSupport.a \
	"$QL"/libQt5ThemeSupport.a \
	-lQt5DBus -lQt5Widgets -lQt5Gui -lQt5Core -lbe -ltracker -lgame -lGL \
	"$WORK"/x/glu/lib/libGLU.so.1 "$WORK"/x/glib2/lib/libgthread-2.0.so.0 \
	"$WORK"/x/glib2/lib/libglib-2.0.so.0 "$WORK"/x/gettext_libintl/lib/libintl.so.8 \
	"$WORK"/x/fontconfig/lib/libfontconfig.so.1 \
	"$(ls "$G"/build_packages/freetype-*/lib/libfreetype.so.6)" -lgcc_s
"$STRIP" "$PLUGIN"

# ---- the package --------------------------------------------------------------------
STAGE=$WORK/stage
rm -rf "$STAGE"
mkdir -p "$STAGE/add-ons/Qt5/platforms" "$STAGE/data/doc/qthaikuplugins"
install -m 755 "$PLUGIN" "$STAGE/add-ons/Qt5/platforms/libqhaiku.so"
env LD_LIBRARY_PATH=$G/objects/linux/lib $G/objects/linux/x86_64/release/tools/addattr/addattr \
	-t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
	"$STAGE/add-ons/Qt5/platforms/libqhaiku.so"
# The source of the change, as the LGPL asks.
cp "$PATCH" "$STAGE/data/doc/qthaikuplugins/"
sed -e "s/^version\t.*/version\t\t\t$NEW_VERSION/" \
	-e "s/^packager\t.*/packager\t\t\"RenkuOS\"/" \
	-e 's/^\(summary\t*\)"\(.*\)"/\1"\2 (inline input methods)"/' \
	"$WORK/x/plugins/.PackageInfo" > "$STAGE/.PackageInfo"

RESULT=$OUT/qthaikuplugins-$NEW_VERSION-x86_64.hpkg
cp "$WORK/$PLUGINS_PKG" "$RESULT"
chmod u+w "$RESULT"
$PACKAGE add -f -C "$STAGE" "$RESULT" .PackageInfo \
	add-ons/Qt5/platforms/libqhaiku.so data/doc/qthaikuplugins
echo "-> $RESULT"
