/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"

#include <QObject>
#include <QSet>

class QLocalSocket;

namespace deskflow::core::ipc {

class CoreIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit CoreIpcServer(QObject *parent);

  static CoreIpcServer &instance();

  //! Send a command to the GUI acting for the signed in user, if one is connected; not queued for later
  void sendToUserShell(const QString &command, const QString &args);

protected:
  void clientDisconnected(QLocalSocket *clientSocket) override;

private:
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;

  QLocalSocket *m_userShell = nullptr;
};

} // namespace deskflow::core::ipc
