/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/ShellIntegration.h"

#include <QStringList>
#include <QUrl>

namespace deskflow::shell {

QString encodePaths(const std::vector<std::filesystem::path> &paths)
{
  QStringList encoded;
  for (const auto &path : paths)
    encoded.append(QString::fromLatin1(QUrl::toPercentEncoding(QString::fromStdU16String(path.u16string()))));
  return encoded.join('|');
}

std::vector<std::filesystem::path> decodePaths(const QString &text)
{
  std::vector<std::filesystem::path> paths;
  for (const auto &part : text.split('|', Qt::SkipEmptyParts))
    paths.emplace_back(QUrl::fromPercentEncoding(part.toLatin1()).toStdU16String());
  return paths;
}

} // namespace deskflow::shell
