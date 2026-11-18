// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// mozc_im: the Mozc input method of x86_gcc2 hybrids, as a process of its
// own. The input_server there is built with gcc 2 and cannot load Mozc, so a
// small gcc 2 add-on (../relay) starts this, sends it the keys, and makes the
// BInputServerMethod calls it asks for. See relay_protocol.h.

#include <Application.h>
#include <Autolock.h>
#include <Locker.h>
#include <Menu.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>

#include "haiku/input_method/method_host.h"
#include "haiku/input_method/mode_icon.h"
#include "haiku/input_method/mozc_looper.h"
#include "haiku/input_method/relay_protocol.h"

namespace mozc {
namespace haiku {
namespace {

// The input_server's event thread waits for the relay; it is never long.
constexpr bigtime_t kSendTimeout = 1000000;
constexpr bigtime_t kCheckInterval = 2000000;
enum : uint32 { kMsgCheckRelay = 'MZck' };

// Sends the calls of the looper to the relay add-on in the input_server.
class RelayHost : public MethodHost {
 public:
  void SetRelay(const BMessenger& relay) {
    BAutolock lock(lock_);
    relay_ = relay;
  }
  bool HasRelay() {
    BAutolock lock(lock_);
    return relay_.IsValid();
  }

  status_t EnqueueMessage(BMessage* message) override {
    BMessage relayed(kMozcRelayEnqueue);
    relayed.AddMessage("message", message);
    const status_t status = Send(&relayed);
    if (status == B_OK) {
      delete message;
    }
    return status;
  }

  status_t SetIcon(const uchar* icon) override {
    BMessage relayed(kMozcRelaySetIcon);
    relayed.AddData("icon", B_RAW_TYPE, icon, kModeIconSize * kModeIconSize);
    return Send(&relayed);
  }

  status_t SetMenu(const BMenu* menu, BMessenger target) override {
    BMessage relayed(kMozcRelaySetMenu);
    if (menu != nullptr) {
      BMessage archive;
      if (menu->Archive(&archive, true) == B_OK) {
        relayed.AddMessage("menu", &archive);
      }
    }
    relayed.AddMessenger("target", target);
    return Send(&relayed);
  }

 private:
  status_t Send(BMessage* message) {
    BMessenger relay;
    {
      BAutolock lock(lock_);
      relay = relay_;
    }
    return relay.SendMessage(message, static_cast<BHandler*>(nullptr),
                             kSendTimeout);
  }

  BLocker lock_{"mozc_im relay"};
  BMessenger relay_;
};

class MozcImApplication : public BApplication {
 public:
  MozcImApplication()
      : BApplication(MOZC_IM_SIGNATURE),
        looper_(nullptr),
        check_runner_(nullptr) {}

  ~MozcImApplication() override { delete check_runner_; }

  void MessageReceived(BMessage* message) override {
    switch (message->what) {
      case kMozcRelayRegister: {
        BMessenger relay;
        if (message->FindMessenger("relay", &relay) != B_OK) {
          break;
        }
        host_.SetRelay(relay);
        if (looper_ == nullptr) {
          looper_ = new MozcLooper(&host_);
          looper_->Run();
        }
        if (check_runner_ == nullptr) {
          BMessage check(kMsgCheckRelay);
          check_runner_ = new BMessageRunner(BMessenger(this), &check,
                                             kCheckInterval);
        }
        // The add-on answers with kMsgMethodLoaded, which (re)sends the
        // icon and the menu.
        BMessage hello(kMozcRelayHello);
        hello.AddMessenger("looper", BMessenger(looper_));
        relay.SendMessage(&hello, static_cast<BHandler*>(nullptr),
                          kSendTimeout);
        break;
      }
      case kMsgCheckRelay:
        // The input_server is gone (restarted, or the add-on unloaded);
        // the next one starts a new mozc_im.
        if (!host_.HasRelay()) {
          PostMessage(B_QUIT_REQUESTED);
        }
        break;
      default:
        BApplication::MessageReceived(message);
        break;
    }
  }

  bool QuitRequested() override {
    if (looper_ != nullptr && looper_->Lock()) {
      // QuitRequested() saves the user data.
      looper_->QuitRequested();
      looper_->Quit();
      looper_ = nullptr;
    }
    return true;
  }

 private:
  RelayHost host_;
  MozcLooper* looper_;
  BMessageRunner* check_runner_;
};

}  // namespace
}  // namespace haiku
}  // namespace mozc

int main() {
  mozc::haiku::MozcImApplication application;
  application.Run();
  return 0;
}
