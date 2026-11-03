/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonIpcClient.h"

#include "common/Constants.h"

namespace deskflow::gui::ipc {

DaemonIpcClient::DaemonIpcClient(QObject *parent) : IpcClient(parent, kDaemonIpcName, QStringLiteral("daemon"))
{
}

void DaemonIpcClient::sendLogLevel(const QString &logLevel)
{
  sendMessage(QStringLiteral("logLevel=%1").arg(logLevel));
}

void DaemonIpcClient::sendConfigFile(const QString &configFile)
{
  sendMessage(QStringLiteral("configFile=%1").arg(configFile));
}

void DaemonIpcClient::sendStartProcess()
{
  sendMessage(QStringLiteral("start"));
}

void DaemonIpcClient::sendStopProcess()
{
  sendMessage(QStringLiteral("stop"));
}

void DaemonIpcClient::sendClearSettings()
{
  sendMessage(QStringLiteral("clearSettings"));
}

void DaemonIpcClient::requestLogPath()
{
  sendMessage(QStringLiteral("logPath"));
}

void DaemonIpcClient::sendInstallUpdate(const QString &version)
{
  sendMessage(QStringLiteral("installUpdate=%1").arg(version));
}

void DaemonIpcClient::sendCancelUpdate()
{
  sendMessage(QStringLiteral("cancelUpdate"));
}

void DaemonIpcClient::processCommand(const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("logPath") && parts.size() == 2) {
    Q_EMIT logPathReceived(parts.at(1));
  } else if (command == QStringLiteral("updateStatus") && parts.size() == 2) {
    Q_EMIT updateStatus(parts.at(1).section(QLatin1Char('|'), 0, 0), parts.at(1).section(QLatin1Char('|'), 1));
  }
}

} // namespace deskflow::gui::ipc
