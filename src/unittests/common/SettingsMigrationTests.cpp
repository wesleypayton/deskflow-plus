/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsMigrationTests.h"

#include "common/SettingsMigration.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace deskflow::settingsMigration;

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content);
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

void SettingsMigrationTests::deskflowPath_namesAndFolders()
{
  QCOMPARE(
      deskflowPath(QStringLiteral("C:/Users/me/AppData/Roaming/Deskflow Plus/Deskflow Plus.conf")),
      QStringLiteral("C:/Users/me/AppData/Roaming/Deskflow/Deskflow.conf")
  );
  QCOMPARE(deskflowPath(QStringLiteral("C:/ProgramData/Deskflow Plus")), QStringLiteral("C:/ProgramData/Deskflow"));
  QCOMPARE(
      deskflowPath(QStringLiteral("C:/ProgramData/Deskflow Plus/tls")), QStringLiteral("C:/ProgramData/Deskflow/tls")
  );
  QCOMPARE(
      deskflowPath(QStringLiteral("C:/Users/me/AppData/Local/Deskflow Plus.state")),
      QStringLiteral("C:/Users/me/AppData/Local/Deskflow.state")
  );
}

void SettingsMigrationTests::pointAtOwnFolders_rewritesPaths()
{
  const auto text = QStringLiteral("[daemon]\nconfigFile=C:/Users/me/AppData/Roaming/Deskflow/Deskflow.conf\n"
                                   "[security]\ncertificate=C:/ProgramData/Deskflow/tls/deskflow.pem\n"
                                   "[log]\nfile=C:/Users/me/deskflow.log\n");
  QCOMPARE(
      pointAtOwnFolders(text, QStringLiteral("C:/ProgramData/Deskflow Plus")),
      QStringLiteral("[daemon]\nconfigFile=C:/Users/me/AppData/Roaming/Deskflow Plus/Deskflow Plus.conf\n"
                     "[security]\ncertificate=C:/ProgramData/Deskflow Plus/tls/deskflow.pem\n"
                     "[log]\nfile=C:/Users/me/deskflow.log\n")
  );
}

void SettingsMigrationTests::copySettingsFile_onlyOnce()
{
  QTemporaryDir dir;
  const auto from = dir.filePath(QStringLiteral("Deskflow/Deskflow.conf"));
  const auto to = dir.filePath(QStringLiteral("Deskflow Plus/Deskflow Plus.conf"));
  writeFile(from, "[core]\nport=24800\n");

  QVERIFY(copySettingsFile(from, to, dir.filePath(QStringLiteral("system"))));
  QCOMPARE(readFile(to), QByteArray("[core]\nport=24800\n"));

  // changed since, by Deskflow Plus: left alone
  writeFile(to, "[core]\nport=24900\n");
  QVERIFY(!copySettingsFile(from, to, dir.filePath(QStringLiteral("system"))));
  QCOMPARE(readFile(to), QByteArray("[core]\nport=24900\n"));
  QCOMPARE(readFile(from), QByteArray("[core]\nport=24800\n"));
}

void SettingsMigrationTests::migrate_copiesCertificatesNotServiceSettings()
{
  QTemporaryDir dir;
  const auto base = dir.path();
  writeFile(base + "/Roaming/Deskflow/Deskflow.conf", "[core]\nport=24800\n");
  writeFile(base + "/ProgramData/Deskflow/Deskflow.conf", "[daemon]\nlogLevel=INFO\n");
  writeFile(base + "/ProgramData/Deskflow/tls/deskflow.pem", "certificate");
  writeFile(base + "/ProgramData/Deskflow/tls/trusted-servers", "fingerprint");
  writeFile(base + "/ProgramData/Deskflow/deskflow-daemon.log", "log");

  const Locations locations{
      base + "/Roaming/Deskflow Plus/Deskflow Plus.conf",
      base + "/ProgramData/Deskflow Plus/Deskflow Plus.conf",
      base + "/ProgramData/Deskflow Plus",
      base + "/Local/Deskflow Plus.state",
      {},
      QStringLiteral("daemon/configFile")
  };
  migrate(locations);

  QCOMPARE(readFile(locations.userFile), QByteArray("[core]\nport=24800\n"));
  QCOMPARE(readFile(base + "/ProgramData/Deskflow Plus/tls/deskflow.pem"), QByteArray("certificate"));
  QCOMPARE(readFile(base + "/ProgramData/Deskflow Plus/tls/trusted-servers"), QByteArray("fingerprint"));
  // the service's are for the service to copy, and logs aren't copied
  QVERIFY(!QFileInfo::exists(locations.systemFile));
  QVERIFY(!QFileInfo::exists(base + "/ProgramData/Deskflow Plus/deskflow-daemon.log"));
  // Deskflow's are left as they were
  QCOMPARE(readFile(base + "/ProgramData/Deskflow/tls/deskflow.pem"), QByteArray("certificate"));
}

void SettingsMigrationTests::migrate_portableKeepsOldSettings()
{
  QTemporaryDir dir;
  const auto base = dir.path();
  writeFile(base + "/settings/Deskflow.conf", "[core]\nport=24800\n");
  // unpacked over the old copy: a blank settings file under the new name
  writeFile(base + "/settings/Deskflow Plus.conf", " ");

  Locations locations;
  locations.portableFile = base + "/settings/Deskflow Plus.conf";
  locations.userFile = base + "/user/Deskflow Plus/Deskflow Plus.conf";
  migrate(locations);

  QCOMPARE(readFile(locations.portableFile), QByteArray("[core]\nport=24800\n"));
  QVERIFY(!QFileInfo::exists(base + "/settings/Deskflow.conf"));
  QVERIFY(!QFileInfo::exists(locations.userFile));
}

QTEST_MAIN(SettingsMigrationTests)
