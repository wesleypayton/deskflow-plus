/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class UpdaterTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void newerRelease_picksNewest();
  void newerRelease_noneWhenCurrent();
  void newerRelease_skipsUnusable();
  void newerRelease_badJson();
  void newerRelease_testBuildsOnlyWhenAsked();
  void findRelease_exactVersion();
  void isTestBuild_fourthNumber();
  void mode_roundTrips();
  void download_keepsMatchingFile();
  void download_dropsChangedFile();
  void download_dropsWrongSize();
};
