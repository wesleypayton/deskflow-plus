/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DiscoveryTests.h"

#include "common/Discovery.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QUdpSocket>

using namespace deskflow::discovery;

void DiscoveryTests::answer_roundTrips()
{
  const auto server = readAnswer(answer(QStringLiteral("Fenway_Group"), 24800), QHostAddress("192.168.0.114"));
  QVERIFY(server.has_value());
  QCOMPARE(server->name, QStringLiteral("Fenway_Group"));
  QCOMPARE(server->address, QHostAddress("192.168.0.114"));
  QCOMPARE(server->port, quint16{24800});

  // an IPv4 sender given in IPv6 form
  QCOMPARE(
      readAnswer(answer(QStringLiteral("a"), 1), QHostAddress("::ffff:10.0.0.2"))->address, QHostAddress("10.0.0.2")
  );
}

void DiscoveryTests::readAnswer_refusesOthers()
{
  const QHostAddress from("10.0.0.2");
  QVERIFY(!readAnswer(question(), from).has_value());
  QVERIFY(!readAnswer(QByteArrayLiteral("Deskflow Plus server 1\n"), from).has_value());
  QVERIFY(!readAnswer(QByteArrayLiteral("Deskflow Plus server 1\n0\nname"), from).has_value());
  QVERIFY(!readAnswer(QByteArrayLiteral("Deskflow Plus server 1\n24800\n"), from).has_value());
  QVERIFY(!readAnswer(QByteArrayLiteral("something else"), from).has_value());
}

void DiscoveryTests::finder_findsResponder()
{
  // a port nothing else is likely to use, answered on this computer
  constexpr quint16 kPort = 24871;
  Responder responder(QStringLiteral("test-server"), kPort);

  // broadcasts may not loop back on every network, so ask the responder directly as well
  Finder finder;
  QSignalSpy finished(&finder, &Finder::finished);
  finder.find(kPort, 1000);
  QUdpSocket direct;
  QSignalSpy readable(&direct, &QUdpSocket::readyRead);
  direct.writeDatagram(question(), QHostAddress::LocalHost, kPort);
  QVERIFY(readable.wait(2000));
  const auto reply = direct.receiveDatagram();
  QCOMPARE(readAnswer(reply.data(), reply.senderAddress())->name, QStringLiteral("test-server"));

  QVERIFY(finished.wait(3000));
}

QTEST_MAIN(DiscoveryTests)
