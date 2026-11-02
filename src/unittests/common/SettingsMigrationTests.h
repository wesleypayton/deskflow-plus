/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class SettingsMigrationTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void deskflowPath_namesAndFolders();
  void pointAtOwnFolders_rewritesPaths();
  void copySettingsFile_onlyOnce();
  void migrate_copiesCertificatesNotServiceSettings();
  void migrate_portableKeepsOldSettings();
};
