/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateRelease.h"

#include "common/PlatformInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace deskflow::gui::updates {

namespace {

const auto kAsk = QStringLiteral("ask");
const auto kAlways = QStringLiteral("always");
const auto kNever = QStringLiteral("never");

std::optional<Release> releaseFrom(const QJsonObject &release)
{
  static const QRegularExpression setupName(QStringLiteral(R"(^deskflow-plus-(\d+\.\d+\.\d+\.\d+)-win-x64-setup\.exe$)")
  );
  static const QRegularExpression sha256(
      QStringLiteral("^sha256:([0-9a-f]{64})$"), QRegularExpression::CaseInsensitiveOption
  );

  if (release.value(QStringLiteral("draft")).toBool())
    return {};

  for (const auto &value : release.value(QStringLiteral("assets")).toArray()) {
    const auto asset = value.toObject();
    const auto name = asset.value(QStringLiteral("name")).toString();
    const auto nameMatch = setupName.match(name);
    const auto digestMatch = sha256.match(asset.value(QStringLiteral("digest")).toString());
    const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
    const auto size = asset.value(QStringLiteral("size")).toInteger();
    if (!nameMatch.hasMatch() || !digestMatch.hasMatch() || url.scheme() != QStringLiteral("https") || size <= 0)
      continue;

    return Release{
        QVersionNumber::fromString(nameMatch.captured(1)),
        QUrl(release.value(QStringLiteral("html_url")).toString()),
        name,
        url,
        size,
        digestMatch.captured(1).toLower().toLatin1()
    };
  }
  return {};
}

} // namespace

bool isSupported()
{
  return deskflow::platform::isWindows();
}

Mode modeFromString(const QString &mode)
{
  if (mode == kAlways)
    return Mode::Always;
  if (mode == kNever)
    return Mode::Never;
  return Mode::Ask;
}

QString modeToString(Mode mode)
{
  switch (mode) {
  case Mode::Always:
    return kAlways;
  case Mode::Never:
    return kNever;
  default:
    return kAsk;
  }
}

std::optional<Release> newerRelease(const QByteArray &json, const QVersionNumber &current)
{
  const auto document = QJsonDocument::fromJson(json);
  if (!document.isArray())
    return {};

  std::optional<Release> newest;
  for (const auto &value : document.array()) {
    if (auto release = releaseFrom(value.toObject());
        release && release->version > (newest ? newest->version : current))
      newest = std::move(release);
  }
  return newest;
}

} // namespace deskflow::gui::updates
