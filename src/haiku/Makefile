# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Cross build of Mozc for Haiku/RenkuOS.
#
#   make gen        host (Linux) Bazel build, then prepare $(SRC_OUT)/sources.mk,
#                   the generated files and the third party sources
#   make            mozc_server, the input_server add-on and mozc_tool
#   make hpkg       $(OUT)/mozc-<version>-<arch>.hpkg
#
# ARCH=x86_64 (the default) builds for x86_64 against a RenkuOS tree in
# $(RENKU) (generated/ must exist).
#
# ARCH=x86_gcc2h builds for the 32-bit x86_gcc2 hybrid against the tree in
# $(GEN) (default $(RENKU)/generated.x86_gcc2h). Its input_server is a gcc 2
# binary and cannot load Mozc, so the input_server add-on is the gcc 2 relay
# of relay/, and the method itself runs in mozc_im, a process built for the
# x86 secondary architecture, next to mozc_server and mozc_tool in lib/x86/mozc.
# `make gen` is the same for both; its output is shared.

ARCH ?= x86_64

MOZC_SRC := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
HAIKU_DIR := $(MOZC_SRC)/haiku
# What `make gen` makes: the source lists, generated files and third party
# sources, for every architecture.
SRC_OUT ?= $(MOZC_SRC)/out_haiku
ifeq ($(ARCH),x86_64)
OUT ?= $(SRC_OUT)
else
OUT ?= $(SRC_OUT)/$(ARCH)
endif
OBJ_DIR := $(OUT)/obj
GEN_DIR := $(SRC_OUT)/gen
ABSL_DIR := $(SRC_OUT)/third_party/abseil-cpp
PROTOBUF_DIR := $(SRC_OUT)/third_party/protobuf

BAZEL ?= bazelisk
BAZEL_FLAGS ?= --config oss_linux

RENKU ?= $(HOME)/renku
HOST_TOOLS = $(GEN)/objects/linux/x86_64/release/tools
HOST_LIBS = $(GEN)/objects/linux/lib

ifeq ($(ARCH),x86_64)
GEN ?= $(RENKU)/generated
PKG_ARCH := x86_64
CROSS := $(GEN)/cross-tools-x86_64/bin/x86_64-unknown-haiku-
DEVEL := $(GEN)/objects/haiku/x86_64/packaging/packages_build/regular/hpkg_-haiku_devel.hpkg/contents/develop
DEVEL_LIB := $(DEVEL)/lib
REL := $(GEN)/objects/haiku/x86_64/release
INPUT_SERVER := $(REL)/servers/input/input_server
GCCLIB := $(wildcard $(GEN)/build_packages/gcc_syslibs-*-x86_64/lib)
ZLIB := $(wildcard $(GEN)/build_packages/zlib-*-x86_64)
ZLIB_INC := $(ZLIB)/develop/headers
ZLIB_LIB := $(ZLIB)/develop/lib
# Where the binaries of lib/mozc go in the package.
PKG_LIB := lib/mozc
else ifeq ($(ARCH),x86_gcc2h)
GEN ?= $(RENKU)/generated.x86_gcc2h
PKG_ARCH := x86_gcc2
CROSS := $(GEN)/cross-tools-x86/bin/i586-pc-haiku-
CROSS_GCC2 := $(GEN)/cross-tools-x86_gcc2/bin/i586-pc-haiku-
DEVEL_PKGS := $(GEN)/objects/haiku/x86_gcc2/packaging/packages_build/regular
# The headers are the same for both architectures.
DEVEL := $(DEVEL_PKGS)/hpkg_-haiku_devel.hpkg/contents/develop
DEVEL_LIB := $(DEVEL_PKGS)/hpkg_-haiku_x86_devel.hpkg/contents/develop/lib/x86
DEVEL_LIB_GCC2 := $(DEVEL)/lib
REL := $(GEN)/objects/haiku/x86/release
REL_GCC2 := $(GEN)/objects/haiku/x86_gcc2/release
INPUT_SERVER := $(REL_GCC2)/servers/input/input_server
GCCLIB := $(wildcard $(GEN)/build_packages/gcc_x86_syslibs-*-x86_gcc2/lib/x86)
ZLIB := $(wildcard $(GEN)/build_packages/zlib_x86-*-x86_gcc2)
ZLIB_INC := $(ZLIB)/develop/headers/x86
ZLIB_LIB := $(ZLIB)/develop/lib/x86
PKG_LIB := lib/x86/mozc
else
$(error ARCH must be x86_64 or x86_gcc2h)
endif

CXX := $(CROSS)g++
CC := $(CROSS)gcc
HDR := $(DEVEL)/headers
RC := $(HOST_TOOLS)/rc/rc
XRES := $(HOST_TOOLS)/xres
ADDATTR := env LD_LIBRARY_PATH=$(HOST_LIBS) $(HOST_TOOLS)/addattr/addattr
PACKAGE := env LD_LIBRARY_PATH=$(HOST_LIBS) $(HOST_TOOLS)/package/package

HAIKU_INC := -idirafter $(HDR) -idirafter $(HDR)/posix -idirafter $(HDR)/bsd \
	-idirafter $(HDR)/gnu -idirafter $(HDR)/os \
	$(foreach d,$(wildcard $(HDR)/os/*/),-idirafter $(d)) \
	-idirafter $(HDR)/os/add-ons/input_server -idirafter $(HDR)/os/add-ons/graphics \
	-idirafter $(HDR)/private/shared -idirafter $(HDR)/private/input \
	-idirafter $(ZLIB_INC)
HAIKU_LIBDIRS := -B$(DEVEL_LIB) -L$(DEVEL_LIB) -L$(REL)/kits \
	-L$(REL)/kits/locale -L$(REL)/system/libroot \
	-L$(REL)/system/libnetwork -L$(GCCLIB) \
	-L$(ZLIB_LIB)

INCLUDES := -I$(MOZC_SRC) -I$(GEN_DIR) -I$(ABSL_DIR) \
	-I$(PROTOBUF_DIR)/src -I$(PROTOBUF_DIR)/third_party/utf8_range \
	$(HAIKU_INC)
CXXFLAGS := -std=c++20 -O2 -g1 -fPIC -funsigned-char -ffunction-sections \
	-fdata-sections -DNDEBUG
MOZC_WARNINGS := -Wall -Wno-sign-compare -Wno-char-subscripts \
	-Wno-deprecated-declarations
LDFLAGS := -Wl,--gc-sections

# Paths of the installed package, relative to the installation location.
VERSION = $(shell sed -n 's/^\(MAJOR\|MINOR\|BUILD\|REVISION\)=//p' \
	$(MOZC_SRC)/data/version/mozc_version_template.bzl 2>/dev/null | paste -sd.)

define COMPILE_MOZC
	@mkdir -p $(dir $@)
	@echo "  CXX  $<"
	@$(CXX) $(CXXFLAGS) $(MOZC_WARNINGS) $(INCLUDES) $(1) -c $< -o $@
endef

define COMPILE_THIRD_PARTY
	@mkdir -p $(dir $@)
	@echo "  CXX  $<"
	@$(CXX) $(CXXFLAGS) -w $(INCLUDES) $(1) -c $< -o $@
endef

all: server addon tool

-include $(SRC_OUT)/sources.mk

ifeq ($(wildcard $(SRC_OUT)/sources.mk),)
server addon tool:
	@echo "Run 'make gen' first." && false
else

# ---- mozc_server ---------------------------------------------------------------

SERVER := $(OUT)/bin/mozc_server

$(SERVER): $(SERVER_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CXX) $(LDFLAGS) $^ $(HAIKU_LIBDIRS) -lbe -lnetwork -lz -lgcc_s -o $@

server: $(SERVER)

# ---- input_server add-on ---------------------------------------------------------

ADDON := $(OUT)/bin/Mozc
ADDON_SOURCES := candidate_window.cc deskbar_menu.cc key_translator.cc \
	mode_icon.cc mozc_looper.cc strings.cc
ADDON_OBJECTS := $(patsubst %.cc,$(OBJ_DIR)/haiku/input_method/%.o,$(ADDON_SOURCES))

$(OBJ_DIR)/haiku/input_method/%.o: $(HAIKU_DIR)/input_method/%.cc
	$(call COMPILE_MOZC,-DMOZC_BUILD -MMD -MP)

-include $(ADDON_OBJECTS:.o=.d) $(OBJ_DIR)/haiku/input_method/mozc_method.d \
	$(OBJ_DIR)/haiku/input_method/mozc_im_main.d

ifeq ($(ARCH),x86_64)

$(ADDON): $(ADDON_OBJECTS) $(OBJ_DIR)/haiku/input_method/mozc_method.o \
		$(CLIENT_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CXX) -shared -Wl,-z,defs $(LDFLAGS) $^ $(HAIKU_LIBDIRS) $(INPUT_SERVER) \
		-lbe -lnetwork -lz -lgcc_s -o $@

addon: $(ADDON)

else

# mozc_im: the method, as a process of its own (see relay/).
MOZC_IM := $(OUT)/bin/mozc_im

$(OUT)/mozc_im.rsrc: $(HAIKU_DIR)/input_method/mozc_im.rdef
	@mkdir -p $(dir $@)
	$(RC) -o $@ $<

$(MOZC_IM): $(ADDON_OBJECTS) $(OBJ_DIR)/haiku/input_method/mozc_im_main.o \
		$(CLIENT_OBJECTS) $(OUT)/mozc_im.rsrc
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CXX) $(LDFLAGS) $(filter %.o,$^) $(HAIKU_LIBDIRS) \
		-lbe -lnetwork -lz -lgcc_s -o $@
	$(XRES) -o $@ $(OUT)/mozc_im.rsrc

# The relay add-on, with gcc 2 like the input_server. The gcc 2 driver does
# not find Haiku's glue code by itself; it is linked the way the Haiku build
# links gcc 2 add-ons (HAIKU_LIBRARY_*_GLUE_CODE in ArchitectureRules).
GCC2_LIB := $(wildcard $(GEN)/cross-tools-x86_gcc2/lib/gcc-lib/i586-pc-haiku/*)
GCC2_LIBDIRS := -L$(DEVEL_LIB_GCC2) -L$(REL_GCC2)/kits \
	-L$(REL_GCC2)/system/libroot
GCC2_BEGIN := $(DEVEL_LIB_GCC2)/crti.o $(GCC2_LIB)/crtbeginS.o \
	$(DEVEL_LIB_GCC2)/init_term_dyn.o
GCC2_END := $(GCC2_LIB)/crtendS.o $(DEVEL_LIB_GCC2)/crtn.o
RELAY_OBJECT := $(OBJ_DIR)/haiku/relay/mozc_relay_method.o

$(RELAY_OBJECT): $(HAIKU_DIR)/relay/mozc_relay_method.cpp \
		$(HAIKU_DIR)/input_method/relay_protocol.h
	@mkdir -p $(dir $@)
	@echo "  CXX2 $<"
	@$(CROSS_GCC2)g++ -O2 -fPIC -Wall -Wno-multichar $(HAIKU_INC) -c $< -o $@

$(ADDON): $(RELAY_OBJECT)
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CROSS_GCC2)g++ -nostdlib -shared -Xlinker --no-undefined \
		-Xlinker -soname=Mozc $(GCC2_BEGIN) $^ $(GCC2_LIBDIRS) \
		$(INPUT_SERVER) -lbe -lroot $(GCC2_LIB)/libgcc.a $(GCC2_END) -o $@

addon: $(ADDON) $(MOZC_IM)

endif

# ---- mozc_tool -------------------------------------------------------------------

TOOL := $(OUT)/bin/mozc_tool
TOOL_SOURCES := dictionary_store.cc dictionary_tool_window.cc \
	keymap_editor_window.cc mozc_tool_main.cc tool_strings.cc \
	word_register_window.cc
TOOL_OWN_OBJECTS := $(patsubst %.cc,$(OBJ_DIR)/haiku/tool/%.o,$(TOOL_SOURCES)) \
	$(OBJ_DIR)/haiku/input_method/key_translator.o
KEYMAP_STRINGS := $(GEN_DIR)/haiku/tool/keymap_strings.inc

$(KEYMAP_STRINGS): $(MOZC_SRC)/gui/config_dialog/keymap_ja.qtts \
		$(HAIKU_DIR)/build/gen_keymap_strings.py
	@mkdir -p $(dir $@)
	python3 -I $(HAIKU_DIR)/build/gen_keymap_strings.py $< $@

$(OBJ_DIR)/haiku/tool/keymap_editor_window.o: $(KEYMAP_STRINGS)

$(OBJ_DIR)/haiku/tool/%.o: $(HAIKU_DIR)/tool/%.cc
	$(call COMPILE_MOZC,-DMOZC_BUILD -MMD -MP -idirafter $(HDR)/private/interface)

-include $(TOOL_OWN_OBJECTS:.o=.d)

$(TOOL): $(TOOL_OWN_OBJECTS) $(TOOL_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CXX) $(LDFLAGS) $^ $(HAIKU_LIBDIRS) -L$(REL)/kits/tracker \
		-L$(REL)/kits/textencoding -lcolumnlistview -ltracker -ltextencoding \
		-lbe -lnetwork -lz -lgcc_s -o $@

tool: $(TOOL)

# ---- hpkg ------------------------------------------------------------------------

MOZC_VERSION := $(shell sed -n 's/.*"\([0-9]*\.[0-9]*\.[0-9]*\)\.[0-9]*".*/\1/p' \
	$(GEN_DIR)/base/version_def.h | head -1)
PKG_VERSION := $(MOZC_VERSION)-2
HPKG := $(OUT)/mozc-$(PKG_VERSION)-$(PKG_ARCH).hpkg
PKG_ROOT := $(OUT)/hpkg
STRIP := $(CROSS)strip
ifeq ($(ARCH),x86_64)
PKG_REQUIRES := haiku\n\tlib:libstdc++\n\tlib:libz
PKG_PROGRAMS := $(SERVER) $(TOOL)
else
PKG_REQUIRES := haiku\n\thaiku_x86\n\tlib:libstdc++_x86\n\tlib:libz_x86
PKG_PROGRAMS := $(SERVER) $(TOOL) $(MOZC_IM)
endif

$(HPKG): $(PKG_PROGRAMS) $(ADDON) $(HAIKU_DIR)/packaging/PackageInfo.in
	rm -rf $(PKG_ROOT) $@
	mkdir -p $(PKG_ROOT)/add-ons/input_server/methods $(PKG_ROOT)/$(PKG_LIB) \
		$(PKG_ROOT)/$(PKG_LIB)/documents $(PKG_ROOT)/data/licenses
	$(STRIP) -o $(PKG_ROOT)/add-ons/input_server/methods/Mozc $(ADDON)
	for p in $(PKG_PROGRAMS); do \
		$(STRIP) -o $(PKG_ROOT)/$(PKG_LIB)/$$(basename $$p) $$p || exit 1; \
		$(ADDATTR) -t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
			$(PKG_ROOT)/$(PKG_LIB)/$$(basename $$p) || exit 1; \
	done
ifneq ($(ARCH),x86_64)
	# strip drops the resources: the signature and B_BACKGROUND_APP.
	$(XRES) -o $(PKG_ROOT)/$(PKG_LIB)/mozc_im $(OUT)/mozc_im.rsrc
	$(ADDATTR) -t mime BEOS:APP_SIG application/x-vnd.RenkuOS-mozc-im \
		$(PKG_ROOT)/$(PKG_LIB)/mozc_im
endif
	cp $(MOZC_SRC)/data/installer/credits_en.html $(PKG_ROOT)/$(PKG_LIB)/documents/
	cp $(MOZC_SRC)/../LICENSE "$(PKG_ROOT)/data/licenses/Mozc"
	sed -e 's/@VERSION@/$(PKG_VERSION)/g' -e 's/@ARCH@/$(PKG_ARCH)/' \
		-e 's/@REQUIRES@/$(PKG_REQUIRES)/' \
		$(HAIKU_DIR)/packaging/PackageInfo.in > $(PKG_ROOT)/.PackageInfo
	$(PACKAGE) create -C $(PKG_ROOT) $@
	@echo "-> $@"

hpkg: $(HPKG)

# ---- bundle -----------------------------------------------------------------------
# Mozc, TerminalJ+, and the qt6_base and qthaikuplugins with inline input, with
# an installer.
#   make bundle TERMINALJ=<terminalj project> QT6_BASE=<qt6_base hpkg> \
#     QT5_PLUGINS=<qthaikuplugins hpkg>

TERMINALJ ?= $(HOME)/renkuapp/terminalj
ifeq ($(ARCH),x86_64)
QT6_BASE ?= $(OUT)/qt6_base-6.10.3.1-1-x86_64.hpkg
QT5_PLUGINS ?= $(OUT)/qthaikuplugins-5.15.16.0.1-1-x86_64.hpkg
BUNDLE_NAME := Mozc-for-RenkuOS-$(PKG_VERSION)
else
QT6_BASE ?= $(OUT)/qt6_base_x86-6.10.3.1-1-x86_gcc2.hpkg
QT5_PLUGINS ?= $(OUT)/qthaikuplugins_x86-5.15.16.0.1-1-x86_gcc2.hpkg
BUNDLE_NAME := Mozc-for-RenkuOS-$(PKG_VERSION)-$(PKG_ARCH)
endif
BUNDLE := $(OUT)/$(BUNDLE_NAME).zip

bundle: $(HPKG)
	$(MAKE) -C $(TERMINALJ) ARCH=$(ARCH) GEN=$(GEN) hpkg
	rm -rf $(OUT)/bundle $(BUNDLE)
	mkdir -p "$(OUT)/bundle/$(BUNDLE_NAME)"
	cp $(HPKG) $(TERMINALJ)/build/$(PKG_ARCH)/terminalj_plus-*-$(PKG_ARCH).hpkg $(QT6_BASE) \
		$(QT5_PLUGINS) "$(OUT)/bundle/$(BUNDLE_NAME)/"
	install -m 755 $(HAIKU_DIR)/packaging/bundle/install.sh \
		"$(OUT)/bundle/$(BUNDLE_NAME)/インストール"
	cp $(HAIKU_DIR)/packaging/bundle/README.txt \
		"$(OUT)/bundle/$(BUNDLE_NAME)/はじめにお読みください.txt"
	cd $(OUT)/bundle && zip -qry ../$(BUNDLE_NAME).zip $(BUNDLE_NAME)
	@echo "-> $(BUNDLE)"

endif

# ---- host preparation -----------------------------------------------------------

AQUERY = $(BAZEL) aquery $(BAZEL_FLAGS) 'mnemonic("CppCompile", $(1))' \
	--output=jsonproto > $(2)

TOOL_TARGETS := //client:client //config:config_handler \
	//client/user_dictionary:user_dictionary_importer \
	//client/user_dictionary:user_dictionary_storage \
	//data_manager:pos_list_provider //dictionary:user_dictionary_util \
	//dictionary:user_pos //base:init_mozc //base:process //session:keymap \
	//composer:key_parser
EMPTY :=
TOOL_DEPS := $(subst $(EMPTY) $(EMPTY), + ,$(foreach t,$(TOOL_TARGETS),deps($(t))))

gen:
	cd $(MOZC_SRC) && $(BAZEL) build $(BAZEL_FLAGS) //server:mozc_server \
		$(TOOL_TARGETS)
	@mkdir -p $(OUT)/aquery
	cd $(MOZC_SRC) && $(call AQUERY,deps(//server:mozc_server),$(OUT)/aquery/server.json)
	cd $(MOZC_SRC) && $(call AQUERY,deps(//client:client) + deps(//base:init_mozc),$(OUT)/aquery/client.json)
	cd $(MOZC_SRC) && $(call AQUERY,$(TOOL_DEPS),$(OUT)/aquery/tool.json)
	cd $(MOZC_SRC) && python3 -I $(HAIKU_DIR)/build/gen_sources.py \
		--bazel_bin=$$(readlink -f bazel-bin) \
		--external=$$($(BAZEL) info $(BAZEL_FLAGS) output_base)/external \
		--out=$(OUT) SERVER=$(OUT)/aquery/server.json \
		CLIENT=$(OUT)/aquery/client.json TOOL=$(OUT)/aquery/tool.json
	for p in $(HAIKU_DIR)/build/patches/abseil-cpp-*.patch; do \
		patch -d $(ABSL_DIR) -p1 < $$p || exit 1; done

clean:
	rm -rf $(OBJ_DIR) $(OUT)/bin $(OUT)/hpkg

.PHONY: all gen server addon tool hpkg bundle clean
