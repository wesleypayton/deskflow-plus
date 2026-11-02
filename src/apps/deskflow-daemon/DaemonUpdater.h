/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class UpdateDownload;

// Installs updates for the app, so they need no permission prompt. The app only says which version
// to install: the list of releases, the setup and its SHA-256 all come from the releases page this
// was built with, and the setup is kept where only the system can write. Setup stops this service
// while it upgrades, so the new service starts the app again for the user, see
// restartAppAfterUpdate().
class DaemonUpdater : public QObject
{
  Q_OBJECT
public:
  explicit DaemonUpdater(QObject *parent = nullptr);

  //! Downloads and installs \p version, when it's a release newer than this one
  void install(const QString &version);
  void cancel();

  //! Starts the app for the signed in user, after an update this service installed
  static void restartAppAfterUpdate();

Q_SIGNALS:
  //! How it's going: downloading (with a percent), installing, cancelled or failed (with the reason)
  void status(const QString &state, const QString &detail);

private:
  void fail(const QString &reason);
  void runSetup(const QString &path);

  QNetworkAccessManager *m_network;
  QPointer<UpdateDownload> m_download;
  bool m_busy = false;
  bool m_cancelled = false;
};
