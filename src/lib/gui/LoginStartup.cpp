/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LoginStartup.h"

#include "common/Constants.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>

namespace deskflow::gui::loginStartup {

#if defined(Q_OS_WIN)

namespace {

const auto kRunKey = QStringLiteral(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)");

QString quotedApp()
{
  return QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

QString command()
{
  return QStringLiteral("%1 --%2").arg(quotedApp(), kBackgroundOption);
}

} // namespace

bool isSupported()
{
  return true;
}

void apply(bool enabled)
{
  QSettings run(kRunKey, QSettings::NativeFormat);
  const auto name = QString::fromUtf8(kDisplayName);
  const auto current = run.value(name).toString();

  if (enabled) {
    if (current != command()) {
      qInfo("starting at sign-in: %s", qPrintable(command()));
      run.setValue(name, command());
    }
  } else if (current.startsWith(quotedApp(), Qt::CaseInsensitive)) {
    qInfo("no longer starting at sign-in");
    run.remove(name);
  }
}

#else

bool isSupported()
{
  return false;
}

void apply(bool)
{
  // not supported yet
}

#endif

} // namespace deskflow::gui::loginStartup
