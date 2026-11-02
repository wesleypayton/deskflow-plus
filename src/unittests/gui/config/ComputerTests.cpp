/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"

void ComputerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ComputerTests::basicFunctionality()
{
  Computer computer;
  QVERIFY(computer.isNull());

  computer.setName("stub");
  QVERIFY(!computer.isNull());

  computer.saveSettings(Settings::proxy());
  computer.loadSettings(Settings::proxy());
  QCOMPARE("stub", computer.name());
}

void ComputerTests::layoutRoundTrip()
{
  Computer computer("laptop");
  QVERIFY(!computer.hasLayout());

  computer.setLayoutRect(QRect(-1920, 180, 1920, 1080));
  computer.setMonitors({QRect(0, 0, 1280, 1080), QRect(1280, 300, 640, 480)});

  computer.saveSettings(Settings::proxy());
  Computer loaded;
  loaded.loadSettings(Settings::proxy());
  QCOMPARE(loaded.layoutRect(), computer.layoutRect());
  QCOMPARE(loaded.monitors(), computer.monitors());
  QVERIFY(loaded == computer);
}

void ComputerTests::setMonitorsKeepsPosition()
{
  Computer computer("desk");
  computer.setLayoutRect(QRect(500, 200, 1920, 1080));

  // monitors to the left of and above the primary have negative coordinates
  computer.setMonitors({QRect(-2560, -360, 2560, 1440), QRect(0, 0, 1920, 1080)});
  QCOMPARE(computer.layoutRect(), QRect(500, 200, 4480, 1440));
  QCOMPARE(computer.monitors().at(0), QRect(0, 0, 2560, 1440));
  QCOMPARE(computer.monitors().at(1), QRect(2560, 360, 1920, 1080));

  // with no monitors known it is one monitor covering the whole computer
  Computer single("single");
  single.setLayoutRect(QRect(0, 0, 1920, 1080));
  QCOMPARE(single.monitors(), QList<QRect>{QRect(0, 0, 1920, 1080)});
}

QTEST_MAIN(ComputerTests)
