/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"

#include "base/Log.h"
#include "common/ShellIntegration.h"
#include "deskflow/ipc/UserShell.h"

// these act for the signed in user: done here when this process runs as that user, otherwise by the GUI

namespace deskflow::transfer::platform {

namespace userShell = deskflow::core::ipc::userShell;

void startDragProbe()
{
  if (userShell::delegated()) {
    userShell::send(QStringLiteral("shellDragProbe"), QStringLiteral("show"));
  } else if (userShell::actsForUser() && !shell::showDropProbe()) {
    LOG_WARN("couldn't show the drag probe");
  }
}

std::vector<std::filesystem::path> dragProbeFiles()
{
  if (userShell::delegated())
    return userShell::dragFiles();
  return userShell::actsForUser() ? shell::dropProbeFiles() : std::vector<std::filesystem::path>{};
}

void endLocalDrag()
{
  if (userShell::delegated())
    userShell::send(QStringLiteral("shellDragProbe"), QStringLiteral("release"));
  else if (userShell::actsForUser())
    shell::releaseOverDropProbe();
}

void stopDragProbe()
{
  if (userShell::delegated()) {
    userShell::send(QStringLiteral("shellDragProbe"), QStringLiteral("hide"));
    userShell::setDragFiles({});
  } else if (userShell::actsForUser()) {
    shell::hideDropProbe();
  }
}

DropFolder dropFolderAtCursor()
{
  if (userShell::delegated()) {
    if (const auto folder = userShell::dropFolder(); folder)
      return *folder;
    LOG_WARN("the Deskflow Plus window didn't say where to drop the files");
    return {};
  }
  if (!userShell::actsForUser()) {
    LOG_WARN("can't drop files while running as SYSTEM unless the Deskflow Plus window is open");
    return {};
  }
  return shell::dropFolderAtCursor();
}

void revealInFileManager(const std::vector<std::filesystem::path> &items)
{
  if (userShell::delegated())
    userShell::send(QStringLiteral("shellReveal"), shell::encodePaths(items));
  else if (userShell::actsForUser())
    shell::revealInFileManager(items);
}

} // namespace deskflow::transfer::platform
