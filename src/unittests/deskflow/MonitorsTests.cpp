/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorsTests.h"

#include "deskflow/Monitors.h"

using namespace deskflow;

void MonitorsTests::roundTrip()
{
  const MonitorList monitors = {{0, 0, 1920, 1080}, {1920, 0, 2560, 1440}};
  const auto data = encodeMonitors(monitors);
  QCOMPARE(data.size(), size_t{8});
  QVERIFY(decodeMonitors(data, 0, 0, 4480, 1440) == monitors);
}

void MonitorsTests::negativeCoordinates()
{
  // a monitor left of and above the primary has negative coordinates on Windows
  const MonitorList monitors = {{-1440, -360, 1440, 900}, {0, 0, 1920, 1080}};
  QVERIFY(decodeMonitors(encodeMonitors(monitors), -1440, -360, 3360, 1440) == monitors);
}

void MonitorsTests::rejectsMalformed()
{
  QVERIFY(decodeMonitors({}, 0, 0, 1920, 1080).empty());
  QVERIFY(decodeMonitors({0, 0, 1920}, 0, 0, 1920, 1080).empty());
  QVERIFY(decodeMonitors(encodeMonitors({{0, 0, 0, 1080}}), 0, 0, 1920, 1080).empty());
  QVERIFY(decodeMonitors(encodeMonitors({{0, 0, 1920, -1}}), 0, 0, 1920, 1080).empty());
}

void MonitorsTests::rejectsOutsideShape()
{
  QVERIFY(decodeMonitors(encodeMonitors({{0, 0, 1921, 1080}}), 0, 0, 1920, 1080).empty());
  QVERIFY(decodeMonitors(encodeMonitors({{-1, 0, 1920, 1080}}), 0, 0, 1920, 1080).empty());
  QVERIFY(decodeMonitors(encodeMonitors({{0, 0, 1920, 1080}, {0, 1080, 10, 10}}), 0, 0, 1920, 1080).empty());

  // an overflowing rectangle must not wrap around into the shape
  QVERIFY(decodeMonitors(encodeMonitors({{2147483000, 0, 2000, 1080}}), 0, 0, 1920, 1080).empty());
}

void MonitorsTests::rejectsTooMany()
{
  const MonitorList tooMany(kMaxMonitors + 1, MonitorRect{0, 0, 10, 10});
  QVERIFY(decodeMonitors(encodeMonitors(tooMany), 0, 0, 10, 10).empty());

  const MonitorList enough(kMaxMonitors, MonitorRect{0, 0, 10, 10});
  QCOMPARE(decodeMonitors(encodeMonitors(enough), 0, 0, 10, 10).size(), kMaxMonitors);
}

QTEST_MAIN(MonitorsTests)
