/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/TransferHooks.h"

#include "base/Log.h"
#include "deskflow/FileTransferPlatform.h"

namespace deskflow::transfer {

namespace {

// how often the drop target is put back under the cursor while looking for a drag
constexpr auto kProbeInterval = std::chrono::milliseconds(300);

} // namespace

TransferAgent::Hooks
platformHooks(std::function<void(const TransferMessage &)> send, std::shared_ptr<LocalTransferState> state)
{
  TransferAgent::Hooks hooks;
  hooks.send = std::move(send);

  hooks.destination = [state](TransferKind kind) {
    if (kind == TransferKind::Clipboard)
      return platform::clipboardInbox();

    // the button was just released here, so the cursor marks where the files were dropped
    const auto folder = platform::dropFolderAtCursor();
    state->revealDrop = folder.reveal;
    if (!folder.path.empty())
      LOG_INFO("receiving dropped files in %s", pathToUtf8(folder.path).c_str());
    return folder.path;
  };

  hooks.received = [state](TransferKind kind, const std::vector<std::filesystem::path> &items) {
    if (kind == TransferKind::Drag) {
      LOG_INFO("received %d dropped items", static_cast<int>(items.size()));
      if (state->revealDrop)
        platform::revealInFileManager(items);
      return;
    }

    state->receivedClipboardFiles = items;
    if (platform::putFilesOnClipboard(items))
      LOG_INFO("received %d copied items, ready to paste", static_cast<int>(items.size()));
  };

  hooks.failed = [](const std::string &reason) { LOG_WARN("file transfer failed: %s", reason.c_str()); };
  return hooks;
}

void offerClipboardFiles(TransferAgent &agent, const LocalTransferState &state)
{
  const auto files = platform::filesOnClipboard();
  if (files.empty()) {
    agent.withdraw(TransferKind::Clipboard);
  } else if (files != state.receivedClipboardFiles) {
    agent.offer(TransferKind::Clipboard, files);
  }
}

bool probeForDrag(TransferAgent &agent, LocalTransferState &state, bool look)
{
  if (!look) {
    platform::stopDragProbe();
    state.dragOffered = false;
    state.lastProbe = {};
    return false;
  }

  if (state.dragOffered)
    return true;

  // showing the drop target nudges the cursor, which comes back here as a move, so don't do it every time
  if (const auto now = std::chrono::steady_clock::now(); now - state.lastProbe > kProbeInterval) {
    state.lastProbe = now;
    platform::startDragProbe();
  }

  const auto files = platform::dragProbeFiles();
  if (files.empty() || agent.offer(TransferKind::Drag, files) == 0)
    return false;

  LOG_INFO("carrying %d dragged items to another computer", static_cast<int>(files.size()));
  state.dragOffered = true;
  platform::endLocalDrag();
  return true;
}

} // namespace deskflow::transfer
