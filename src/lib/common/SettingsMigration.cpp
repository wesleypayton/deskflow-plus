/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/SettingsMigration.h"

#include "common/Constants.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

#ifdef Q_OS_WIN
#include <Windows.h>

#include <array>
#endif

namespace deskflow::settingsMigration {

namespace {

// before 1.0, Deskflow Plus kept its settings where Deskflow does, named for Deskflow
const auto kDeskflowName = QStringLiteral("Deskflow");

bool runningAsSystem()
{
#ifdef Q_OS_WIN
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  alignas(TOKEN_USER) std::array<BYTE, sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE> buffer{};
  DWORD size = 0;
  const bool system = GetTokenInformation(token, TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &size) &&
                      IsWellKnownSid(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, WinLocalSystemSid);
  CloseHandle(token);
  return system;
#else
  return false;
#endif
}

// copies a file if it isn't there yet
bool copyIfMissing(const QString &from, const QString &to)
{
  if (QFileInfo::exists(to) || !QFileInfo::exists(from))
    return false;
  QDir().mkpath(QFileInfo(to).absolutePath());
  return QFile::copy(from, to);
}

} // namespace

QString deskflowPath(const QString &path)
{
  const QFileInfo info(path);
  auto name = info.fileName();
  name.replace(QString::fromUtf8(kAppName), kDeskflowName);
  auto folder = info.absolutePath();
  if (QFileInfo(folder).fileName() == QString::fromUtf8(kAppName))
    folder = QStringLiteral("%1/%2").arg(QFileInfo(folder).absolutePath(), kDeskflowName);
  return QStringLiteral("%1/%2").arg(folder, name);
}

QString pointAtOwnFolders(QString text, const QString &systemDir)
{
  const auto appName = QString::fromUtf8(kAppName);
  // the settings file of whichever user it is
  text.replace(
      QStringLiteral("/%1/%1.conf").arg(kDeskflowName), QStringLiteral("/%1/%1.conf").arg(appName), Qt::CaseInsensitive
  );
  text.replace(deskflowPath(systemDir) + QLatin1Char('/'), systemDir + QLatin1Char('/'), Qt::CaseInsensitive);
  return text;
}

bool copySettingsFile(const QString &from, const QString &to, const QString &systemDir)
{
  if (QFileInfo::exists(to))
    return false;
  QFile in(from);
  if (!in.open(QIODevice::ReadOnly))
    return false;
  const auto text = pointAtOwnFolders(QString::fromUtf8(in.readAll()), systemDir);

  QDir().mkpath(QFileInfo(to).absolutePath());
  QSaveFile out(to);
  if (!out.open(QIODevice::WriteOnly))
    return false;
  out.write(text.toUtf8());
  return out.commit();
}

void migrate(const Locations &locations)
{
  if (QString::fromUtf8(kAppName) == kDeskflowName)
    return;

  // a portable copy unpacked before 1.0 has its settings file under Deskflow's name, in its own folder
  if (!locations.portableFile.isEmpty()) {
    const auto old = QStringLiteral("%1/%2.conf").arg(QFileInfo(locations.portableFile).absolutePath(), kDeskflowName);
    // unpacking a newer copy over it adds a blank settings file under the new name
    if (QFileInfo::exists(old) &&
        (!QFileInfo::exists(locations.portableFile) || QFileInfo(locations.portableFile).size() <= 1)) {
      QFile::remove(locations.portableFile);
      QFile::rename(old, locations.portableFile);
    }
    if (QFileInfo::exists(locations.portableFile))
      return;
  }

  copySettingsFile(deskflowPath(locations.userFile), locations.userFile, locations.systemDir);
  copyIfMissing(deskflowPath(locations.stateFile), locations.stateFile);

  // the system's settings are only the service's to write, and the certificates and server settings only the
  // user's: a file copied by the service would be the service's, and the app couldn't change it
  if (runningAsSystem()) {
    if (!copySettingsFile(deskflowPath(locations.systemFile), locations.systemFile, locations.systemDir))
      return;

    // the user's settings, which the service starts the core with, may not have been copied yet
    const QSettings system(locations.systemFile, QSettings::IniFormat);
    if (const auto userFile = system.value(locations.daemonConfigKey).toString(); !userFile.isEmpty())
      copySettingsFile(deskflowPath(userFile), userFile, locations.systemDir);
    return;
  }

  const auto oldSystemDir = deskflowPath(locations.systemDir);
  QDirIterator files(oldSystemDir, QDir::Files, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    const auto from = files.next();
    const auto relative = QDir(oldSystemDir).relativeFilePath(from);
    // the service's own settings and logs stay behind
    if (relative.compare(QStringLiteral("%1.conf").arg(kDeskflowName), Qt::CaseInsensitive) == 0 ||
        relative.endsWith(QStringLiteral(".log"), Qt::CaseInsensitive))
      continue;
    copyIfMissing(from, QStringLiteral("%1/%2").arg(locations.systemDir, relative));
  }
}

} // namespace deskflow::settingsMigration
