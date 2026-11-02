/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutGeometry.h"

#include <QtMath>

#include <algorithm>
#include <array>
#include <optional>

namespace deskflow::gui::layout {

namespace {

// smallest offset within threshold that moves one of the moving lines onto one of the target lines
std::optional<qreal> bestOffset(const std::array<qreal, 3> &moving, const QList<qreal> &targets, qreal threshold)
{
  std::optional<qreal> best;
  for (const auto line : moving) {
    for (const auto target : targets) {
      const auto offset = target - line;
      if (qAbs(offset) <= threshold && (!best || qAbs(offset) < qAbs(*best)))
        best = offset;
    }
  }
  return best;
}

} // namespace

QPointF snapPosition(const QRectF &moving, const QList<QRectF> &others, qreal threshold)
{
  QList<qreal> xTargets;
  QList<qreal> yTargets;
  for (const auto &other : others) {
    xTargets << other.left() << other.right() << other.center().x();
    yTargets << other.top() << other.bottom() << other.center().y();
  }

  auto position = moving.topLeft();
  if (const auto dx = bestOffset({moving.left(), moving.right(), moving.center().x()}, xTargets, threshold))
    position.rx() += *dx;
  if (const auto dy = bestOffset({moving.top(), moving.bottom(), moving.center().y()}, yTargets, threshold))
    position.ry() += *dy;
  return position;
}

namespace {

void addSharedEdges(const QRectF &a, const QRectF &b, QList<QLineF> &edges)
{
  // a vertical edge: one's right is the other's left. positions are whole pixels so exact compares are safe
  if (a.right() == b.left() || b.right() == a.left()) {
    const auto x = a.right() == b.left() ? a.right() : a.left();
    const auto top = std::max(a.top(), b.top());
    const auto bottom = std::min(a.bottom(), b.bottom());
    if (bottom > top)
      edges.append(QLineF(x, top, x, bottom));
  }

  // a horizontal edge: one's bottom is the other's top
  if (a.bottom() == b.top() || b.bottom() == a.top()) {
    const auto y = a.bottom() == b.top() ? a.bottom() : a.top();
    const auto left = std::max(a.left(), b.left());
    const auto right = std::min(a.right(), b.right());
    if (right > left)
      edges.append(QLineF(left, y, right, y));
  }
}

} // namespace

QList<QLineF> sharedEdges(const QList<QRectF> &rects)
{
  QList<QLineF> edges;
  for (qsizetype i = 0; i < rects.size(); ++i) {
    for (qsizetype j = i + 1; j < rects.size(); ++j)
      addSharedEdges(rects.at(i), rects.at(j), edges);
  }
  return edges;
}

QList<QLineF> sharedEdges(const QList<QList<QRectF>> &computers)
{
  QList<QLineF> edges;
  for (qsizetype i = 0; i < computers.size(); ++i) {
    for (qsizetype j = i + 1; j < computers.size(); ++j) {
      for (const auto &a : computers.at(i)) {
        for (const auto &b : computers.at(j))
          addSharedEdges(a, b, edges);
      }
    }
  }
  return edges;
}

bool overlapsAny(const QRectF &rect, const QList<QRectF> &others)
{
  return std::ranges::any_of(others, [&rect](const QRectF &other) { return rect.intersects(other); });
}

QPoint placeBeside(const QSize &size, const QRect &anchor, const QList<QRect> &others)
{
  QList<QRectF> taken;
  for (const auto &other : others)
    taken.append(QRectF(other));

  const std::array candidates = {
      QPoint(anchor.x() + anchor.width(), anchor.y()), QPoint(anchor.x() - size.width(), anchor.y()),
      QPoint(anchor.x(), anchor.y() + anchor.height()), QPoint(anchor.x(), anchor.y() - size.height())
  };
  for (const auto &candidate : candidates) {
    if (!overlapsAny(QRectF(QRect(candidate, size)), taken))
      return candidate;
  }

  int right = anchor.x() + anchor.width();
  for (const auto &other : others)
    right = std::max(right, other.x() + other.width());
  return {right, anchor.y()};
}

QList<QRect> gridToLayout(const QList<QSize> &sizes, int columns, qsizetype anchor)
{
  if (columns <= 0)
    return QList<QRect>(sizes.size());

  const auto rows = static_cast<int>((sizes.size() + columns - 1) / columns);
  QList<int> widths(columns, 0);
  QList<int> heights(rows, 0);
  for (qsizetype i = 0; i < sizes.size(); ++i) {
    const auto column = static_cast<int>(i % columns);
    const auto row = static_cast<int>(i / columns);
    widths[column] = std::max(widths[column], sizes.at(i).width());
    heights[row] = std::max(heights[row], sizes.at(i).height());
  }

  // keep empty columns and rows so computers on either side of a gap stay apart
  for (auto &width : widths)
    width = width > 0 ? width : kDefaultComputerSize.width();
  for (auto &height : heights)
    height = height > 0 ? height : kDefaultComputerSize.height();

  const bool hasAnchor = anchor >= 0 && anchor < sizes.size();
  const auto anchorColumn = hasAnchor ? static_cast<int>(anchor % columns) : 0;
  const auto anchorRow = hasAnchor ? static_cast<int>(anchor / columns) : 0;

  QList<QRect> rects(sizes.size());
  for (qsizetype i = 0; i < sizes.size(); ++i) {
    const auto &size = sizes.at(i);
    if (size.isEmpty())
      continue;

    const auto column = static_cast<int>(i % columns);
    const auto row = static_cast<int>(i / columns);
    int x = 0;
    for (int c = 0; c < column; ++c)
      x += widths.at(c);
    int y = 0;
    for (int r = 0; r < row; ++r)
      y += heights.at(r);

    // left of or above the anchor, hug the far edge of the cell so the anchor side touches
    if (column < anchorColumn)
      x += widths.at(column) - size.width();
    if (row < anchorRow)
      y += heights.at(row) - size.height();

    rects[i] = QRect(QPoint(x, y), size);
  }
  return rects;
}

} // namespace deskflow::gui::layout
