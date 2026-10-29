/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutGeometryTests.h"

#include "gui/LayoutGeometry.h"

using namespace deskflow::gui::layout;

void LayoutGeometryTests::snap_touchesNearbyEdge()
{
  // dragged so its left edge is 10 pixels past the other's right edge
  const QRectF other(0, 0, 1920, 1080);
  const QRectF moving(1930, 300, 1440, 900);
  QCOMPARE(snapPosition(moving, {other}, 20), QPointF(1920, 300));
}

void LayoutGeometryTests::snap_alignsEdgesAndCentres()
{
  const QRectF other(0, 0, 1920, 1080);

  // top edges within reach line up
  QCOMPARE(snapPosition(QRectF(1920, 15, 1440, 900), {other}, 20).y(), 0.0);

  // centres within reach line up: 1080 / 2 - 900 / 2 = 90
  QCOMPARE(snapPosition(QRectF(1920, 100, 1440, 900), {other}, 20).y(), 90.0);
}

void LayoutGeometryTests::snap_ignoresFarEdges()
{
  const QRectF other(0, 0, 1920, 1080);
  const QRectF moving(2500, 400, 1440, 900);
  QCOMPARE(snapPosition(moving, {other}, 20), moving.topLeft());
}

void LayoutGeometryTests::snap_picksClosest()
{
  // left edge is 15 from one computer and 5 from another
  const QRectF near(0, 0, 1005, 500);
  const QRectF far(0, 600, 1015, 500);
  const QRectF moving(1000, 2000, 300, 300);
  QCOMPARE(snapPosition(moving, {far, near}, 20).x(), 1005.0);
}

void LayoutGeometryTests::overlap_touchingIsFine()
{
  const QRectF other(0, 0, 1920, 1080);
  QVERIFY(!overlapsAny(QRectF(1920, 0, 100, 100), {other}));
  QVERIFY(!overlapsAny(QRectF(0, 1080, 100, 100), {other}));
  QVERIFY(overlapsAny(QRectF(1919, 0, 100, 100), {other}));
  QVERIFY(!overlapsAny(QRectF(0, 0, 100, 100), {}));
}

void LayoutGeometryTests::sharedEdges_spansWhereRectsTouch()
{
  const QRectF server(0, 0, 1920, 1080);
  const QRectF right(1920, 540, 1440, 900);     // touches the lower half of the server's right edge
  const QRectF above(-500, -1440, 2560, 1440);  // overhangs the server's top edge on the left
  const QRectF corner(-1920, 1080, 1920, 1080); // only meets the server's bottom left corner
  const auto edges = sharedEdges({server, right, above, corner});

  QCOMPARE(edges.size(), 2);
  QVERIFY(edges.contains(QLineF(1920, 540, 1920, 1080)));
  QVERIFY(edges.contains(QLineF(0, 0, 1920, 0)));
}

void LayoutGeometryTests::placeBeside_prefersRightOfAnchor()
{
  const QRect server(0, 0, 2560, 1440);
  QCOMPARE(placeBeside(QSize(1920, 1080), server, {server}), QPoint(2560, 0));
}

void LayoutGeometryTests::placeBeside_triesOtherSides()
{
  const QRect server(0, 0, 2560, 1440);
  const QRect right(2560, 0, 1920, 1080);
  QCOMPARE(placeBeside(QSize(1920, 1080), server, {server, right}), QPoint(-1920, 0));

  const QRect left(-1920, 0, 1920, 1080);
  QCOMPARE(placeBeside(QSize(1920, 1080), server, {server, right, left}), QPoint(0, 1440));

  // every side taken, so go past everything on the right
  const QRect below(0, 1440, 1920, 1080);
  const QRect above(0, -1080, 1920, 1080);
  QCOMPARE(placeBeside(QSize(1920, 1080), server, {server, right, left, below, above}), QPoint(4480, 0));
}

void LayoutGeometryTests::grid_neighboursTouchServer()
{
  // 3x3 grid, server in the middle and bigger than the default size
  const QSize client(1920, 1080);
  const QSize server(3840, 2160);
  const QSize empty;
  const QList<QSize> sizes = {empty,  client, empty,  //
                              client, server, client, //
                              empty,  client, empty};
  const auto rects = gridToLayout(sizes, 3, 4);

  const auto &centre = rects.at(4);
  QCOMPARE(rects.at(3).right() + 1, centre.left()); // left neighbour
  QCOMPARE(rects.at(5).left(), centre.right() + 1); // right neighbour
  QCOMPARE(rects.at(1).bottom() + 1, centre.top()); // above
  QCOMPARE(rects.at(7).top(), centre.bottom() + 1); // below
  QVERIFY(rects.at(0).isNull());
}

void LayoutGeometryTests::grid_keepsGaps()
{
  // computers with an empty cell between them don't touch
  const QSize client(1920, 1080);
  const auto rects = gridToLayout({client, QSize(), client}, 3, 0);
  QCOMPARE(rects.at(2).left() - (rects.at(0).right() + 1), 1920);
}

QTEST_MAIN(LayoutGeometryTests)
