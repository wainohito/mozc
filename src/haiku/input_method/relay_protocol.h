/*
 * Copyright 2026 RenkuOS contributors.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Messages between the Mozc relay add-on in the input_server and mozc_im, the
 * process that runs the input method on x86_gcc2 hybrids.
 *
 * The input_server of a gcc 2 hybrid is a gcc 2 binary and can only load
 * gcc 2 add-ons, while Mozc needs C++20. So the add-on (relay/) is a small
 * gcc 2 one: it starts mozc_im, hands it the keys, and makes the calls of
 * BInputServerMethod that mozc_im asks for. Everything else -- the conversion,
 * the candidate window, the Deskbar menu -- is in mozc_im, built for the x86
 * secondary architecture.
 *
 * This header is included by both, so it is plain C++98.
 */

#ifndef MOZC_HAIKU_INPUT_METHOD_RELAY_PROTOCOL_H_
#define MOZC_HAIKU_INPUT_METHOD_RELAY_PROTOCOL_H_

#define MOZC_IM_SIGNATURE "application/x-vnd.RenkuOS-mozc-im"

enum {
	// add-on -> mozc_im (the launch message): "relay" (messenger), where
	// mozc_im sends the calls below. Sent again when the add-on starts
	// mozc_im while it is running already.
	kMozcRelayRegister = 'MZrg',

	// mozc_im -> add-on: "looper" (messenger), where the add-on sends the
	// keys, kMsgMethodActivated and kMsgMethodLoaded of messages.h.
	kMozcRelayHello = 'MZhi',
	// mozc_im -> add-on: "message" (message), for EnqueueMessage().
	kMozcRelayEnqueue = 'MZeq',
	// mozc_im -> add-on: "icon" (B_RAW_TYPE, 16x16 B_CMAP8), for SetIcon().
	kMozcRelaySetIcon = 'MZic',
	// mozc_im -> add-on: "menu" (message, the archived BMenu; missing for
	// none), "target" (messenger), for SetMenu().
	kMozcRelaySetMenu = 'MZmn'
};

// The relay add-on looks for mozc_im here, relative to the lib directory of
// the x86 architecture of its installation location (lib/x86).
#define MOZC_IM_RELATIVE_PATH "mozc/mozc_im"

#endif  // MOZC_HAIKU_INPUT_METHOD_RELAY_PROTOCOL_H_
