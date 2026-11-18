// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_MOZC_METHOD_H_
#define MOZC_HAIKU_INPUT_METHOD_MOZC_METHOD_H_

#include <Messenger.h>
#include <add-ons/input_server/InputServerMethod.h>

namespace mozc {
namespace haiku {

class MozcLooper;
class MethodHost;

// The input_server add-on. It hands key presses to the MozcLooper, which
// does everything else in its own thread.
class MozcMethod : public BInputServerMethod {
 public:
  MozcMethod();
  ~MozcMethod() override;

  status_t InitCheck() override;
  filter_result Filter(BMessage* message, BList* out_list) override;
  status_t MethodActivated(bool active) override;

 private:
  MethodHost* host_;
  MozcLooper* looper_;
  BMessenger messenger_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_MOZC_METHOD_H_
