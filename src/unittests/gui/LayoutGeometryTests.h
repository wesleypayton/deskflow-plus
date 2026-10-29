/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class LayoutGeometryTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void snap_touchesNearbyEdge();
  void snap_alignsEdgesAndCentres();
  void snap_ignoresFarEdges();
  void snap_picksClosest();
  void overlap_touchingIsFine();
  void sharedEdges_spansWhereRectsTouch();
  void placeBeside_prefersRightOfAnchor();
  void placeBeside_triesOtherSides();
  void grid_neighboursTouchServer();
  void grid_keepsGaps();
};
