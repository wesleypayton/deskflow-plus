/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QRect>

namespace deskflow::gui {

//! This computer's monitors in desktop pixels, positioned as the OS arranges them
QList<QRect> localMonitors();

} // namespace deskflow::gui
