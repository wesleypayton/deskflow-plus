/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonUpdater.h"

#include "base/Log.h"
#include "common/Constants.h"
#include "common/UpdateDownload.h"
#include "common/UpdateRelease.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "platform/MSWindowsSession.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>

#include <Windows.h>
// after Windows.h
#include <shlobj.h>
#include <userenv.h>

#include <memory>
#include <string>

using namespace deskflow::updates;
using namespace std::chrono_literals;

namespace {

// a note for the service that setup starts once it has upgraded, see restartAppAfterUpdate()
const auto kStateKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Deskflow Plus");
const auto kRestartValue = QStringLiteral("RestartAppAfterUpdate");

// a note older than this was left by a setup that didn't finish
constexpr qint64 kRestartWithinSecs = 15 * 60;

// after a restart the user may not have signed in yet, so keep trying for a minute
constexpr int kRestartAttempts = 20;
constexpr auto kRestartRetry = 3s;

constexpr auto kListTimeout = 30s;

// in the system's own profile, which only the system and administrators can change, so the setup that was
// checked is the one that runs
QString updateFolder()
{
  PWSTR path = nullptr;
  QString folder;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    folder = QDir::fromNativeSeparators(QString::fromWCharArray(path)) + QStringLiteral("/Deskflow Plus/Update");
  CoTaskMemFree(path);
  return folder;
}

// true once the app is running for the signed in user, or there's no point trying again
bool startAppForSessionUser()
{
  MSWindowsSession session;
  session.updateActiveSession();
  if (session.isProcessInSession(L"deskflow.exe", nullptr))
    return true;

  HANDLE token = nullptr;
  try {
    token = session.getUserToken(nullptr);
  } catch (const std::runtime_error &) {
    LOG_DEBUG("no one is signed in to start the app for yet");
    return false;
  }

  LPVOID environment = nullptr;
  if (!CreateEnvironmentBlock(&environment, token, FALSE))
    environment = nullptr;

  const auto folder = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
  const auto app = folder + L"\\deskflow.exe";
  auto command = L"\"" + app + L"\"";
  std::wstring desktop = L"winsta0\\default";

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.lpDesktop = desktop.data();
  PROCESS_INFORMATION process{};
  const BOOL started = CreateProcessAsUserW(
      token, app.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, environment,
      folder.c_str(), &startup, &process
  );
  if (environment != nullptr)
    DestroyEnvironmentBlock(environment);
  CloseHandle(token);

  if (!started) {
    LOG_WARN("couldn't start the app after the update, error %lu", GetLastError());
    return false;
  }
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);
  LOG_INFO("started the app again after the update");
  return true;
}

} // namespace

DaemonUpdater::DaemonUpdater(QObject *parent) : QObject(parent), m_network{new QNetworkAccessManager(this)}
{
  // nothing else
}

void DaemonUpdater::install(const QString &version)
{
  if (m_busy) {
    Q_EMIT status(QStringLiteral("failed"), QStringLiteral("An update is already being installed."));
    return;
  }

  // only a newer release, from the releases page this was built with
  const auto wanted = QVersionNumber::fromString(version);
  if (wanted.isNull() || wanted <= currentVersion()) {
    Q_EMIT status(QStringLiteral("failed"), QStringLiteral("%1 isn't newer than this version.").arg(version));
    return;
  }

  m_busy = true;
  m_cancelled = false;
  LOG_INFO("looking for version %s to install", qPrintable(wanted.toString()));
  QNetworkRequest request{QUrl(kUrlReleases)};
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("%1-daemon/%2").arg(kAppId, kVersion));
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setTransferTimeout(kListTimeout);
  auto *reply = m_network->get(request);

  connect(reply, &QNetworkReply::finished, this, [this, reply, wanted] {
    reply->deleteLater();
    if (m_cancelled) {
      m_busy = false;
      Q_EMIT status(QStringLiteral("cancelled"), {});
      return;
    }
    if (reply->error() != QNetworkReply::NoError) {
      fail(QStringLiteral("Couldn't reach the releases page: %1").arg(reply->errorString()));
      return;
    }

    const auto release = findRelease(reply->readAll(), wanted);
    if (!release) {
      fail(QStringLiteral("Version %1 isn't on the releases page.").arg(wanted.toString()));
      return;
    }

    const auto folder = updateFolder();
    QDir dir(folder);
    if (folder.isEmpty() || !dir.removeRecursively() || !dir.mkpath(QStringLiteral("."))) {
      fail(QStringLiteral("Couldn't make a folder for the download."));
      return;
    }

    const auto path = dir.filePath(release->setupName);
    LOG_INFO("downloading %s", qPrintable(release->setupUrl.toString()));
    m_download = new UpdateDownload(m_network, this);
    auto lastPercent = std::make_shared<qint64>(-1);
    connect(m_download, &UpdateDownload::progress, this, [this, lastPercent](qint64 received, qint64 total) {
      if (const auto percent = received * 100 / qMax(total, qint64{1}); percent != *lastPercent) {
        *lastPercent = percent;
        Q_EMIT status(QStringLiteral("downloading"), QString::number(percent));
      }
    });
    connect(m_download, &UpdateDownload::finished, this, [this, path](const QString &error) {
      m_download->deleteLater();
      if (m_cancelled) {
        m_busy = false;
        Q_EMIT status(QStringLiteral("cancelled"), {});
      } else if (!error.isEmpty()) {
        fail(error);
      } else {
        runSetup(path);
      }
    });
    m_download->start(release->setupUrl, path, release->setupSize, release->setupSha256);
  });
}

void DaemonUpdater::cancel()
{
  if (!m_busy)
    return;
  m_cancelled = true;
  if (m_download)
    m_download->cancel();
}

void DaemonUpdater::fail(const QString &reason)
{
  LOG_WARN("couldn't install the update: %s", qPrintable(reason));
  m_busy = false;
  Q_EMIT status(QStringLiteral("failed"), reason);
}

void DaemonUpdater::runSetup(const QString &path)
{
  // setup stops this service, and the one it starts afterwards starts the app again
  QSettings state(kStateKey, QSettings::Registry64Format);
  state.setValue(kRestartValue, QDateTime::currentSecsSinceEpoch());
  state.sync();

  // quietly: the app shows how it's going, and the new service starts the app rather than setup
  const auto file = QDir::toNativeSeparators(path).toStdWString();
  auto command = L"\"" + file + L"\" /quiet /norestart LaunchAfterSetup=0";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(
          file.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process
      )) {
    state.remove(kRestartValue);
    fail(QStringLiteral("Couldn't start setup, error %1.").arg(GetLastError()));
    return;
  }
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);

  LOG_INFO("checked the download and started setup: %s", qPrintable(QDir::toNativeSeparators(path)));
  Q_EMIT status(QStringLiteral("installing"), {});
}

void DaemonUpdater::restartAppAfterUpdate()
{
  QSettings state(kStateKey, QSettings::Registry64Format);
  const auto written = state.value(kRestartValue).toLongLong();
  if (written == 0)
    return;
  state.remove(kRestartValue);
  if (QDateTime::currentSecsSinceEpoch() - written > kRestartWithinSecs)
    return;

  auto attempts = std::make_shared<int>(0);
  auto *timer = new QTimer(QCoreApplication::instance()); // NOSONAR - Qt managed
  timer->setInterval(kRestartRetry);
  QObject::connect(timer, &QTimer::timeout, timer, [timer, attempts] {
    if (startAppForSessionUser() || ++*attempts >= kRestartAttempts)
      timer->deleteLater();
  });
  timer->start();
}
