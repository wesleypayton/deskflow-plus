/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/RemoteInput.h"

// remote input is only implemented on Windows

namespace deskflow::remote::platform {

bool startWatching(std::function<void()>)
{
  return false;
}

void stopWatching()
{
}

void setCapture(bool)
{
}

std::vector<RemoteInput> takeInput()
{
  return {};
}

void playBack(const RemoteInput &)
{
}

bool moveCursorBy(int32_t, int32_t, int32_t &, int32_t &)
{
  return false;
}

} // namespace deskflow::remote::platform
