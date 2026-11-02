/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/RememberedServers.h"

namespace deskflow::rememberedServers {

namespace {

QString nameOf(const QString &entry)
{
  return entry.section('=', 0, 0);
}

} // namespace

QStringList remember(const QStringList &entries, const QString &name, const QString &address)
{
  const auto trimmedName = name.trimmed();
  const auto trimmedAddress = address.trimmed();
  if (trimmedName.isEmpty() || trimmedAddress.isEmpty() || trimmedName.contains('=') ||
      trimmedName.compare(trimmedAddress, Qt::CaseInsensitive) == 0) {
    return entries;
  }

  QStringList result{QStringLiteral("%1=%2").arg(trimmedName, trimmedAddress)};
  for (const auto &entry : entries) {
    if (result.size() >= kMaxEntries)
      break;
    if (nameOf(entry).compare(trimmedName, Qt::CaseInsensitive) != 0)
      result.append(entry);
  }
  return result;
}

QString lookup(const QStringList &entries, const QString &name)
{
  const auto trimmedName = name.trimmed();
  for (const auto &entry : entries) {
    if (nameOf(entry).compare(trimmedName, Qt::CaseInsensitive) == 0)
      return entry.section('=', 1);
  }
  return {};
}

} // namespace deskflow::rememberedServers
