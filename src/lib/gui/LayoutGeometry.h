/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QLineF>
#include <QList>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>

// Geometry for the free-form computer layout. Rectangles are in layout space,
// where one unit is one pixel of the computer's desktop.
namespace deskflow::gui::layout {

//! Size given to computers whose real size isn't known yet
inline const QSize kDefaultComputerSize(1920, 1080);

//! Snap a dragged rectangle to the edges and centres of the others
/*!
Returns the top left for \c moving after nudging it, separately on each axis,
by the smallest distance within \c threshold that makes one of its edges or its
centre line up with an edge or centre of another rectangle, or that makes it
touch another rectangle.
*/
QPointF snapPosition(const QRectF &moving, const QList<QRectF> &others, qreal threshold);

//! Where the cursor can cross between rectangles
/*!
Returns one line for each span where an edge of one rectangle lies on the
opposite edge of another, the same rule the server uses to link computers.
Rectangles that only meet at a corner share nothing.
*/
QList<QLineF> sharedEdges(const QList<QRectF> &rects);

//! True if \c rect shares any area with one of \c others (touching edges is fine)
bool overlapsAny(const QRectF &rect, const QList<QRectF> &others);

//! Find a free spot for a rectangle of \c size touching \c anchor
/*!
Tries the right, left, bottom and top of \c anchor in that order, top or left
aligned, and falls back to the right of everything.
*/
QPoint placeBeside(const QSize &size, const QRect &anchor, const QList<QRect> &others);

//! Turn the old fixed grid into touching rectangles
/*!
\c sizes holds one entry per grid cell, row by row, with an empty size for an
empty cell. Each column is as wide as its widest computer and each row as tall
as its tallest. Computers are pushed toward the cell at \c anchor (the server)
within their column and row, so the server's grid neighbours touch it however
their sizes differ. Empty cells get a null rectangle.
*/
QList<QRect> gridToLayout(const QList<QSize> &sizes, int columns, qsizetype anchor);

} // namespace deskflow::gui::layout
