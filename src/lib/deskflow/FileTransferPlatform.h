/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

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

} // namespace deskflow::transfer::platform
