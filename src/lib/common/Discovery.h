/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

class QTimer;
class QUdpSocket;

// Finding Deskflow Plus servers on the local network. A client broadcasts a question to the
// server's port, over UDP, and each server running there answers with its name. This only fills in
// where to connect: the connection itself is checked as usual, fingerprints included.
namespace deskflow::discovery {

//! A server that answered
struct Server
{
  QString name;
  QHostAddress address;
  quint16 port = 0;

  bool operator==(const Server &) const = default;
};

//! What a client asks
QByteArray question();

//! A server's answer, with its name and the port it takes connections on
QByteArray answer(const QString &name, quint16 port);

//! The server in \p datagram, an answer that came from \p from, if it's one
std::optional<Server> readAnswer(const QByteArray &datagram, const QHostAddress &from);

//! Answers clients looking for servers, while the server runs
class Responder : public QObject
{
  Q_OBJECT
public:
  Responder(const QString &name, quint16 port, QObject *parent = nullptr);

private:
  void read();

  QUdpSocket *m_socket;
  QByteArray m_answer;
};

//! Asks the local network for servers, and lists the ones that answer in time
class Finder : public QObject
{
  Q_OBJECT
public:
  explicit Finder(QObject *parent = nullptr);

  //! Asks on every network this computer is on, and reports what answered once \p waitMs is up
  void find(quint16 port, int waitMs = 1500);

Q_SIGNALS:
  void finished(const QList<deskflow::discovery::Server> &servers);

private:
  void read();

  QUdpSocket *m_socket;
  QTimer *m_wait;
  QList<Server> m_servers;
};

} // namespace deskflow::discovery
