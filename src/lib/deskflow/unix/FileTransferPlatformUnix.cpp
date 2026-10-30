/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"

// file transfer is only implemented on Windows

namespace deskflow::transfer::platform {

std::vector<std::filesystem::path> filesOnClipboard()
{
  return {};
}

bool putFilesOnClipboard(const std::vector<std::filesystem::path> &)
{
  return false;
}

std::filesystem::path clipboardInbox()
{
  return {};
}

void startDragProbe()
{
}

std::vector<std::filesystem::path> dragProbeFiles()
{
  return {};
}

void endLocalDrag()
{
}

void stopDragProbe()
{
}

DropFolder dropFolderAtCursor()
{
  return {};
}

void revealInFileManager(const std::vector<std::filesystem::path> &)
{
}

} // namespace deskflow::transfer::platform
