/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "UpdateRelease.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkAccessManager;
class QWidget;

// Checks the GitHub releases for a newer version, at startup and once a day after, and does what
// the update setting says: asks (Always, Yes, No or Never), installs, or does nothing. Installing
// downloads the setup, checks it against the release, and runs it, which asks Windows for
// permission, closes the app, upgrades it and starts it again.
class Updater : public QObject
{
  Q_OBJECT
public:
  explicit Updater(QWidget *window);

  //! Starts checking, as the update setting says
  void start();

  //! Checks now and says what it found, whatever the update setting is
  void checkNow(QWidget *parent);

  //! Offers to install the newer version that was found
  void offerUpdate(QWidget *parent);

Q_SIGNALS:
  void updateFound(const QString &version);
  //! A check from checkNow finished
  void checkFinished();
  //! The installer is running, and the app should quit so it isn't waited on
  void installerStarted();

private:
  void scheduledCheck();
  void check(QWidget *parent, bool manual);
  void askToUpdate(const deskflow::gui::updates::Release &release);
  bool confirmInstall(const deskflow::gui::updates::Release &release, QWidget *parent) const;
  void install(const deskflow::gui::updates::Release &release, QWidget *parent);
  static bool runSetup(const QString &path);

  QWidget *m_window;
  QNetworkAccessManager *m_network;
  QTimer m_nextCheck;
  std::optional<deskflow::gui::updates::Release> m_newer;
  bool m_asking = false;
  bool m_installing = false;
};
