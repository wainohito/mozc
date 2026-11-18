/*
 * Copyright 2026 RenkuOS contributors.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Mozc input_server method of x86_gcc2 hybrids.
 *
 * The input_server there is built with gcc 2, so its add-ons must be too, and
 * Mozc cannot be. This add-on is the part that has to live in the
 * input_server: it starts mozc_im (lib/x86/mozc/mozc_im), hands it the keys,
 * and makes the BInputServerMethod calls mozc_im asks for. See
 * ../input_method/relay_protocol.h. It is built with gcc 2 and kept to C++98
 * for that reason.
 *
 * While mozc_im is not running, keys go to the application unchanged.
 *
 * mozc_im (and with it mozc_server) is started with the input_server only
 * when the preferred language is Japanese. Otherwise nothing of Mozc runs
 * until it is chosen as the input method (Command+Space or the Deskbar).
 */

#include <Autolock.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Input.h>
#include <LocaleRoster.h>
#include <Locker.h>
#include <Looper.h>
#include <Menu.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <Roster.h>
#include <add-ons/input_server/InputServerMethod.h>

#include <string.h>

#include "../input_method/relay_protocol.h"


// From ../input_method/messages.h, which is C++11.
static const uint32 kMsgMethodActivated = 'MZac';
static const uint32 kMsgMethodLoaded = 'MZld';

static const uint32 kMsgLaunch = 'MZln';

// The input_server must not wait long for mozc_im.
static const bigtime_t kSendTimeout = 250000;
// mozc_im is started again at most this often when it is gone.
static const bigtime_t kRelaunchInterval = 5000000;

static const int32 kIconSize = 16 * 16;


class MozcRelayMethod;


class RelayLooper : public BLooper {
public:
								RelayLooper(MozcRelayMethod* method);
	virtual						~RelayLooper();

	virtual	void				MessageReceived(BMessage* message);

			// These may be called from any thread.
			status_t			SendToIM(BMessage* message);
			void				SetActive(bool active);
			void				RequestLaunch();
			void				QuitIM();

private:
			void				_Launch();
			void				_SetMenu(BMessage* message);

			MozcRelayMethod*	fMethod;
			BLocker				fLock;
			BMessenger			fTarget;
			bool				fActive;
			bigtime_t			fLastLaunch;
			BMenu*				fMenu;
};


class MozcRelayMethod : public BInputServerMethod {
public:
								MozcRelayMethod();
	virtual						~MozcRelayMethod();

	virtual	status_t			InitCheck();
	virtual	filter_result		Filter(BMessage* message, BList* outList);
	virtual	status_t			MethodActivated(bool active);

private:
			RelayLooper*		fLooper;
};


static const uchar*
blank_icon()
{
	static uchar icon[kIconSize];
	memset(icon, B_TRANSPARENT_MAGIC_CMAP8, sizeof(icon));
	return icon;
}


/*!	Whether the first preferred language of the user is Japanese ("ja",
	"ja_JP", ...), as chosen in the Locale preferences or the welcome window.
*/
static bool
japanese_is_preferred()
{
	BLocaleRoster* roster = BLocaleRoster::Default();
	BMessage languages;
	const char* language;
	if (roster == NULL || roster->GetPreferredLanguages(&languages) != B_OK
		|| languages.FindString("language", 0, &language) != B_OK) {
		return false;
	}
	return strncmp(language, "ja", 2) == 0
		&& (language[2] == '\0' || language[2] == '_' || language[2] == '-');
}


//	#pragma mark - RelayLooper


RelayLooper::RelayLooper(MozcRelayMethod* method)
	:
	BLooper("Mozc relay"),
	fMethod(method),
	fLock("Mozc relay target"),
	fActive(false),
	fLastLaunch(-kRelaunchInterval),
	fMenu(NULL)
{
}


RelayLooper::~RelayLooper()
{
	delete fMenu;
}


status_t
RelayLooper::SendToIM(BMessage* message)
{
	BMessenger target;
	{
		BAutolock _(fLock);
		target = fTarget;
	}
	if (!target.IsValid())
		return B_BAD_VALUE;
	return target.SendMessage(message, (BHandler*)NULL, kSendTimeout);
}


void
RelayLooper::SetActive(bool active)
{
	{
		BAutolock _(fLock);
		fActive = active;
	}
	BMessage message(kMsgMethodActivated);
	message.AddBool("active", active);
	if (SendToIM(&message) != B_OK && active)
		RequestLaunch();
}


void
RelayLooper::RequestLaunch()
{
	PostMessage(kMsgLaunch);
}


void
RelayLooper::QuitIM()
{
	BMessenger(MOZC_IM_SIGNATURE).SendMessage(B_QUIT_REQUESTED);
}


void
RelayLooper::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgLaunch:
			_Launch();
			break;

		case kMozcRelayHello:
		{
			BMessenger looper;
			if (message->FindMessenger("looper", &looper) != B_OK)
				break;
			bool active;
			{
				BAutolock _(fLock);
				fTarget = looper;
				active = fActive;
			}
			BMessage loaded(kMsgMethodLoaded);
			SendToIM(&loaded);
			if (active) {
				BMessage activated(kMsgMethodActivated);
				activated.AddBool("active", true);
				SendToIM(&activated);
			}
			break;
		}

		case kMozcRelayEnqueue:
		{
			BMessage* event = new BMessage;
			if (message->FindMessage("message", event) != B_OK
				|| fMethod->EnqueueMessage(event) != B_OK) {
				delete event;
			}
			break;
		}

		case kMozcRelaySetIcon:
		{
			const void* icon;
			ssize_t size;
			if (message->FindData("icon", B_RAW_TYPE, &icon, &size) == B_OK
				&& size == kIconSize) {
				fMethod->SetIcon((const uchar*)icon);
			}
			break;
		}

		case kMozcRelaySetMenu:
			_SetMenu(message);
			break;

		default:
			BLooper::MessageReceived(message);
			break;
	}
}


void
RelayLooper::_Launch()
{
	{
		BAutolock _(fLock);
		if (fTarget.IsValid())
			return;
	}
	bigtime_t now = system_time();
	if (now - fLastLaunch < kRelaunchInterval)
		return;
	fLastLaunch = now;

	// mozc_im is in the x86 lib directory of the installation location of
	// this add-on, the system or the home folder.
	char path[B_PATH_NAME_LENGTH];
	if (find_path_etc((const void*)&blank_icon, NULL, "x86",
			B_FIND_PATH_LIB_DIRECTORY, MOZC_IM_RELATIVE_PATH,
			B_FIND_PATH_EXISTING_ONLY, path, sizeof(path)) != B_OK) {
		strlcpy(path, "/boot/system/lib/x86/" MOZC_IM_RELATIVE_PATH,
			sizeof(path));
	}

	entry_ref ref;
	if (get_ref_for_path(path, &ref) != B_OK)
		return;

	// mozc_im answers with kMozcRelayHello; it is single launch, so when it
	// runs already the message reaches the running one.
	BMessage registration(kMozcRelayRegister);
	registration.AddMessenger("relay", BMessenger(this));
	be_roster->Launch(&ref, &registration);
}


void
RelayLooper::_SetMenu(BMessage* message)
{
	BMenu* menu = NULL;
	BMessage archive;
	if (message->FindMessage("menu", &archive) == B_OK) {
		BArchivable* object = BMenu::Instantiate(&archive);
		if (object != NULL)
			menu = static_cast<BMenu*>(object);
	}
	BMessenger target;
	message->FindMessenger("target", &target);

	// The input_server keeps the pointer, so the old menu goes only after
	// the new one replaced it.
	fMethod->SetMenu(menu, target);
	delete fMenu;
	fMenu = menu;
}


//	#pragma mark - MozcRelayMethod


MozcRelayMethod::MozcRelayMethod()
	:
	BInputServerMethod("Mozc", blank_icon()),
	fLooper(NULL)
{
}


MozcRelayMethod::~MozcRelayMethod()
{
	SetMenu(NULL, BMessenger());
	if (fLooper != NULL) {
		fLooper->QuitIM();
		if (fLooper->Lock())
			fLooper->Quit();
	}
}


status_t
MozcRelayMethod::InitCheck()
{
	if (fLooper == NULL) {
		fLooper = new RelayLooper(this);
		fLooper->Run();
		// Otherwise it is started when Mozc is chosen (SetActive(), Filter()).
		if (japanese_is_preferred())
			fLooper->RequestLaunch();
	}
	return B_OK;
}


filter_result
MozcRelayMethod::Filter(BMessage* message, BList* outList)
{
	if (message->what != B_KEY_DOWN && message->what != B_UNMAPPED_KEY_DOWN)
		return B_DISPATCH_MESSAGE;

	// mozc_im decides, and sends the key back if Mozc has no use for it.
	if (fLooper == NULL || fLooper->SendToIM(message) != B_OK) {
		if (fLooper != NULL)
			fLooper->RequestLaunch();
		return B_DISPATCH_MESSAGE;
	}
	return B_SKIP_MESSAGE;
}


status_t
MozcRelayMethod::MethodActivated(bool active)
{
	if (fLooper != NULL)
		fLooper->SetActive(active);
	return BInputServerMethod::MethodActivated(active);
}


extern "C" _EXPORT BInputServerMethod*
instantiate_input_method()
{
	return new MozcRelayMethod();
}
