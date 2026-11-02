/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdaterTests.h"

#include "common/UpdateDownload.h"
#include "common/UpdateRelease.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace deskflow::updates;

namespace {

const auto kDigest = QStringLiteral("sha256:8a4222344c741d1be811e760358c6f6051a94acb8c45d9aba9fa9fabaf402603");

QJsonObject asset(const QString &name, const QString &digest = kDigest, qint64 size = 41978430)
{
  return {
      {QStringLiteral("name"), name},
      {QStringLiteral("size"), size},
      {QStringLiteral("digest"), digest},
      {QStringLiteral("browser_download_url"),
       QStringLiteral("https://github.com/wesleypayton/deskflow-plus/releases/download/v/%1").arg(name)}
  };
}

QJsonObject release(const QString &tag, const QJsonArray &assets, bool draft = false, bool prerelease = true)
{
  return {
      {QStringLiteral("tag_name"), tag},
      {QStringLiteral("draft"), draft},
      {QStringLiteral("prerelease"), prerelease},
      {QStringLiteral("html_url"),
       QStringLiteral("https://github.com/wesleypayton/deskflow-plus/releases/tag/%1").arg(tag)},
      {QStringLiteral("assets"), assets}
  };
}

// a release with four numbers is a test build, as they're published
QJsonObject releaseOf(const QString &version, bool draft = false)
{
  return release(
      QStringLiteral("v-%1").arg(version),
      {asset(QStringLiteral("deskflow-plus-%1-win-x64-portable.exe").arg(version)),
       asset(QStringLiteral("deskflow-plus-%1-win-x64-setup.exe").arg(version))},
      draft, version.count(QLatin1Char('.')) == 3
  );
}

QByteArray json(const QJsonArray &releases)
{
  return QJsonDocument(releases).toJson();
}

} // namespace

void UpdaterTests::newerRelease_picksNewest()
{
  // GitHub lists newest first, but the order isn't relied on
  const auto found = newerRelease(
      json(
          {releaseOf(QStringLiteral("1.27.0.51")), releaseOf(QStringLiteral("1.27.0.60")),
           releaseOf(QStringLiteral("1.27.0.48"))}
      ),
      QVersionNumber({1, 27, 0, 50}), true
  );
  QVERIFY(found.has_value());
  QCOMPARE(found->version, QVersionNumber({1, 27, 0, 60}));
  QCOMPARE(found->setupName, QStringLiteral("deskflow-plus-1.27.0.60-win-x64-setup.exe"));
  QCOMPARE(found->setupSize, 41978430);
  QCOMPARE(found->setupSha256, QByteArray("8a4222344c741d1be811e760358c6f6051a94acb8c45d9aba9fa9fabaf402603"));
  QCOMPARE(
      found->pageUrl.toString(),
      QStringLiteral("https://github.com/wesleypayton/deskflow-plus/releases/tag/v-1.27.0.60")
  );
}

void UpdaterTests::newerRelease_noneWhenCurrent()
{
  const auto releases = json({releaseOf(QStringLiteral("1.27.0.51")), releaseOf(QStringLiteral("1.27.0.48"))});
  QVERIFY(!newerRelease(releases, QVersionNumber({1, 27, 0, 51}), true).has_value());
  QVERIFY(!newerRelease(releases, QVersionNumber({1, 28, 0, 0}), true).has_value());
}

void UpdaterTests::newerRelease_skipsUnusable()
{
  const auto setup = QStringLiteral("deskflow-plus-1.27.0.70-win-x64-setup.exe");
  auto plainHttp = asset(setup);
  plainHttp[QStringLiteral("browser_download_url")] = QStringLiteral("http://example.com/%1").arg(setup);

  const auto found = newerRelease(
      json(
          {releaseOf(QStringLiteral("1.27.0.90"), true),
           release(QStringLiteral("no-digest"), {asset(setup, QString())}),
           release(QStringLiteral("other-digest"), {asset(setup, QStringLiteral("sha512:abcd"))}),
           release(QStringLiteral("empty"), {asset(setup, kDigest, 0)}), release(QStringLiteral("http"), {plainHttp}),
           release(
               QStringLiteral("portable-only"), {asset(QStringLiteral("deskflow-plus-1.27.0.80-win-x64-portable.exe"))}
           ),
           releaseOf(QStringLiteral("1.27.0.55"))}
      ),
      QVersionNumber({1, 27, 0, 50}), true
  );
  QVERIFY(found.has_value());
  QCOMPARE(found->version, QVersionNumber({1, 27, 0, 55}));
}

void UpdaterTests::newerRelease_badJson()
{
  QVERIFY(!newerRelease("", QVersionNumber(1), true).has_value());
  QVERIFY(!newerRelease("1.27.0", QVersionNumber(1), true).has_value());
  QVERIFY(!newerRelease(R"({"message":"API rate limit exceeded"})", QVersionNumber(1), true).has_value());
}

void UpdaterTests::newerRelease_testBuildsOnlyWhenAsked()
{
  const auto releases = json(
      {releaseOf(QStringLiteral("1.0.1.4")), releaseOf(QStringLiteral("1.0.1")), releaseOf(QStringLiteral("1.0.0"))}
  );

  const auto release = newerRelease(releases, QVersionNumber({1, 0, 0}), false);
  QVERIFY(release.has_value());
  QCOMPARE(release->version, QVersionNumber({1, 0, 1}));
  QCOMPARE(release->setupName, QStringLiteral("deskflow-plus-1.0.1-win-x64-setup.exe"));
  QVERIFY(!release->testBuild);

  const auto testBuild = newerRelease(releases, QVersionNumber({1, 0, 0}), true);
  QVERIFY(testBuild.has_value());
  QCOMPARE(testBuild->version, QVersionNumber({1, 0, 1, 4}));
  QVERIFY(testBuild->testBuild);

  // a test build is newer than the release it follows
  QVERIFY(!newerRelease(releases, QVersionNumber({1, 0, 1, 4}), false).has_value());
}

void UpdaterTests::findRelease_exactVersion()
{
  const auto releases = json({releaseOf(QStringLiteral("1.0.1.4")), releaseOf(QStringLiteral("1.0.1"))});
  QCOMPARE(findRelease(releases, QVersionNumber({1, 0, 1, 4}))->version, QVersionNumber({1, 0, 1, 4}));
  QCOMPARE(findRelease(releases, QVersionNumber({1, 0, 1}))->version, QVersionNumber({1, 0, 1}));
  QVERIFY(!findRelease(releases, QVersionNumber({1, 0, 2})).has_value());
}

void UpdaterTests::isTestBuild_fourthNumber()
{
  QVERIFY(!isTestBuild(QVersionNumber({1, 0, 0})));
  QVERIFY(!isTestBuild(QVersionNumber({1, 0, 0, 0})));
  QVERIFY(isTestBuild(QVersionNumber({1, 0, 0, 3})));
}

void UpdaterTests::mode_roundTrips()
{
  for (const auto mode : {Mode::Ask, Mode::Always, Mode::Never})
    QCOMPARE(modeFromString(modeToString(mode)), mode);
  QCOMPARE(modeFromString(QString()), Mode::Ask);
  QCOMPARE(modeFromString(QStringLiteral("sometimes")), Mode::Ask);
}

namespace {

struct DownloadResult
{
  QString error;
  bool kept = false;
};

DownloadResult download(const QByteArray &content, qint64 size, const QByteArray &sha256)
{
  QTemporaryDir dir;
  const auto source = dir.filePath(QStringLiteral("source.exe"));
  QFile file(source);
  if (!file.open(QIODevice::WriteOnly))
    return {file.errorString()};
  file.write(content);
  file.close();

  QNetworkAccessManager network;
  UpdateDownload download(&network);
  QSignalSpy finished(&download, &UpdateDownload::finished);
  const auto target = dir.filePath(QStringLiteral("setup.exe"));
  download.start(QUrl::fromLocalFile(source), target, size, sha256);
  if (!finished.wait(5000))
    return {QStringLiteral("timed out")};
  return {finished.at(0).at(0).toString(), QFile::exists(target)};
}

} // namespace

void UpdaterTests::download_keepsMatchingFile()
{
  const QByteArray content(300000, 'x');
  const auto result =
      download(content, content.size(), QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
  QCOMPARE(result.error, QString());
  QVERIFY(result.kept);
}

void UpdaterTests::download_dropsChangedFile()
{
  const QByteArray content(300000, 'x');
  const auto other = QCryptographicHash::hash("something else", QCryptographicHash::Sha256).toHex();
  const auto result = download(content, content.size(), other);
  QVERIFY(!result.error.isEmpty());
  QVERIFY(!result.kept);
}

void UpdaterTests::download_dropsWrongSize()
{
  const QByteArray content(300000, 'x');
  const auto sha256 = QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex();
  for (const auto size : {content.size() - 1, content.size() + 1}) {
    const auto result = download(content, size, sha256);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(!result.kept);
  }
}

QTEST_MAIN(UpdaterTests)
