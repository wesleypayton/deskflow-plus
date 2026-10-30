/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LocalMonitors.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace deskflow::gui {

QList<QRect> localMonitors()
{
  QList<QRect> monitors;

#ifdef Q_OS_WIN
  // the same call the core uses, so the rectangles match what the server reports
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR, HDC, LPRECT rect, LPARAM data) -> BOOL {
        reinterpret_cast<QList<QRect> *>(data)->append(
            QRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top)
        );
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&monitors)
  );
#endif

  if (monitors.isEmpty()) {
    // Qt reports device independent pixels, so scale back up to desktop pixels
    for (const auto *screen : QGuiApplication::screens()) {
      const auto ratio = screen->devicePixelRatio();
      const auto geometry = screen->geometry();
      monitors.append(QRect(
          qRound(geometry.x() * ratio), qRound(geometry.y() * ratio), qRound(geometry.width() * ratio),
          qRound(geometry.height() * ratio)
      ));
    }
  }

  return monitors;
}

} // namespace deskflow::gui
