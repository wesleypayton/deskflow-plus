/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerLayoutView.h"

#include "ComputerSetupModel.h"
#include "LayoutGeometry.h"
#include "dialogs/ComputerSettingsDialog.h"

#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include <algorithm>

namespace {

// how close, in screen pixels, an edge has to be dragged to another before it snaps
constexpr qreal kSnapDistance = 12;

// empty space kept around the computers when fitting them in the view, as a fraction of their extent
constexpr qreal kFitMargin = 0.08;

// colour of the edges the cursor can cross
const QColor kEdgeColor(46, 204, 113);

// big enough that dragging never runs into the edge of the scene
const QRectF kSceneBounds(-1e6, -1e6, 2e6, 2e6);

} // namespace

//! One computer on the canvas, drawn as its monitors
class ComputerLayoutItem : public QGraphicsItem
{
public:
  ComputerLayoutItem(ComputerLayoutView *view, qsizetype index) : m_view(view), m_index(index)
  {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setCursor(Qt::OpenHandCursor);
    setPos(computer().layoutRect().topLeft());
    setToolTip(computer().name());
  }

  qsizetype index() const
  {
    return m_index;
  }

  QRectF boundingRect() const override
  {
    return QRectF(QPointF(0, 0), QSizeF(computer().layoutRect().size()));
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
  {
    const auto &palette = m_view->palette();
    const bool server = computer().isServer();

    auto fill = server ? palette.color(QPalette::Highlight) : palette.color(QPalette::Button);
    if (m_overlapping)
      fill = QColor(220, 60, 60);

    auto border = isSelected() ? palette.color(QPalette::Highlight).darker(140) : palette.color(QPalette::Mid);
    QPen pen(border, isSelected() ? 3 : 1.5);
    pen.setCosmetic(true);
    painter->setRenderHint(QPainter::Antialiasing);

    // with several monitors, outline the whole desktop faintly so gaps between monitors read as one computer
    const auto monitors = computer().monitors();
    if (monitors.size() > 1) {
      QPen outline(palette.color(QPalette::Mid), 1, Qt::DashLine);
      outline.setCosmetic(true);
      painter->setPen(outline);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(boundingRect());
    }

    const auto radius = std::min(boundingRect().width(), boundingRect().height()) * 0.03;
    painter->setPen(pen);
    painter->setBrush(fill);
    for (const auto &monitor : monitors)
      painter->drawRoundedRect(QRectF(monitor).adjusted(8, 8, -8, -8), radius, radius);

    // draw the name in screen pixels so it stays readable however far the view is zoomed out
    const auto deviceRect = painter->transform().mapRect(boundingRect());
    painter->save();
    painter->resetTransform();
    auto font = m_view->font();
    font.setBold(server);
    painter->setFont(font);
    painter->setPen(
        server && !m_overlapping ? palette.color(QPalette::HighlightedText) : palette.color(QPalette::ButtonText)
    );
    const auto text =
        painter->fontMetrics().elidedText(computer().name(), Qt::ElideRight, static_cast<int>(deviceRect.width()) - 8);
    painter->drawText(deviceRect, Qt::AlignCenter, text);
    painter->restore();
  }

  //! Size or position changed in the model
  void refresh()
  {
    prepareGeometryChange();
    m_snapping = false;
    setPos(computer().layoutRect().topLeft());
    setToolTip(computer().name());
    update();
  }

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
  {
    if (change == ItemPositionChange && m_snapping) {
      const auto snapped = m_view->snap(m_index, value.toPointF());
      const bool overlapping = m_view->overlapsOthers(m_index, QRectF(snapped, boundingRect().size()));
      if (overlapping != m_overlapping) {
        m_overlapping = overlapping;
        update();
      }
      return snapped;
    }
    return QGraphicsItem::itemChange(change, value);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override
  {
    m_start = pos();
    m_snapping = true;
    setCursor(Qt::ClosedHandCursor);
    QGraphicsItem::mousePressEvent(event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
  {
    // leaving the canvas turns the move into a drag that the trash can take
    const auto *viewport = m_view->viewport();
    if (!viewport->rect().contains(viewport->mapFromGlobal(event->screenPos()))) {
      m_snapping = false;
      m_overlapping = false;
      setPos(m_start);
      setCursor(Qt::OpenHandCursor);
      ungrabMouse();
      m_view->dragToTrash(m_index);
      return;
    }
    QGraphicsItem::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
  {
    QGraphicsItem::mouseReleaseEvent(event);
    setCursor(Qt::OpenHandCursor);
    if (m_snapping) {
      m_snapping = false;
      m_overlapping = false;
      m_view->finishMove(this, m_start);
    }
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override
  {
    m_view->editComputer(m_index);
  }

private:
  Computer &computer() const
  {
    return m_view->computerAt(m_index);
  }

  ComputerLayoutView *m_view;
  qsizetype m_index;
  QPointF m_start;
  bool m_snapping = false;
  bool m_overlapping = false;
};

ComputerLayoutView::ComputerLayoutView(QWidget *parent) : QGraphicsView(parent)
{
  setScene(&m_scene);
  setSceneRect(kSceneBounds);
  setRenderHint(QPainter::Antialiasing);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setAcceptDrops(true);
  setFocusPolicy(Qt::StrongFocus);
}

void ComputerLayoutView::setModel(ComputerSetupModel *model)
{
  m_model = model;
  reload();
}

void ComputerLayoutView::reload()
{
  m_scene.clear();
  if (m_model == nullptr)
    return;

  auto &computers = m_model->m_computers;
  for (qsizetype i = 0; i < computers.size(); ++i) {
    auto &computer = computers[i];
    if (computer.isNull())
      continue;
    computers.placeInLayout(computer);
    m_scene.addItem(new ComputerLayoutItem(this, i));
  }
  fitToComputers();
}

void ComputerLayoutView::resizeEvent(QResizeEvent *event)
{
  QGraphicsView::resizeEvent(event);
  fitToComputers();
}

void ComputerLayoutView::dragEnterEvent(QDragEnterEvent *event)
{
  // only the new computer tile, a computer dragged out of the canvas comes back as a plain move
  if (event->source() != this && event->mimeData()->hasFormat(ComputerSetupModel::mimeType()) && !m_model->isFull()) {
    event->setDropAction(Qt::CopyAction);
    event->accept();
  } else {
    event->ignore();
  }
}

void ComputerLayoutView::dragMoveEvent(QDragMoveEvent *event)
{
  event->setDropAction(Qt::CopyAction);
  event->accept();
}

void ComputerLayoutView::dropEvent(QDropEvent *event)
{
  if (event->source() == this || !event->mimeData()->hasFormat(ComputerSetupModel::mimeType())) {
    event->ignore();
    return;
  }

  auto encoded = event->mimeData()->data(ComputerSetupModel::mimeType());
  QDataStream stream(&encoded, QIODevice::ReadOnly);
  int column = -1;
  int row = -1;
  Computer computer;
  stream >> column >> row >> computer;
  event->acceptProposedAction();

  // centre it under the cursor, snapped like a dragged computer, or next to the server if there's no room
  const QSizeF size(deskflow::gui::layout::kDefaultComputerSize);
  const auto centre = mapToScene(event->position().toPoint());
  const auto position = snap(-1, centre - QPointF(size.width() / 2, size.height() / 2));
  computer.setLayoutRect(QRect(position.toPoint(), size.toSize()));

  // ask for the name once the drag has finished, a modal dialog inside a drop upsets some platforms' drag loops
  QMetaObject::invokeMethod(
      this,
      [this, computer]() mutable {
        if (ComputerSettingsDialog dialog(this, &computer, &m_model->m_computers); dialog.exec() != QDialog::Accepted)
          return;

        m_model->m_computers.placeInLayout(computer);
        m_model->m_computers.addComputerToFirstEmpty(computer);
        Q_EMIT m_model->computersChanged();
        reload();
      },
      Qt::QueuedConnection
  );
}

void ComputerLayoutView::keyPressEvent(QKeyEvent *event)
{
  if (const auto *item = selectedItem();
      item && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
    removeComputer(item->index());
    return;
  }
  QGraphicsView::keyPressEvent(event);
}

void ComputerLayoutView::contextMenuEvent(QContextMenuEvent *event)
{
  auto *item = dynamic_cast<ComputerLayoutItem *>(itemAt(event->pos()));
  if (item == nullptr) {
    QGraphicsView::contextMenuEvent(event);
    return;
  }

  const auto index = item->index();
  QMenu menu(this);
  menu.addAction(tr("Edit…"), this, [this, index] { editComputer(index); });
  auto *remove = menu.addAction(tr("Remove"), this, [this, index] { removeComputer(index); });
  remove->setEnabled(!computerAt(index).isServer());
  menu.exec(event->globalPos());
}

void ComputerLayoutView::drawForeground(QPainter *painter, const QRectF &)
{
  // mark where the cursor crosses, from where the items are now so a drag shows it live
  QList<QRectF> rects;
  for (const auto *item : m_scene.items())
    rects.append(item->sceneBoundingRect());

  const auto edges = deskflow::gui::layout::sharedEdges(rects);

  // a dark casing under a bright line keeps it visible on any computer's colour
  QPen casing(QColor(0, 0, 0, 120), 7, Qt::SolidLine, Qt::RoundCap);
  casing.setCosmetic(true);
  painter->setPen(casing);
  for (const auto &edge : edges)
    painter->drawLine(edge);

  QPen line(kEdgeColor, 4, Qt::SolidLine, Qt::RoundCap);
  line.setCosmetic(true);
  painter->setPen(line);
  for (const auto &edge : edges)
    painter->drawLine(edge);
}

void ComputerLayoutView::drawBackground(QPainter *painter, const QRectF &rect)
{
  painter->fillRect(rect, palette().color(QPalette::Base));
  if (!m_scene.items().isEmpty())
    return;

  painter->save();
  painter->resetTransform();
  painter->setPen(palette().color(QPalette::PlaceholderText));
  painter->drawText(viewport()->rect(), Qt::AlignCenter, tr("Drag a new computer here to add it."));
  painter->restore();
}

Computer &ComputerLayoutView::computerAt(qsizetype index) const
{
  return m_model->m_computers[index];
}

QList<QRectF> ComputerLayoutView::otherRects(qsizetype index) const
{
  QList<QRectF> rects;
  const auto &computers = m_model->m_computers;
  for (qsizetype i = 0; i < computers.size(); ++i) {
    if (i != index && !computers.at(i).isNull() && computers.at(i).hasLayout())
      rects.append(QRectF(computers.at(i).layoutRect()));
  }
  return rects;
}

QPointF ComputerLayoutView::snap(qsizetype index, const QPointF &proposed) const
{
  const auto size =
      index < 0 ? QSizeF(deskflow::gui::layout::kDefaultComputerSize) : QSizeF(computerAt(index).layoutRect().size());
  const auto scale = transform().m11() > 0 ? transform().m11() : 1;
  const auto snapped =
      deskflow::gui::layout::snapPosition(QRectF(proposed, size), otherRects(index), kSnapDistance / scale);

  // layout positions are whole pixels
  return QPointF(qRound(snapped.x()), qRound(snapped.y()));
}

bool ComputerLayoutView::overlapsOthers(qsizetype index, const QRectF &rect) const
{
  return deskflow::gui::layout::overlapsAny(rect, otherRects(index));
}

void ComputerLayoutView::finishMove(ComputerLayoutItem *item, const QPointF &start)
{
  const auto index = item->index();
  const QRectF moved(item->pos(), item->boundingRect().size());
  if (overlapsOthers(index, moved)) {
    // computers can't share space, put it back where it came from
    item->setPos(start);
    item->update();
    return;
  }

  if (item->pos() == start)
    return;

  computerAt(index).moveLayoutTo(item->pos().toPoint());
  Q_EMIT m_model->computersChanged();
  fitToComputers();
}

void ComputerLayoutView::editComputer(qsizetype index)
{
  ComputerSettingsDialog dialog(this, &computerAt(index), &m_model->m_computers);
  dialog.exec();
  Q_EMIT m_model->computersChanged();

  // the item that asked may still be handling the click, so rebuild once it's done
  QMetaObject::invokeMethod(this, &ComputerLayoutView::reload, Qt::QueuedConnection);
}

void ComputerLayoutView::removeComputer(qsizetype index)
{
  // the server is this computer, it can't be left out
  if (computerAt(index).isServer())
    return;

  computerAt(index) = Computer();
  Q_EMIT m_model->computersChanged();
  QMetaObject::invokeMethod(this, &ComputerLayoutView::reload, Qt::QueuedConnection);
}

void ComputerLayoutView::dragToTrash(qsizetype index)
{
  if (computerAt(index).isServer())
    return;

  QByteArray encoded;
  QDataStream stream(&encoded, QIODevice::WriteOnly);
  stream << -1 << -1 << computerAt(index);

  auto *mimeData = new QMimeData;
  mimeData->setData(ComputerSetupModel::mimeType(), encoded);

  auto *drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->setPixmap(QIcon::fromTheme("video-display").pixmap(QSize(64, 64)));
  drag->setHotSpot(QPoint(32, 32));
  if (drag->exec(Qt::MoveAction) == Qt::MoveAction)
    removeComputer(index);
}

void ComputerLayoutView::fitToComputers()
{
  const auto bounds = m_scene.itemsBoundingRect();
  if (bounds.isEmpty()) {
    resetTransform();
    return;
  }

  const auto margin = std::max(bounds.width(), bounds.height()) * kFitMargin;
  fitInView(bounds.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
}

ComputerLayoutItem *ComputerLayoutView::selectedItem() const
{
  const auto selected = m_scene.selectedItems();
  return selected.isEmpty() ? nullptr : dynamic_cast<ComputerLayoutItem *>(selected.first());
}
