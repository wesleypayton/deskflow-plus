/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerLayoutTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/widgets/ComputerLayoutView.h"

#include <QGraphicsItem>

namespace {

const Computer *find(const ServerConfig &config, const QString &name)
{
  for (const auto &computer : config.computers()) {
    if (computer.name() == name)
      return &computer;
  }
  return nullptr;
}

// the canvas item drawn at the given layout rectangle
QGraphicsItem *itemAt(const ComputerLayoutView *view, const QRect &rect)
{
  for (auto *item : view->scene()->items()) {
    if (item->sceneBoundingRect() == QRectF(rect))
      return item;
  }
  return nullptr;
}

// press on the middle of an item and drag it by some screen pixels
void drag(ComputerLayoutView *view, const QGraphicsItem *item, const QPoint &by)
{
  const auto start = view->mapFromScene(item->sceneBoundingRect().center());
  QTest::mousePress(view->viewport(), Qt::LeftButton, {}, start);
  QTest::mouseMove(view->viewport(), start + by / 2);
  QTest::mouseMove(view->viewport(), start + by);
  QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, start + by);
}

} // namespace

void ComputerLayoutTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  // settings from before the free-form layout: laptop left of the server and tv above it in the 5 wide grid
  QFile file(m_settingsFile);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
  file.write(R"([core]
coreMode=2
computerName=server
[computer_server]
name=server
[computer_laptop]
name=laptop
[computer_tv]
name=tv
[internalConfig]
screens\size=15
screens\3\name=tv
screens\7\name=laptop
screens\8\name=server
)");
  file.close();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ComputerLayoutTests::legacyGridBecomesTouchingLayout()
{
  ServerConfig config;
  const auto *server = find(config, "server");
  const auto *laptop = find(config, "laptop");
  const auto *tv = find(config, "tv");
  QVERIFY(server && laptop && tv);
  QVERIFY(server->hasLayout() && laptop->hasLayout() && tv->hasLayout());

  const auto &s = server->layoutRect();
  const auto &l = laptop->layoutRect();
  const auto &t = tv->layoutRect();

  // the laptop's right edge is the server's left edge, and they share some height
  QCOMPARE(l.x() + l.width(), s.x());
  QVERIFY(l.top() < s.bottom() && s.top() < l.bottom());

  // the tv's bottom edge is the server's top edge, and they share some width
  QCOMPARE(t.y() + t.height(), s.y());
  QVERIFY(t.left() < s.right() && s.left() < t.right());
}

void ComputerLayoutTests::writesLayoutSection()
{
  ServerConfig config;
  QString text;
  QTextStream stream(&text);
  stream << config;

  QVERIFY(text.contains("section: layout"));
  QVERIFY(!text.contains("section: links"));
  for (const auto &name : {QStringLiteral("server"), QStringLiteral("laptop"), QStringLiteral("tv")}) {
    const auto rect = find(config, name)->layoutRect();
    const auto line =
        QStringLiteral("\t%1 = %2,%3,%4,%5").arg(name).arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    QVERIFY2(text.contains(line), qPrintable(line));
  }
}

void ComputerLayoutTests::dialogShowsEveryComputer()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.resize(900, 600);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));

  const auto *view = dialog.findChild<ComputerLayoutView *>();
  QVERIFY(view != nullptr);
  QCOMPARE(view->scene()->items().size(), 3);

  // every computer is fitted into the view
  const auto visible = view->mapToScene(view->viewport()->rect()).boundingRect();
  for (const auto *item : view->scene()->items())
    QVERIFY(visible.contains(item->sceneBoundingRect()));

  if (const auto snapshot = qEnvironmentVariable("DESKFLOW_LAYOUT_SNAPSHOT"); !snapshot.isEmpty())
    dialog.grab().save(snapshot);
}

void ComputerLayoutTests::dragMovesComputer()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.resize(900, 600);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  auto *view = dialog.findChild<ComputerLayoutView *>();

  auto *laptop = itemAt(view, find(config, "laptop")->layoutRect());
  QVERIFY(laptop != nullptr);
  const auto before = laptop->pos();

  // down and away from everything, into empty space
  drag(view, laptop, QPoint(-40, 120));
  QVERIFY(laptop->pos().x() < before.x());
  QVERIFY(laptop->pos().y() > before.y());
}

void ComputerLayoutTests::dragOntoAnotherSpringsBack()
{
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.resize(900, 600);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  auto *view = dialog.findChild<ComputerLayoutView *>();

  auto *laptop = itemAt(view, find(config, "laptop")->layoutRect());
  const auto *server = itemAt(view, find(config, "server")->layoutRect());
  QVERIFY(laptop != nullptr && server != nullptr);
  const auto before = laptop->pos();

  // drop the laptop right on top of the server
  const auto by = view->mapFromScene(server->sceneBoundingRect().center()) -
                  view->mapFromScene(laptop->sceneBoundingRect().center());
  drag(view, laptop, by);
  QCOMPARE(laptop->pos(), before);
}

QTEST_MAIN(ComputerLayoutTests)
