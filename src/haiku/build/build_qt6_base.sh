#!/bin/bash
# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Builds a qt6_base package that is the HaikuPorts one with the Haiku
# platform plugin (add-ons/Qt6/platforms/libqhaiku.so) rebuilt with the inline
# input method patch of this port (patches/qt6-haikuplugins-*), so that Mozc
# composes text inline in Qt 6 applications.
#
# Only that file and the package info change; everything else, attributes
# included, is the HaikuPorts package as it is.
#
# Usage: build_qt6_base.sh <work dir> <output dir>
# Needs: a built RenkuOS tree in $RENKU (default ~/renku), git, curl, and the
# moc of Qt 6.10 on the host (Debian: qt6-base-dev-tools).

set -euo pipefail

WORK=$(realpath -m "$1")
OUT=$(realpath -m "$2")
HERE=$(dirname "$(realpath "$0")")
PATCH=$HERE/patches/qt6-haikuplugins-0001-platform-compose-input-method-text-inline.patch

QT_VERSION=6.10.3
QT_REVISION=3
PLUGINS_REV=1d255f2f8e58d66c1d3f2fc12cf651e4ca0aa80d
HAIKUPORTS_REV=master
NEW_VERSION=$QT_VERSION.1-1

RENKU=${RENKU:-$HOME/renku}
G=$RENKU/generated
CXX=$G/cross-tools-x86_64/bin/x86_64-unknown-haiku-g++
STRIP=$G/cross-tools-x86_64/bin/x86_64-unknown-haiku-strip
PACKAGE="env LD_LIBRARY_PATH=$G/objects/linux/lib $G/objects/linux/x86_64/release/tools/package/package"
DEVEL=$G/objects/haiku/x86_64/packaging/packages_build/regular/hpkg_-haiku_devel.hpkg/contents/develop
REL=$G/objects/haiku/x86_64/release
MOC=${MOC:-/usr/lib/qt6/libexec/moc}
REPO=https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages

mkdir -p "$WORK" "$OUT"
cd "$WORK"

# ---- the HaikuPorts packages ----------------------------------------------------
QT_PKG=qt6_base-$QT_VERSION-$QT_REVISION-x86_64.hpkg
QT_DEVEL_PKG=qt6_base_devel-$QT_VERSION-$QT_REVISION-x86_64.hpkg
for p in $QT_PKG $QT_DEVEL_PKG; do
	[ -f "$p" ] || curl -fsSL -o "$p" "$REPO/$p"
done
extract() {
	[ -d "x/$2" ] || { mkdir -p "x/$2"; $PACKAGE extract -C "x/$2" "$1"; }
}
extract "$QT_PKG" qt
extract "$QT_DEVEL_PKG" qt_devel
for p in freetype_devel mesa_devel glu_devel; do
	extract "$(ls "$G"/download/$p-*-x86_64.hpkg | head -1)" "$p"
done

# ---- the plugin sources, as HaikuPorts builds them, plus our patch ---------------
if [ ! -d src ]; then
	git clone -q https://github.com/threedeyes/qt6-haikuplugins.git src
	git -C src checkout -q $PLUGINS_REV
	curl -fsSL -o haikuports.patchset \
		"https://raw.githubusercontent.com/haikuports/haikuports/$HAIKUPORTS_REV/dev-qt/qt6-base/patches/qt6_base-$QT_VERSION-source2.patchset"
	git -C src apply ../haikuports.patchset
	git -C src apply "$PATCH"
fi

# ---- build ------------------------------------------------------------------------
Q=$WORK/x/qt_devel/develop/headers/Qt6
QI="-I$Q"
for m in QtCore QtGui QtWidgets; do
	QI="$QI -I$Q/$m -I$Q/$m/$QT_VERSION -I$Q/$m/$QT_VERSION/$m"
done
QI="$QI -I$WORK/x/mesa_devel/develop/headers"
QI="$QI -I$WORK/x/mesa_devel/develop/headers/opengl"
QI="$QI -I$WORK/x/glu_devel/develop/headers"
QI="$QI -I$WORK/x/freetype_devel/develop/headers/freetype2"
QI="$QI -I$WORK/x/qt_devel/data/Qt6/mkspecs/haiku-g++"
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
cd "$WORK/src/src/platform"
for h in *.h; do
	if grep -q Q_OBJECT "$h"; then
		"$MOC" -D__HAIKU__ $QI "$h" -o "$OBJ/moc_${h%.h}.cpp"
	fi
done
"$MOC" -D__HAIKU__ $QI main.cpp -o "$OBJ/main.moc"
for f in *.cpp "$OBJ"/moc_*.cpp ../../3rdparty/simplecrypt/simplecrypt.cpp; do
	echo "  CXX  $(basename "$f")"
	"$CXX" $FLAGS -I. -I"$OBJ" -I../../3rdparty/simplecrypt $QI $HI -w \
		-c "$f" -o "$OBJ/$(basename "${f%.cpp}").o"
done
PLUGIN=$OBJ/libqhaiku.so
"$CXX" -shared -Wl,-z,defs -Wl,-soname,libqhaiku.so -o "$PLUGIN" "$OBJ"/*.o \
	-B"$DEVEL/lib" -L"$DEVEL/lib" -L"$WORK/x/qt/lib" -L"$REL/kits" \
	-L"$REL/kits/tracker" -L"$REL/kits/game" -L"$REL/system/libroot" \
	-L"$(ls -d "$G"/build_packages/mesa-*/lib)" \
	-L"$(ls -d "$G"/build_packages/gcc_syslibs-*/lib)" \
	-lQt6Widgets -lQt6Gui -lQt6Core -lbe -ltracker -lgame -lGL \
	"$(ls "$G"/build_packages/freetype-*/lib/libfreetype.so.6)" -lgcc_s
"$STRIP" "$PLUGIN"

# ---- the package --------------------------------------------------------------------
STAGE=$WORK/stage
rm -rf "$STAGE"
mkdir -p "$STAGE/add-ons/Qt6/platforms" "$STAGE/data/doc/qt6_base"
install -m 755 "$PLUGIN" "$STAGE/add-ons/Qt6/platforms/libqhaiku.so"
env LD_LIBRARY_PATH=$G/objects/linux/lib $G/objects/linux/x86_64/release/tools/addattr/addattr \
	-t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
	"$STAGE/add-ons/Qt6/platforms/libqhaiku.so"
# The source of the change, as the LGPL and the GPL ask.
cp "$PATCH" "$STAGE/data/doc/qt6_base/"
sed -e "s/^version\t.*/version\t\t\t$NEW_VERSION/" \
	-e "s/^packager\t.*/packager\t\t\"RenkuOS\"/" \
	-e 's/^\(summary\t*\)"\(.*\)"/\1"\2 (inline input methods)"/' \
	"$WORK/x/qt/.PackageInfo" > "$STAGE/.PackageInfo"

RESULT=$OUT/qt6_base-$NEW_VERSION-x86_64.hpkg
cp "$WORK/$QT_PKG" "$RESULT"
chmod u+w "$RESULT"
$PACKAGE add -f -C "$STAGE" "$RESULT" .PackageInfo \
	add-ons/Qt6/platforms/libqhaiku.so data/doc/qt6_base
echo "-> $RESULT"
