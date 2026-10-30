/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ServerConfigTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void equalityCheck();
  void equalityCheck_diff_options();
  void equalityCheck_diff_alias();
  void equalityCheck_diff_filters();
  //  void equalityCheck_diff_address();
  void equalityCheck_diff_neighbours1();
  void equalityCheck_diff_neighbours2();
  void equalityCheck_diff_neighbours3();
  void equalityCheck_diff_layout();
  void layout_invalidRect();
  void layout_sideBySide();
  void layout_partialSpan();
  void layout_twoNeighboursOnOneEdge();
  void layout_stacked();
  void layout_cornerOnly();
  void layout_overlap();
  void layout_replacesLinks();
};
