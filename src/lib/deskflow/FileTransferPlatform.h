/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/ShellIntegration.h"

#include <filesystem>
#include <vector>

//! What file transfer needs from the operating system
/*!
Only Windows is supported; elsewhere there are never any files on the
clipboard and incoming transfers are refused.
*/
namespace deskflow::transfer::platform {

//! The files and folders on the clipboard, if it holds a file list (copied in a file manager)
std::vector<std::filesystem::path> filesOnClipboard();

//! Put files on the clipboard, as if they had been copied in the file manager
/*!
Any text already on the clipboard is kept. The clipboard is marked as written
by Deskflow so it isn't mistaken for the user copying something new.
*/
bool putFilesOnClipboard(const std::vector<std::filesystem::path> &items);

//! A new, empty folder for files arriving for the clipboard, or empty if there's nowhere to put them
std::filesystem::path clipboardInbox();

//! @name Dragging files out
//! A held drag is identified by putting an invisible drop target under the cursor:
//! the system's drag loop tells it what is being dragged.
//@{

//! Show the drop target under the cursor, if it isn't already
void startDragProbe();

//! The files being dragged, once the drag has passed over the drop target
std::vector<std::filesystem::path> dragProbeFiles();

//! Release the mouse button locally so the drag ends on the drop target, which accepts nothing
void endLocalDrag();

//! Hide the drop target and forget what it saw
void stopDragProbe();

//@}

//! Where files dropped at the cursor should go
using DropFolder = deskflow::shell::DropFolder;

//! The folder window or desktop under the cursor, otherwise a Deskflow folder in Downloads
DropFolder dropFolderAtCursor();

//! Open the file manager with these items selected
void revealInFileManager(const std::vector<std::filesystem::path> &items);

} // namespace deskflow::transfer::platform
