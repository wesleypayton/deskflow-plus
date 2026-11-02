/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

#include <optional>

// Finding a newer Deskflow Plus on the GitHub releases page. Each release carries a setup .exe
// named for its version, as deploy/windows/make-release.ps1 names it, and GitHub lists the SHA-256
// of each file, which the download is checked against.
namespace deskflow::gui::updates {

//! What happens when there's a newer version, as the update setting stores it
enum class Mode
{
  Ask,
  Always,
  Never
};

//! After the user answers Yes or No, how long until they're asked again
inline constexpr int kAskAgainDays = 7;

struct Release
{
  QVersionNumber version;
  QUrl pageUrl;
  QString setupName;
  QUrl setupUrl;
  qint64 setupSize = 0;
  //! The setup's SHA-256, as lowercase hex
  QByteArray setupSha256;
};

//! Only Windows has a setup to update with
bool isSupported();

Mode modeFromString(const QString &mode);
QString modeToString(Mode mode);

//! The newest release in \p json, GitHub's list of releases, when it's newer than \p current.
//! Drafts, and releases without a setup whose SHA-256 is listed, are passed over.
std::optional<Release> newerRelease(const QByteArray &json, const QVersionNumber &current);

} // namespace deskflow::gui::updates
