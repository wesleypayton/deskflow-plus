/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>

class Computer;
class ComputerLayoutItem;
class ComputerSetupModel;

//! Free-form computer layout: drag computers anywhere and they snap to each other
/*!
Each computer is drawn as its monitors, at their real proportions, and can be
moved anywhere it doesn't overlap another computer. Computers whose edges touch
are neighbours, and the cursor crosses wherever they touch. Double click a
computer to edit it, press Delete or drag it out onto the trash to remove it,
and drop the new computer tile onto the canvas to add one.
*/
class ComputerLayoutView : public QGraphicsView
{
  Q_OBJECT
  friend class ComputerLayoutItem;

public:
  explicit ComputerLayoutView(QWidget *parent = nullptr);

  void setModel(ComputerSetupModel *model);

  //! Rebuild the canvas from the model's computers
  void reload();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
  Computer &computerAt(qsizetype index) const;
  QList<QRectF> otherRects(qsizetype index) const;
  QPointF snap(qsizetype index, const QPointF &proposed) const;
  bool overlapsOthers(qsizetype index, const QRectF &rect) const;
  void finishMove(ComputerLayoutItem *item, const QPointF &start);
  void editComputer(qsizetype index);
  void removeComputer(qsizetype index);
  void dragToTrash(qsizetype index);
  void fitToComputers();
  ComputerLayoutItem *selectedItem() const;

  ComputerSetupModel *m_model = nullptr;
  QGraphicsScene m_scene;
};
