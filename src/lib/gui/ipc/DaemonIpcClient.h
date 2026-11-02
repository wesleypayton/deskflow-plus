/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcClient.h"

#include <QObject>

namespace deskflow::gui::ipc {

class DaemonIpcClient : public IpcClient
{
  Q_OBJECT

public:
  explicit DaemonIpcClient(QObject *parent = nullptr);
  void sendLogLevel(const QString &logLevel);
  void sendConfigFile(const QString &configFile);
  void sendStartProcess();
  void sendStopProcess();
  void sendClearSettings();
  void requestLogPath();
  //! Asks the service to download and install \p version, see DaemonUpdater
  void sendInstallUpdate(const QString &version);
  void sendCancelUpdate();

Q_SIGNALS:
  void logPathReceived(const QString &logPath);
  //! How the update is going: downloading (with a percent), installing, cancelled or failed (with the reason)
  void updateStatus(const QString &state, const QString &detail);

protected:
  void processCommand(const QString &command, const QStringList &parts) override;
};

} // namespace deskflow::gui::ipc
