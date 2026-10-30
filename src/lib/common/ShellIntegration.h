/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

#include <filesystem>
#include <vector>

//! The parts of file transfer that must run as the signed in user
/*!
Windows won't let a drag from Explorer reach a window owned by a more
privileged process, and a process running as SYSTEM can't see the user's
Explorer windows or folders. So when the core runs as SYSTEM (the background
service, so it can control the login screen), the GUI, which always runs as the
user, does these on its behalf. Only implemented on Windows.
*/
namespace deskflow::shell {

//! Where files dropped at the cursor should go
struct DropFolder
{
  std::filesystem::path path; //!< empty if files can't be dropped here
  bool reveal = false;        //!< not a folder the user can see, so open it once the files arrive
};

//! @name Dragging files out
//! A held drag is identified by putting an invisible drop target under the cursor:
//! the system's drag loop tells it what is being dragged.
//@{

//! Show the drop target under the cursor, if it isn't already; false if it can't be made
bool showDropProbe();

//! The files being dragged, once the drag has passed over the drop target
std::vector<std::filesystem::path> dropProbeFiles();

//! Release the mouse button locally so the drag ends on the drop target, which accepts nothing
void releaseOverDropProbe();

//! Hide the drop target and forget what it saw
void hideDropProbe();

//! Move the cursor out and back, so anything waiting for it to move looks again
void nudgeCursor();

//@}

//! The folder window or desktop under the cursor, otherwise a Deskflow folder in Downloads
DropFolder dropFolderAtCursor();

//! Open the file manager with these items selected
void revealInFileManager(const std::vector<std::filesystem::path> &items);

//! Paths as text for an IPC message: each one percent encoded, so none of the message separators appear
QString encodePaths(const std::vector<std::filesystem::path> &paths);
std::vector<std::filesystem::path> decodePaths(const QString &text);

} // namespace deskflow::shell
