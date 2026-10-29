/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/ShellIntegration.h"

#include <QString>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

//! Getting the GUI to do what must run as the signed in user, when the core runs as SYSTEM
/*!
See deskflow::shell. The GUI announces itself with a shellHelper command once
connected; the core then sends it shellDragProbe, shellDropFolder and
shellReveal commands, and it answers with shellDragFiles and shellDropFolder.
*/
namespace deskflow::core::ipc::userShell {

//! True if this process runs as the signed in user, so it can do these itself
bool actsForUser();

//! True if this process runs as SYSTEM and the GUI is connected to do these for it
bool delegated();

//! Send the GUI a command, from any thread
void send(const QString &command, const QString &args = {});

//! The files the GUI's drop target last saw being dragged
std::vector<std::filesystem::path> dragFiles();
void setDragFiles(std::vector<std::filesystem::path> files);

//! Ask the GUI where files dropped at the cursor should go, waiting a moment for the answer
/*!
Must not be called on the thread that reads the IPC socket, which has to be
free to receive the answer.
*/
std::optional<deskflow::shell::DropFolder> dropFolder();

//! The signed in user's temp folder, as the GUI reported it
std::filesystem::path tempFolder();

//! @name Called by the core's IPC server
//@{
void helperConnected(const std::filesystem::path &tempFolder);
void helperDisconnected();
void deliverDropFolder(uint64_t id, const deskflow::shell::DropFolder &folder);
//@}

} // namespace deskflow::core::ipc::userShell
