// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_INPUT_METHOD_METHOD_HOST_H_
#define MOZC_HAIKU_INPUT_METHOD_METHOD_HOST_H_

#include <Messenger.h>
#include <SupportDefs.h>

class BMenu;
class BMessage;

namespace mozc {
namespace haiku {

// What the MozcLooper needs of the input_server method: the three calls of
// BInputServerMethod it makes. On x86_64 the looper runs in the input_server
// and this is the method itself (InputServerMethodHost). On x86_gcc2 hybrids
// the input_server is built with gcc 2 and cannot load Mozc, so the looper
// runs in mozc_im, a separate process, and this relays the calls to a small
// gcc 2 add-on (relay/) that makes them in the input_server.
class MethodHost {
 public:
  virtual ~MethodHost() = default;

  // Takes ownership of `message` when it succeeds, as
  // BInputServerMethod::EnqueueMessage() does.
  virtual status_t EnqueueMessage(BMessage* message) = 0;
  // A 16x16 B_CMAP8 icon.
  virtual status_t SetIcon(const uchar* icon) = 0;
  // The menu is not kept; the target receives the messages of its items.
  virtual status_t SetMenu(const BMenu* menu, BMessenger target) = 0;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_INPUT_METHOD_METHOD_HOST_H_
