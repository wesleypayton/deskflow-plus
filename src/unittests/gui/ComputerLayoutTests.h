/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ComputerLayoutTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  // Test are run in order top to bottom
  void initTestCase();
  void legacyGridBecomesTouchingLayout();
  void writesLayoutSection();
  void dialogShowsEveryComputer();
  void dragMovesComputer();
  void dragOntoAnotherSpringsBack();

private:
  inline static const QString m_settingsPath = QStringLiteral("tmp/layout");
  inline static const QString m_settingsFile = QStringLiteral("%1/Deskflow.conf").arg(m_settingsPath);
  inline static const QString m_stateFile = QStringLiteral("%1/Deskflow.state").arg(m_settingsPath);
};
