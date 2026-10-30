/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "common/Constants.h"
#include "common/ShellIntegration.h"
#include "deskflow/App.h"
#include "deskflow/ipc/UserShell.h"

#include <QLocalSocket>

namespace deskflow::core::ipc {

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(QObject *parent) : IpcServer(parent, kCoreIpcName, QStringLiteral("core"))
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  Q_UNUSED(parts)
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  if (command == QStringLiteral("reloadConfig")) {
    // the GUI rewrote the server config, e.g. because a client's monitors changed
    LOG_DEBUG("core ipc server got reload config message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    auto *events = App::instance().getEvents();
    events->addEvent(Event(EventTypes::ServerAppReloadConfig, events->getSystemTarget()));
    return;
  }

  // the GUI acting for the signed in user, see deskflow::shell
  const auto args = parts.size() >= 2 ? parts.at(1) : QString();
  if (command == QStringLiteral("shellHelper")) {
    const auto folders = deskflow::shell::decodePaths(args);
    m_userShell = clientSocket;
    userShell::helperConnected(folders.empty() ? std::filesystem::path() : folders.front());
    return;
  }
  if (command == QStringLiteral("shellDragFiles")) {
    userShell::setDragFiles(deskflow::shell::decodePaths(args));
    return;
  }
  if (command == QStringLiteral("shellDropFolder")) {
    // id|path|reveal
    const auto fields = args.split('|');
    if (fields.size() == 3) {
      const auto paths = deskflow::shell::decodePaths(fields.at(1));
      userShell::deliverDropFolder(
          fields.at(0).toULongLong(), {paths.empty() ? std::filesystem::path() : paths.front(), fields.at(2) == "1"}
      );
    }
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

void CoreIpcServer::sendToUserShell(const QString &command, const QString &args)
{
  if (m_userShell == nullptr)
    return;
  writeToClientSocket(m_userShell, args.isEmpty() ? command : QStringLiteral("%1=%2").arg(command, args));
  m_userShell->flush();
}

void CoreIpcServer::clientDisconnected(QLocalSocket *clientSocket)
{
  if (clientSocket == m_userShell) {
    m_userShell = nullptr;
    userShell::helperDisconnected();
  }
}

} // namespace deskflow::core::ipc
