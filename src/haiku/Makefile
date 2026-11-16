# Copyright 2026 RenkuOS contributors.
# SPDX-License-Identifier: BSD-3-Clause
#
# Cross build of Mozc for Haiku/RenkuOS (x86_64).
#
#   make gen        host (Linux) Bazel build, then prepare $(OUT)/sources.mk,
#                   the generated files and the third party sources
#   make            mozc_server, the input_server add-on and mozc_tool
#   make hpkg       $(OUT)/mozc-<version>-x86_64.hpkg
#
# The RenkuOS tree in $(RENKU) must have been built (generated/ must exist).

MOZC_SRC := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
HAIKU_DIR := $(MOZC_SRC)/haiku
OUT ?= $(MOZC_SRC)/out_haiku
OBJ_DIR := $(OUT)/obj
GEN_DIR := $(OUT)/gen
ABSL_DIR := $(OUT)/third_party/abseil-cpp
PROTOBUF_DIR := $(OUT)/third_party/protobuf

BAZEL ?= bazelisk
BAZEL_FLAGS ?= --config oss_linux

RENKU ?= $(HOME)/renku
GEN := $(RENKU)/generated
CROSS := $(GEN)/cross-tools-x86_64/bin/x86_64-unknown-haiku-
CXX := $(CROSS)g++
CC := $(CROSS)gcc
DEVEL := $(GEN)/objects/haiku/x86_64/packaging/packages_build/regular/hpkg_-haiku_devel.hpkg/contents/develop
HDR := $(DEVEL)/headers
REL := $(GEN)/objects/haiku/x86_64/release
GCCLIB := $(wildcard $(GEN)/build_packages/gcc_syslibs-*-x86_64/lib)
ZLIB := $(wildcard $(GEN)/build_packages/zlib-*-x86_64)
HOST_TOOLS := $(GEN)/objects/linux/x86_64/release/tools
HOST_LIBS := $(GEN)/objects/linux/lib
RC := $(HOST_TOOLS)/rc/rc
XRES := $(HOST_TOOLS)/xres
ADDATTR := env LD_LIBRARY_PATH=$(HOST_LIBS) $(HOST_TOOLS)/addattr/addattr
PACKAGE := env LD_LIBRARY_PATH=$(HOST_LIBS) $(HOST_TOOLS)/package/package

HAIKU_INC := -idirafter $(HDR) -idirafter $(HDR)/posix -idirafter $(HDR)/bsd \
	-idirafter $(HDR)/gnu -idirafter $(HDR)/os \
	$(foreach d,$(wildcard $(HDR)/os/*/),-idirafter $(d)) \
	-idirafter $(HDR)/os/add-ons/input_server -idirafter $(HDR)/os/add-ons/graphics \
	-idirafter $(HDR)/private/shared -idirafter $(HDR)/private/input \
	-idirafter $(ZLIB)/develop/headers
HAIKU_LIBDIRS := -B$(DEVEL)/lib -L$(DEVEL)/lib -L$(REL)/kits \
	-L$(REL)/kits/locale -L$(REL)/system/libroot \
	-L$(REL)/system/libnetwork -L$(GCCLIB) \
	-L$(ZLIB)/develop/lib

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

-include $(OUT)/sources.mk

ifeq ($(wildcard $(OUT)/sources.mk),)
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
INPUT_SERVER := $(REL)/servers/input/input_server
ADDON_SOURCES := candidate_window.cc deskbar_menu.cc key_translator.cc \
	mode_icon.cc mozc_looper.cc mozc_method.cc strings.cc
ADDON_OBJECTS := $(patsubst %.cc,$(OBJ_DIR)/haiku/input_method/%.o,$(ADDON_SOURCES))

$(OBJ_DIR)/haiku/input_method/%.o: $(HAIKU_DIR)/input_method/%.cc
	$(call COMPILE_MOZC,-DMOZC_BUILD -MMD -MP)

-include $(ADDON_OBJECTS:.o=.d)

$(ADDON): $(ADDON_OBJECTS) $(CLIENT_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  LINK $@"
	@$(CXX) -shared -Wl,-z,defs $(LDFLAGS) $^ $(HAIKU_LIBDIRS) $(INPUT_SERVER) \
		-lbe -lnetwork -lz -lgcc_s -o $@

addon: $(ADDON)

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
PKG_VERSION := $(MOZC_VERSION)-1
HPKG := $(OUT)/mozc-$(PKG_VERSION)-x86_64.hpkg
PKG_ROOT := $(OUT)/hpkg
STRIP := $(CROSS)strip

$(HPKG): $(SERVER) $(ADDON) $(TOOL) $(HAIKU_DIR)/packaging/PackageInfo.in
	rm -rf $(PKG_ROOT) $@
	mkdir -p $(PKG_ROOT)/add-ons/input_server/methods $(PKG_ROOT)/lib/mozc \
		$(PKG_ROOT)/lib/mozc/documents $(PKG_ROOT)/data/licenses
	$(STRIP) -o $(PKG_ROOT)/add-ons/input_server/methods/Mozc $(ADDON)
	$(STRIP) -o $(PKG_ROOT)/lib/mozc/mozc_server $(SERVER)
	$(STRIP) -o $(PKG_ROOT)/lib/mozc/mozc_tool $(TOOL)
	$(ADDATTR) -t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
		$(PKG_ROOT)/lib/mozc/mozc_server
	$(ADDATTR) -t mime BEOS:TYPE application/x-vnd.Be-elfexecutable \
		$(PKG_ROOT)/lib/mozc/mozc_tool
	cp $(MOZC_SRC)/data/installer/credits_en.html $(PKG_ROOT)/lib/mozc/documents/
	cp $(MOZC_SRC)/../LICENSE "$(PKG_ROOT)/data/licenses/Mozc"
	sed 's/@VERSION@/$(PKG_VERSION)/g' $(HAIKU_DIR)/packaging/PackageInfo.in \
		> $(PKG_ROOT)/.PackageInfo
	$(PACKAGE) create -C $(PKG_ROOT) $@
	@echo "-> $@"

hpkg: $(HPKG)

# ---- bundle -----------------------------------------------------------------------
# Mozc, TerminalJ+ and the qt6_base with inline input, with an installer.
#   make bundle TERMINALJ=<terminalj project> QT6_BASE=<qt6_base hpkg>

TERMINALJ ?= $(HOME)/renkuapp/terminalj
QT6_BASE ?= $(OUT)/qt6_base-6.10.3.1-1-x86_64.hpkg
BUNDLE_NAME := Mozc-for-RenkuOS-$(MOZC_VERSION)
BUNDLE := $(OUT)/$(BUNDLE_NAME).zip

bundle: $(HPKG)
	$(MAKE) -C $(TERMINALJ) hpkg
	rm -rf $(OUT)/bundle $(BUNDLE)
	mkdir -p "$(OUT)/bundle/$(BUNDLE_NAME)"
	cp $(HPKG) $(TERMINALJ)/build/terminalj_plus-*-x86_64.hpkg $(QT6_BASE) \
		"$(OUT)/bundle/$(BUNDLE_NAME)/"
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
