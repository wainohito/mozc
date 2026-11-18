// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/input_method/mozc_method.h"

#include <GraphicsDefs.h>
#include <Input.h>
#include <Message.h>

#include "haiku/input_method/messages.h"
#include "haiku/input_method/method_host.h"
#include "haiku/input_method/mode_icon.h"
#include "haiku/input_method/mozc_looper.h"

extern "C" _EXPORT BInputServerMethod* instantiate_input_method() {
  return new mozc::haiku::MozcMethod();
}

namespace mozc {
namespace haiku {
namespace {

// The add-on may be loaded before the app_server can draw the real icon;
// the looper sets it as soon as it runs.
const ModeIcon& InitialIcon() {
  static const ModeIcon icon = [] {
    ModeIcon blank;
    blank.fill(B_TRANSPARENT_MAGIC_CMAP8);
    return blank;
  }();
  return icon;
}

// The method itself is the host of the looper when both are in the
// input_server.
class InputServerMethodHost : public MethodHost {
 public:
  explicit InputServerMethodHost(BInputServerMethod* method)
      : method_(method) {}

  status_t EnqueueMessage(BMessage* message) override {
    return method_->EnqueueMessage(message);
  }
  status_t SetIcon(const uchar* icon) override {
    return method_->SetIcon(icon);
  }
  status_t SetMenu(const BMenu* menu, BMessenger target) override {
    return method_->SetMenu(menu, target);
  }

 private:
  BInputServerMethod* method_;
};

}  // namespace

MozcMethod::MozcMethod()
    : BInputServerMethod("Mozc", InitialIcon().data()),
      host_(new InputServerMethodHost(this)),
      looper_(nullptr) {}

MozcMethod::~MozcMethod() {
  SetMenu(nullptr, BMessenger());
  if (looper_ != nullptr && looper_->Lock()) {
    // QuitRequested() saves the user data.
    looper_->QuitRequested();
    looper_->Quit();
  }
  delete host_;
}

status_t MozcMethod::InitCheck() {
  if (looper_ == nullptr) {
    looper_ = new MozcLooper(host_);
    looper_->Run();
    messenger_ = BMessenger(looper_);
    messenger_.SendMessage(kMsgMethodLoaded);
  }
  return B_OK;
}

filter_result MozcMethod::Filter(BMessage* message, BList* out_list) {
  if (message->what != B_KEY_DOWN && message->what != B_UNMAPPED_KEY_DOWN) {
    return B_DISPATCH_MESSAGE;
  }
  // The looper decides, and sends the key back if Mozc has no use for it.
  if (messenger_.SendMessage(message) != B_OK) {
    return B_DISPATCH_MESSAGE;
  }
  return B_SKIP_MESSAGE;
}

status_t MozcMethod::MethodActivated(bool active) {
  BMessage message(kMsgMethodActivated);
  message.AddBool("active", active);
  messenger_.SendMessage(&message);
  return BInputServerMethod::MethodActivated(active);
}

}  // namespace haiku
}  // namespace mozc
