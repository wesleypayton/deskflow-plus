/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// Before 1.0, Deskflow Plus kept its settings with Deskflow's, under Deskflow's name, so the two
// couldn't be set up differently. Since 1.0 they're kept under Deskflow Plus's name, and the first
// time it runs it copies what it had, pointing any paths in them at its own folders. Deskflow's are
// left as they were.
namespace deskflow::settingsMigration {

//! Where Deskflow Plus keeps its settings now
struct Locations
{
  QString userFile;
  QString systemFile;
  QString systemDir;
  QString stateFile;
  //! the portable settings file, empty if this platform has none
  QString portableFile;
  //! the setting with the user's settings file in the system's settings
  QString daemonConfigKey;
};

//! Copies what isn't there yet. As the service, that's the system's settings, and the user's settings
//! it starts the core with; as the app, the user's settings and the certificates and server settings.
void migrate(const Locations &locations);

//! Where a file was kept before 1.0: in Deskflow's folder, and named for Deskflow
QString deskflowPath(const QString &path);

//! \p text with paths to the folders Deskflow Plus used before 1.0 pointing at its own
QString pointAtOwnFolders(QString text, const QString &systemDir);

//! Copies a settings file, pointing its paths at Deskflow Plus's folders, unless \p to is there already
bool copySettingsFile(const QString &from, const QString &to, const QString &systemDir);

} // namespace deskflow::settingsMigration
