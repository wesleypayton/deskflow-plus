/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "RememberedServersTests.h"

#include "deskflow/RememberedServers.h"

using namespace deskflow::rememberedServers;

void RememberedServersTests::remembersAndLooksUp()
{
  const auto entries = remember({}, "Fenway_Group", "192.168.1.20");
  QCOMPARE(entries, QStringList{"Fenway_Group=192.168.1.20"});
  QCOMPARE(lookup(entries, "Fenway_Group"), "192.168.1.20");
  QCOMPARE(lookup(entries, " fenway_group "), "192.168.1.20");
  QVERIFY(lookup(entries, "other").isEmpty());
}

void RememberedServersTests::newestFirstAndReplaced()
{
  auto entries = remember({}, "a", "10.0.0.1");
  entries = remember(entries, "b", "10.0.0.2");
  entries = remember(entries, "A", "10.0.0.3");
  QCOMPARE(entries, (QStringList{"A=10.0.0.3", "b=10.0.0.2"}));
  QCOMPARE(lookup(entries, "a"), "10.0.0.3");

  // remembering what's already first changes nothing, so nothing gets written
  QCOMPARE(remember(entries, "A", "10.0.0.3"), entries);
}

void RememberedServersTests::ignoresAddressesAndBlanks()
{
  const QStringList entries{"a=10.0.0.1"};
  QCOMPARE(remember(entries, "192.168.1.20", "192.168.1.20"), entries);
  QCOMPARE(remember(entries, "", "10.0.0.9"), entries);
  QCOMPARE(remember(entries, "b", ""), entries);
  QCOMPARE(remember(entries, "b=c", "10.0.0.9"), entries);
}

void RememberedServersTests::keepsAFew()
{
  QStringList entries;
  for (int i = 0; i < kMaxEntries + 3; ++i)
    entries = remember(entries, QStringLiteral("server%1").arg(i), QStringLiteral("10.0.0.%1").arg(i));
  QCOMPARE(entries.size(), kMaxEntries);
  QCOMPARE(
      lookup(entries, QStringLiteral("server%1").arg(kMaxEntries + 2)), QStringLiteral("10.0.0.%1").arg(kMaxEntries + 2)
  );
  QVERIFY(lookup(entries, "server0").isEmpty());
}

QTEST_MAIN(RememberedServersTests)
