/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Discovery.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>

namespace deskflow::discovery {

namespace {

const auto kQuestion = QByteArrayLiteral("Deskflow Plus: which servers are here? 1");
const auto kAnswerPrefix = QByteArrayLiteral("Deskflow Plus server 1\n");

// a computer's name can't be longer, see Settings::cleanComputerName
constexpr qsizetype kMaxName = 255;

} // namespace

QByteArray question()
{
  return kQuestion;
}

QByteArray answer(const QString &name, quint16 port)
{
  return kAnswerPrefix + QByteArray::number(port) + '\n' + name.left(kMaxName).toUtf8();
}

std::optional<Server> readAnswer(const QByteArray &datagram, const QHostAddress &from)
{
  if (!datagram.startsWith(kAnswerPrefix))
    return {};

  const auto rest = datagram.mid(kAnswerPrefix.size());
  const auto lineEnd = rest.indexOf('\n');
  if (lineEnd < 0)
    return {};

  bool ok = false;
  const auto port = rest.left(lineEnd).toUShort(&ok);
  const auto name = QString::fromUtf8(rest.mid(lineEnd + 1)).trimmed();
  if (!ok || port == 0 || name.isEmpty() || name.size() > kMaxName)
    return {};

  // an IPv4 address can come in IPv6 form
  bool isIPv4 = false;
  const auto ipv4 = QHostAddress(from.toIPv4Address(&isIPv4));
  return Server{name, isIPv4 ? ipv4 : from, port};
}

Responder::Responder(const QString &name, quint16 port, QObject *parent)
    : QObject(parent),
      m_socket{new QUdpSocket(this)},
      m_answer{answer(name, port)}
{
  // the same port number as connections, so the firewall rule for the server covers it
  if (!m_socket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    qWarning("can't answer clients looking for servers: %s", qPrintable(m_socket->errorString()));
  connect(m_socket, &QUdpSocket::readyRead, this, &Responder::read);
}

void Responder::read()
{
  while (m_socket->hasPendingDatagrams()) {
    const auto datagram = m_socket->receiveDatagram();
    if (datagram.data() == kQuestion && !datagram.senderAddress().isNull())
      m_socket->writeDatagram(m_answer, datagram.senderAddress(), static_cast<quint16>(datagram.senderPort()));
  }
}

Finder::Finder(QObject *parent) : QObject(parent), m_socket{new QUdpSocket(this)}, m_wait{new QTimer(this)}
{
  m_wait->setSingleShot(true);
  connect(m_socket, &QUdpSocket::readyRead, this, &Finder::read);
  connect(m_wait, &QTimer::timeout, this, [this] {
    m_socket->close();
    Q_EMIT finished(m_servers);
  });
}

void Finder::find(quint16 port, int waitMs)
{
  m_servers.clear();
  m_socket->close();
  if (!m_socket->bind(QHostAddress::AnyIPv4, 0)) {
    qWarning("can't look for servers: %s", qPrintable(m_socket->errorString()));
    m_wait->start(0);
    return;
  }

  // each network's own broadcast address, as the general one only goes out on one of them
  QSet<QHostAddress> sent;
  for (const auto &interface : QNetworkInterface::allInterfaces()) {
    if (!(interface.flags() & QNetworkInterface::IsUp) || !(interface.flags() & QNetworkInterface::CanBroadcast))
      continue;
    for (const auto &entry : interface.addressEntries()) {
      if (const auto broadcast = entry.broadcast(); !broadcast.isNull() && !sent.contains(broadcast)) {
        sent.insert(broadcast);
        m_socket->writeDatagram(kQuestion, broadcast, port);
      }
    }
  }
  m_socket->writeDatagram(kQuestion, QHostAddress::Broadcast, port);
  m_wait->start(waitMs);
}

void Finder::read()
{
  while (m_socket->hasPendingDatagrams()) {
    const auto datagram = m_socket->receiveDatagram();
    if (const auto server = readAnswer(datagram.data(), datagram.senderAddress());
        server && !m_servers.contains(*server))
      m_servers.append(*server);
  }
}

} // namespace deskflow::discovery
