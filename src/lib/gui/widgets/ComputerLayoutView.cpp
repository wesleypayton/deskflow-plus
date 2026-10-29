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
#include <QCoreApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <array>

namespace {

// how close, in screen pixels, an edge has to be dragged to another before it snaps
constexpr qreal kSnapDistance = 12;

// empty space kept around the computers when fitting them in the view, as a fraction of their extent
constexpr qreal kFitMargin = 0.08;

// colour of the edges the cursor can cross
const QColor kEdgeColor(46, 204, 113);

// a computer being dragged onto another
const QColor kOverlapColor(220, 38, 38);

// labels sit on the computer colours, which are all dark enough for white text
const QColor kTextColor(255, 255, 255);

// the server's colour, then one per other computer; none is green or red, which mark edges and overlaps.
// fixed rather than taken from the theme so each computer stays distinct in light and dark mode alike.
const std::array kComputerColors = {
    QColor(37, 99, 235),  // blue, the server
    QColor(234, 88, 12),  // orange
    QColor(124, 58, 237), // purple
    QColor(219, 39, 119), // pink
    QColor(8, 145, 178),  // cyan
    QColor(217, 119, 6),  // amber
    QColor(79, 70, 229),  // indigo
};

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
    painter->setRenderHint(QPainter::Antialiasing);

    // everything below is sized in screen pixels, so convert once
    const auto scale = painter->transform().m11() > 0 ? painter->transform().m11() : 1;
    const auto pixel = 1 / scale;

    const auto accent = m_overlapping ? kOverlapColor : m_view->colorFor(m_index);
    const auto monitors = computer().monitors();
    const auto group = boundingRect();

    // several monitors share one tinted, outlined backdrop so they read as one computer
    if (monitors.size() > 1) {
      auto tint = accent;
      tint.setAlpha(55);
      QPen outline(accent, 2);
      outline.setCosmetic(true);
      painter->setPen(outline);
      painter->setBrush(tint);
      painter->drawRoundedRect(group, 6 * pixel, 6 * pixel);
    }

    // a small gap between monitors keeps them apart however far the view is zoomed out
    const auto inset = (monitors.size() > 1 ? 4 : 0) * pixel;
    painter->setPen(Qt::NoPen);
    painter->setBrush(accent);
    for (const auto &monitor : monitors)
      painter->drawRoundedRect(QRectF(monitor).adjusted(inset, inset, -inset, -inset), 4 * pixel, 4 * pixel);

    if (isSelected()) {
      QPen selection(kTextColor, 3);
      selection.setCosmetic(true);
      painter->setPen(selection);
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(group.adjusted(-3 * pixel, -3 * pixel, 3 * pixel, 3 * pixel), 8 * pixel, 8 * pixel);
    }

    // labels are drawn in screen pixels so they stay readable at any zoom
    const auto toDevice = painter->transform();
    painter->save();
    painter->resetTransform();

    if (monitors.size() > 1) {
      auto numberFont = m_view->font();
      numberFont.setPointSizeF(numberFont.pointSizeF() * 0.85);
      painter->setFont(numberFont);
      painter->setPen(kTextColor);
      for (qsizetype i = 0; i < monitors.size(); ++i) {
        const auto corner = toDevice.mapRect(QRectF(monitors.at(i))).adjusted(7, 5, 0, 0);
        painter->drawText(corner, Qt::AlignLeft | Qt::AlignTop, QString::number(i + 1));
      }
    }

    // on the biggest monitor rather than the middle of the group, which may be a gap between monitors
    const auto largest = std::ranges::max_element(monitors, {}, [](const QRect &monitor) {
      return static_cast<qint64>(monitor.width()) * monitor.height();
    });
    paintName(painter, toDevice.mapRect(QRectF(*largest)));
    painter->restore();
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

  //! The computer's name centred on its monitors, on a dark pill so it reads over any colour
  void paintName(QPainter *painter, const QRectF &area) const
  {
    auto nameFont = m_view->font();
    nameFont.setBold(true);
    auto noteFont = m_view->font();
    noteFont.setPointSizeF(noteFont.pointSizeF() * 0.85);

    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics noteMetrics(noteFont);
    const auto maxWidth = static_cast<int>(area.width()) - 16;
    const auto name = nameMetrics.elidedText(computer().name(), Qt::ElideRight, maxWidth);
    const auto note = computer().isServer() ? noteMetrics.elidedText(
                                                  QCoreApplication::translate("ComputerLayoutView", "This computer"),
                                                  Qt::ElideRight, maxWidth
                                              )
                                            : QString();

    const auto width = std::max(nameMetrics.horizontalAdvance(name), noteMetrics.horizontalAdvance(note));
    const auto height = nameMetrics.height() + (note.isEmpty() ? 0 : noteMetrics.height());
    QRectF pill(0, 0, width + 16, height + 8);
    pill.moveCenter(area.center());

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 110));
    painter->drawRoundedRect(pill, 6, 6);

    painter->setPen(kTextColor);
    painter->setFont(nameFont);
    const QRectF nameRect(pill.left(), pill.top() + 4, pill.width(), nameMetrics.height());
    painter->drawText(nameRect, Qt::AlignCenter, name);
    if (!note.isEmpty()) {
      painter->setFont(noteFont);
      painter->drawText(nameRect.translated(0, nameMetrics.height()), Qt::AlignCenter, note);
    }
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

QSize ComputerLayoutView::sizeHint() const
{
  // QGraphicsView sizes itself from the scene, which is deliberately huge, so ask for something sensible
  return {640, 360};
}

QColor ComputerLayoutView::colorFor(qsizetype index) const
{
  const auto &computers = m_model->m_computers;
  if (computers.at(index).isServer())
    return kComputerColors.front();

  // other computers take the remaining colours in name order, so a computer keeps its colour between sessions
  QStringList names;
  for (const auto &computer : computers) {
    if (!computer.isNull() && !computer.isServer())
      names.append(computer.name());
  }
  names.sort(Qt::CaseInsensitive);
  const auto position = static_cast<size_t>(std::max<qsizetype>(names.indexOf(computers.at(index).name()), 0));
  return kComputerColors.at(1 + position % (kComputerColors.size() - 1));
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
