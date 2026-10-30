/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

//! Using a client computer's own mouse and keyboard on the other computers
/*!
The client captures its own input and sends it to the server, which plays it
back as if it came from a mouse and keyboard plugged into the server. See
kMsgDRemoteInput.
*/
namespace deskflow::remote {

//! What a RemoteInput carries
enum class InputType : uint8_t
{
  Edge = 1, //!< a: side (a Direction), b: x, c: y
  Move,     //!< a: dx, b: dy
  Button,   //!< a: button (1 left, 2 right, 3 middle, 4 and 5 the side buttons), b: 1 pressed or 0 released
  Wheel,    //!< a: horizontal, b: vertical
  Key       //!< a: virtual key, b: scan code, c: kKeyExtended | kKeyReleased
};

inline constexpr int32_t kKeyExtended = 1;
inline constexpr int32_t kKeyReleased = 2;

//! One piece of input from a client's own mouse or keyboard
struct RemoteInput
{
  InputType type = InputType::Move;
  int32_t a = 0;
  int32_t b = 0;
  int32_t c = 0;
  int32_t d = 0;

  //! True for a button or key being let go, which is always passed on so nothing stays held down
  bool isRelease() const
  {
    return (type == InputType::Button && b == 0) || (type == InputType::Key && (c & kKeyReleased) != 0);
  }
};

//! What remote input needs from the operating system
/*!
Only Windows is supported; elsewhere nothing is watched and nothing is played back.
*/
namespace platform {

//! @name On the client
//@{

//! Start watching this computer's own mouse and keyboard
/*!
\p wake is called, from another thread, when input is waiting for takeInput().
Until setCapture() turns capturing on, the only input reported is the cursor
being pushed against an edge of the screen.
*/
bool startWatching(std::function<void()> wake);

//! Stop watching, and stop capturing
void stopWatching();

//! Start or stop keeping this computer's mouse and keyboard input from its applications
void setCapture(bool capture);

//! The input waiting to be sent
std::vector<RemoteInput> takeInput();

//@}

//! @name On the server
//@{

//! Play back a button, wheel or key from a client as if it happened here
void playBack(const RemoteInput &input);

//! Move this computer's cursor by dx,dy and return where it ended up
bool moveCursorBy(int32_t dx, int32_t dy, int32_t &x, int32_t &y);

//@}

} // namespace platform

} // namespace deskflow::remote
