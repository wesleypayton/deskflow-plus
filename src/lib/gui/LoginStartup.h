/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// Starting the app when the user signs in. On Windows this is an entry under the user's
// Run key; other platforms aren't supported yet.
namespace deskflow::gui::loginStartup {

//! The option the sign-in entry passes, so the app starts in the background
inline const auto kBackgroundOption = QStringLiteral("background");

bool isSupported();

//! Adds or removes this copy of the app's sign-in entry. An entry for another copy of the
//! app, such as a portable one, is only replaced when enabling.
void apply(bool enabled);

} // namespace deskflow::gui::loginStartup
