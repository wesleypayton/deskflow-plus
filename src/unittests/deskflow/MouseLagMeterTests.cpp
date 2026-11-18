/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MouseLagMeterTests.h"

#include "deskflow/MouseLagMeter.h"

using deskflow::MouseLagMeter;
using namespace std::chrono_literals;

namespace {

// somewhere after the clock's zero, which the meter reads as "never"
const MouseLagMeter::Clock::time_point kStart{1h};

// a run of moves read in one go
void batch(MouseLagMeter &meter, MouseLagMeter::Clock::time_point when, int moves)
{
  for (int i = 0; i < moves; ++i)
    meter.moveArrived(when);
  meter.batchEnded();
}

} // namespace

void MouseLagMeterTests::steadyMovesAreQuiet()
{
  MouseLagMeter meter;
  auto now = kStart;
  for (int i = 0; i < 500; ++i, now += 8ms)
    batch(meter, now, 1);

  // a fast mouse delivers a few at a time without anything being wrong
  for (int i = 0; i < 500; ++i, now += 4ms)
    batch(meter, now, 5);

  QVERIFY(!meter.takeReport(now));
}

void MouseLagMeterTests::pausingTheMouseIsQuiet()
{
  MouseLagMeter meter;
  auto now = kStart;
  batch(meter, now, 1);

  // the hand stops for a moment and carries on: a gap, but the moves after it come one by one
  now += 300ms;
  batch(meter, now, 1);
  now += 8ms;
  batch(meter, now, 2);

  // the mouse lies still for a while, then is flicked
  now += 10s;
  batch(meter, now, 8);

  QVERIFY(!meter.takeReport(now));
}

void MouseLagMeterTests::heldUpMovesAreReported()
{
  MouseLagMeter meter;
  auto now = kStart;
  batch(meter, now, 1);

  now += 120ms;
  batch(meter, now, 14);
  now += 60ms;
  batch(meter, now, 6);

  const auto report = meter.takeReport(now);
  QVERIFY(report.has_value());
  QCOMPARE(report->lateBursts, 2);
  QCOMPARE(report->longestGapMs, 120);
  QCOMPARE(report->mostMovesAtOnce, 14);
  QCOMPARE(report->slowSteps, 0);
}

void MouseLagMeterTests::slowStepsAreReported()
{
  MouseLagMeter meter;
  meter.stepTook(1ms);
  meter.stepTook(24ms);
  QVERIFY(!meter.takeReport(kStart));

  meter.stepTook(30ms);
  meter.stepTook(90ms);
  const auto report = meter.takeReport(kStart);
  QVERIFY(report.has_value());
  QCOMPARE(report->slowSteps, 2);
  QCOMPARE(report->slowestStepMs, 90);
  QCOMPARE(report->lateBursts, 0);
}

void MouseLagMeterTests::reportsAreSpacedOut()
{
  MouseLagMeter meter;
  meter.stepTook(50ms);
  QVERIFY(meter.takeReport(kStart).has_value());

  // more trouble straight after waits for the next report, and isn't lost
  meter.stepTook(60ms);
  QVERIFY(!meter.takeReport(kStart + 1s));
  meter.stepTook(70ms);
  const auto report = meter.takeReport(kStart + MouseLagMeter::kReportEvery);
  QVERIFY(report.has_value());
  QCOMPARE(report->slowSteps, 2);
  QCOMPARE(report->slowestStepMs, 70);
}

QTEST_MAIN(MouseLagMeterTests)
