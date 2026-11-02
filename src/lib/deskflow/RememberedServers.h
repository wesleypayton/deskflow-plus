/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

// The address each server name last connected at, so a client can still reach the server when its
// name stops resolving, as at the login screen or when name lookups on the network fail. Mouse
// Without Borders keeps each machine's address the same way. Kept as "name=address" entries,
// most recent first.
namespace deskflow::rememberedServers {

//! How many server names are remembered
inline constexpr qsizetype kMaxEntries = 8;

//! \p entries with \p address remembered for \p name. Nothing changes when \p name is itself the address.
QStringList remember(const QStringList &entries, const QString &name, const QString &address);

//! The address remembered for \p name, or empty
QString lookup(const QStringList &entries, const QString &name);

} // namespace deskflow::rememberedServers
