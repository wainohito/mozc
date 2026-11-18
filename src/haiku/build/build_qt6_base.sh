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
# Needs: a built RenkuOS tree, git, curl, and the moc of Qt 6.10 on the host
# (Debian: qt6-base-dev-tools).
#
# ARCH=x86_64 (the default) uses the tree in $RENKU (default ~/renku) and
# makes qt6_base-*-x86_64.hpkg. ARCH=x86_gcc2h uses the 32-bit x86_gcc2
# hybrid tree in $GEN (default $RENKU/generated.x86_gcc2h) and makes
# qt6_base_x86-*-x86_gcc2.hpkg, the Qt 6 of its x86 secondary architecture.

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

ARCH=${ARCH:-x86_64}
RENKU=${RENKU:-$HOME/renku}
case "$ARCH" in
	x86_64)
		G=${GEN:-$RENKU/generated}
		PKG_ARCH=x86_64
		S=				# package name suffix
		A=				# architecture subdirectory in packages
		CROSS=$G/cross-tools-x86_64/bin/x86_64-unknown-haiku-
		DEVEL=$G/objects/haiku/x86_64/packaging/packages_build/regular/hpkg_-haiku_devel.hpkg/contents/develop
		DEVEL_LIB=$DEVEL/lib
		REL=$G/objects/haiku/x86_64/release
		;;
	x86_gcc2h)
		G=${GEN:-$RENKU/generated.x86_gcc2h}
		PKG_ARCH=x86_gcc2
		S=_x86
		A=/x86
		CROSS=$G/cross-tools-x86/bin/i586-pc-haiku-
		P=$G/objects/haiku/x86_gcc2/packaging/packages_build/regular
		DEVEL=$P/hpkg_-haiku_devel.hpkg/contents/develop
		DEVEL_LIB=$P/hpkg_-haiku_x86_devel.hpkg/contents/develop/lib/x86
		REL=$G/objects/haiku/x86/release
		;;
	*)
		echo "ARCH must be x86_64 or x86_gcc2h" >&2
		exit 1
		;;
esac
CXX=${CROSS}g++
STRIP=${CROSS}strip
HOST_TOOLS=$G/objects/linux/x86_64/release/tools
PACKAGE="env LD_LIBRARY_PATH=$G/objects/linux/lib $HOST_TOOLS/package/package"
ADDATTR="env LD_LIBRARY_PATH=$G/objects/linux/lib $HOST_TOOLS/addattr/addattr"
MOC=${MOC:-/usr/lib/qt6/libexec/moc}
HAIKUPORTS=https://eu.hpkg.haiku-os.org/haikuports/master/$PKG_ARCH/current/packages

mkdir -p "$WORK" "$OUT"
cd "$WORK"

# Prints the file of a HaikuPorts package: the one the RenkuOS build
# downloaded, or, when it took only the runtime package and not the _devel
# one, that same version from the repository.
hp_pkg() {
	local file version base
	file=$(ls "$G"/download/$1-[0-9]*-$PKG_ARCH.hpkg 2>/dev/null | head -1)
	if [ -z "$file" ]; then
		base=${1%_devel}
		version=$(ls "$G"/download/$base-[0-9]*-$PKG_ARCH.hpkg | head -1 \
			| sed "s|.*/$base-||; s|-$PKG_ARCH\.hpkg$||")
		file=$WORK/$1-$version-$PKG_ARCH.hpkg
		[ -f "$file" ] || curl -fsSL -o "$file" "$HAIKUPORTS/$1-$version-$PKG_ARCH.hpkg"
	fi
	echo "$file"
}

# ---- the HaikuPorts packages ----------------------------------------------------
QT_PKG=qt6_base$S-$QT_VERSION-$QT_REVISION-$PKG_ARCH.hpkg
QT_DEVEL_PKG=qt6_base${S}_devel-$QT_VERSION-$QT_REVISION-$PKG_ARCH.hpkg
for p in $QT_PKG $QT_DEVEL_PKG; do
	[ -f "$p" ] || curl -fsSL -o "$p" "$HAIKUPORTS/$p"
done
extract() {
	[ -d "x/$2" ] || { mkdir -p "x/$2"; $PACKAGE extract -C "x/$2" "$1"; }
}
extract "$QT_PKG" qt
extract "$QT_DEVEL_PKG" qt_devel
extract "$(hp_pkg freetype$S)" freetype
for p in freetype mesa glu; do
	extract "$(hp_pkg $p${S}_devel)" ${p}_devel
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
Q=$WORK/x/qt_devel/develop/headers$A/Qt6
QI="-I$Q"
for m in QtCore QtGui QtWidgets; do
	QI="$QI -I$Q/$m -I$Q/$m/$QT_VERSION -I$Q/$m/$QT_VERSION/$m"
done
QI="$QI -I$WORK/x/mesa_devel/develop/headers$A"
QI="$QI -I$WORK/x/mesa_devel/develop/headers$A/opengl"
QI="$QI -I$WORK/x/glu_devel/develop/headers$A"
QI="$QI -I$WORK/x/freetype_devel/develop/headers$A/freetype2"
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
	-B"$DEVEL_LIB" -L"$DEVEL_LIB" -L"$WORK/x/qt/lib$A" -L"$REL/kits" \
	-L"$REL/kits/tracker" -L"$REL/kits/game" -L"$REL/system/libroot" \
	-L"$(ls -d "$G"/build_packages/mesa$S-*/lib$A)" \
	-L"$(ls -d "$G"/build_packages/gcc${S}_syslibs-*/lib$A 2>/dev/null \
		|| ls -d "$G"/build_packages/gcc_syslibs-*/lib$A)" \
	-lQt6Widgets -lQt6Gui -lQt6Core -lbe -ltracker -lgame -lGL \
	"$WORK/x/freetype/lib$A/libfreetype.so.6" -lgcc_s
"$STRIP" "$PLUGIN"

# ---- the package --------------------------------------------------------------------
PLUGIN_DIR=add-ons$A/Qt6/platforms
DOC_DIR=data/doc/qt6_base$S
STAGE=$WORK/stage
rm -rf "$STAGE"
mkdir -p "$STAGE/$PLUGIN_DIR" "$STAGE/$DOC_DIR"
install -m 755 "$PLUGIN" "$STAGE/$PLUGIN_DIR/libqhaiku.so"
$ADDATTR -t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
	"$STAGE/$PLUGIN_DIR/libqhaiku.so"
# The source of the change, as the LGPL and the GPL ask.
cp "$PATCH" "$STAGE/$DOC_DIR/"
sed -e "s/^version\t.*/version\t\t\t$NEW_VERSION/" \
	-e "s/^packager\t.*/packager\t\t\"RenkuOS\"/" \
	-e 's/^\(summary\t*\)"\(.*\)"/\1"\2 (inline input methods)"/' \
	"$WORK/x/qt/.PackageInfo" > "$STAGE/.PackageInfo"

RESULT=$OUT/qt6_base$S-$NEW_VERSION-$PKG_ARCH.hpkg
cp "$WORK/$QT_PKG" "$RESULT"
chmod u+w "$RESULT"
$PACKAGE add -f -C "$STAGE" "$RESULT" .PackageInfo "$PLUGIN_DIR/libqhaiku.so" \
	"$DOC_DIR"
echo "-> $RESULT"
