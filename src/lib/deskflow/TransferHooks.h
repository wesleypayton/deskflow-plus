/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/TransferAgent.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace deskflow::transfer {

//! What a computer remembers between transfers
struct LocalTransferState
{
  //! files received for the clipboard, so putting them there isn't mistaken for the user copying them
  std::vector<std::filesystem::path> receivedClipboardFiles;

  //! the incoming drag lands in a folder the user can't see, so show it when done
  bool revealDrop = false;

  //! files found in the current drag have been offered
  bool dragOffered = false;

  //! when the drop target was last shown, which nudges the cursor; see probeForDrag()
  std::chrono::steady_clock::time_point lastProbe;
};

//! Hooks that put incoming files where this computer wants them
/*!
Copied files go to a fresh inbox and onto the clipboard; dragged files go to
the folder or desktop under the cursor, or to a Deskflow folder that is then
opened.
*/
TransferAgent::Hooks
platformHooks(std::function<void(const TransferMessage &)> send, std::shared_ptr<LocalTransferState> state);

//! Offer the files on the clipboard, or withdraw the offer if it now holds something else
void offerClipboardFiles(TransferAgent &agent, const LocalTransferState &state);

//! Look for a file drag under the cursor, or stop looking
/*!
Once the drag is found its files are offered and the drag is ended locally, on
a drop target that accepts nothing, so the source program neither moves nor
copies anything. Returns true once the files have been offered.
*/
bool probeForDrag(TransferAgent &agent, LocalTransferState &state, bool look);

} // namespace deskflow::transfer
