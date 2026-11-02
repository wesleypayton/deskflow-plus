/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Updater.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/UpdateDownload.h"
#include "common/VersionInfo.h"
#include "gui/ipc/DaemonIpcClient.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
// after windows.h
#include <shellapi.h>
#endif

using namespace deskflow::updates;
using namespace std::chrono_literals;

namespace {

// the network may still be starting when the app starts at sign-in
constexpr auto kFirstCheckDelay = 30s;
constexpr auto kCheckInterval = 24h;
// a check that couldn't reach GitHub tries again sooner
constexpr auto kRetryInterval = 1h;
constexpr auto kCheckTimeout = 30s;

Mode updateMode()
{
  return modeFromString(Settings::value(Settings::Gui::UpdateMode).toString());
}

QString versionLines(const Release &release)
{
  return Updater::tr("New version: %1 (<a href=\"%2\">what's new</a>)<br>Your version: %3")
      .arg(release.version.toString(), release.pageUrl.toString(QUrl::FullyEncoded), QString::fromLatin1(kVersion));
}

QString installNote()
{
  return Updater::tr("%1 closes for a moment while the update installs, then starts again.").arg(kDisplayName);
}

} // namespace

Updater::Updater(QWidget *window) : QObject(window), m_window{window}, m_network{new QNetworkAccessManager(this)}
{
  m_nextCheck.setSingleShot(true);
  connect(&m_nextCheck, &QTimer::timeout, this, &Updater::scheduledCheck);
}

void Updater::start()
{
  if (isSupported())
    m_nextCheck.start(kFirstCheckDelay);
}

void Updater::checkNow(QWidget *parent)
{
  check(parent, true);
}

void Updater::offerUpdate(QWidget *parent)
{
  if (m_newer && confirmInstall(*m_newer, parent))
    install(*m_newer, parent);
}

void Updater::scheduledCheck()
{
  // keeps checking for when the setting changes
  if (updateMode() == Mode::Never) {
    qDebug("not checking for updates, turned off in settings");
    m_nextCheck.start(kCheckInterval);
    return;
  }
  check(m_window, false);
}

void Updater::check(QWidget *parent, bool manual)
{
  const QUrl url(Settings::value(Settings::Gui::UpdateReleasesUrl).toString());
  qDebug("checking for updates at: %s", qPrintable(url.toString()));

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("%1/%2").arg(kAppId, kVersion));
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setTransferTimeout(kCheckTimeout);
  auto *reply = m_network->get(request);

  connect(reply, &QNetworkReply::finished, this, [this, reply, manual, parent = QPointer(parent)] {
    reply->deleteLater();
    const auto failed = reply->error() != QNetworkReply::NoError;
    if (!manual)
      m_nextCheck.start(failed ? kRetryInterval : kCheckInterval);
    if (manual)
      Q_EMIT checkFinished();

    if (failed) {
      qWarning("couldn't check for updates: %s", qPrintable(reply->errorString()));
      if (manual && parent)
        QMessageBox::warning(
            parent, tr("Check for updates"), tr("Couldn't check for updates: %1").arg(reply->errorString())
        );
      return;
    }

    const auto release =
        newerRelease(reply->readAll(), currentVersion(), Settings::value(Settings::Gui::UpdateTestBuilds).toBool());
    if (!release) {
      qDebug("%s is the newest version", kVersion);
      if (manual && parent)
        QMessageBox::information(
            parent, tr("Check for updates"), tr("You have the newest version of %1, %2.").arg(kDisplayName, kVersion)
        );
      return;
    }

    qInfo("a newer version is available: %s", qPrintable(release->version.toString()));
    m_newer = release;
    Q_EMIT updateFound(release->version.toString());

    if (manual) {
      if (parent && confirmInstall(*release, parent))
        install(*release, parent);
      return;
    }

    if (updateMode() == Mode::Always) {
      install(*release, nullptr);
      return;
    }
    const auto askAfter = Settings::value(Settings::Gui::UpdateAskAfter).toDateTime();
    if (updateMode() == Mode::Ask && (!askAfter.isValid() || QDateTime::currentDateTime() >= askAfter))
      askToUpdate(*release);
  });
}

void Updater::askToUpdate(const Release &release)
{
  if (m_asking || m_installing)
    return;
  m_asking = true;

  // a portable copy can't be installed over, so it's only pointed at the download
  const auto portable = Settings::isPortableMode();

  // at sign-in the window is hidden, and the question stands on its own
  QMessageBox message(m_window->isVisible() ? m_window : nullptr);
  message.setWindowTitle(tr("Update available"));
  message.setIcon(QMessageBox::Information);
  message.setTextFormat(Qt::RichText);
  message.setTextInteractionFlags(Qt::TextBrowserInteraction);
  message.setText(tr("<p>A new version of %1 is available. Would you like to update?</p><p>%2</p>")
                      .arg(kDisplayName, versionLines(release)));
  if (!portable)
    message.setInformativeText(installNote());

  // Windows puts these in the order Always, Yes, No, Never
  QPushButton *always = nullptr;
  if (!portable) {
    always = message.addButton(tr("Always"), QMessageBox::YesRole);
    always->setToolTip(tr("Update now, and install new versions automatically from now on"));
  }
  auto *yes = message.addButton(tr("Yes"), QMessageBox::AcceptRole);
  yes->setToolTip(portable ? tr("Open the download page") : tr("Update this time"));
  auto *no = message.addButton(tr("No"), QMessageBox::NoRole);
  no->setToolTip(tr("Don't update this time. You'll be asked again in %n day(s).", nullptr, kAskAgainDays));
  auto *never = message.addButton(tr("Never"), QMessageBox::RejectRole);
  never->setToolTip(tr("Don't update, and don't ask again. You can change this in Settings."));
  message.setDefaultButton(yes);
  message.setEscapeButton(no);
  message.exec();
  m_asking = false;

  const auto *clicked = message.clickedButton();
  if (always != nullptr && clicked == always)
    Settings::setValue(Settings::Gui::UpdateMode, modeToString(Mode::Always));
  else if (clicked == never)
    Settings::setValue(Settings::Gui::UpdateMode, modeToString(Mode::Never));
  else
    Settings::setValue(Settings::Gui::UpdateAskAfter, QDateTime::currentDateTime().addDays(kAskAgainDays));

  if ((always != nullptr && clicked == always) || clicked == yes)
    install(release, m_window);
}

bool Updater::confirmInstall(const Release &release, QWidget *parent) const
{
  const auto portable = Settings::isPortableMode();

  QMessageBox message(parent);
  message.setWindowTitle(tr("Update available"));
  message.setIcon(QMessageBox::Information);
  message.setTextFormat(Qt::RichText);
  message.setTextInteractionFlags(Qt::TextBrowserInteraction);
  message.setText(tr("<p>A new version of %1 is available.</p><p>%2</p>").arg(kDisplayName, versionLines(release)));
  if (!portable)
    message.setInformativeText(installNote());
  auto *install = message.addButton(portable ? tr("Open download page") : tr("Update now"), QMessageBox::AcceptRole);
  message.addButton(tr("Not now"), QMessageBox::RejectRole);
  message.setDefaultButton(install);
  message.exec();
  return message.clickedButton() == install;
}

void Updater::install(const Release &release, QWidget *parent)
{
  if (Settings::isPortableMode()) {
    QDesktopServices::openUrl(release.pageUrl);
    return;
  }
  if (m_installing)
    return;
  m_installing = true;

  // the service downloads and checks the setup itself, and runs it with no permission prompt; it's only told
  // which version to install
  auto *service = new deskflow::gui::ipc::DaemonIpcClient(this);
  QPointer<QProgressDialog> progress;
  if (parent != nullptr) {
    progress = new QProgressDialog(
        tr("Downloading %1 %2...").arg(kDisplayName, release.version.toString()), tr("Cancel"), 0, 100, parent
    );
    progress->setWindowTitle(tr("Updating %1").arg(kDisplayName));
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    connect(progress, &QProgressDialog::canceled, service, &deskflow::gui::ipc::DaemonIpcClient::sendCancelUpdate);
    progress->show();
  }

  const auto version = release.version.toString();
  connect(service, &deskflow::gui::ipc::DaemonIpcClient::connected, service, [service, version] {
    service->sendInstallUpdate(version);
  });
  const auto withoutService = [this, service, progress, release, parent = QPointer(parent)] {
    qInfo("the background service can't install the update, so setup will ask for permission");
    service->deleteLater();
    if (progress)
      progress->deleteLater();
    m_installing = false;
    installWithPermission(release, parent);
  };
  connect(service, &deskflow::gui::ipc::DaemonIpcClient::connectionFailed, this, withoutService);
  connect(service, &deskflow::gui::ipc::DaemonIpcClient::versionMismatch, this, withoutService);
  connect(
      service, &deskflow::gui::ipc::DaemonIpcClient::updateStatus, this,
      [this, service, progress, parent = QPointer(parent)](const QString &state, const QString &detail) {
        if (state == QStringLiteral("downloading")) {
          if (progress)
            progress->setValue(detail.toInt());
          return;
        }

        service->deleteLater();
        if (progress)
          progress->deleteLater();
        m_installing = false;
        if (state == QStringLiteral("installing")) {
          qInfo("the background service is installing the update");
          Q_EMIT installerStarted();
        } else if (state == QStringLiteral("cancelled")) {
          qInfo("the update was cancelled");
        } else {
          qWarning("the update failed: %s", qPrintable(detail));
          if (parent)
            QMessageBox::warning(parent, tr("Update"), tr("Couldn't install the update. %1").arg(detail));
        }
      }
  );
  service->connectToServer();
}

void Updater::installWithPermission(const Release &release, QWidget *parent)
{
  if (m_installing)
    return;
  m_installing = true;

  // only the newest download is kept
  QDir folder(QDir::temp().filePath(QStringLiteral("DeskflowPlusUpdate")));
  folder.removeRecursively();
  folder.mkpath(QStringLiteral("."));
  const auto path = folder.filePath(release.setupName);
  qInfo("downloading the update from: %s", qPrintable(release.setupUrl.toString()));

  auto *download = new UpdateDownload(m_network, this);
  QPointer<QProgressDialog> progress;
  if (parent != nullptr) {
    progress = new QProgressDialog(
        tr("Downloading %1 %2...").arg(kDisplayName, release.version.toString()), tr("Cancel"), 0, 100, parent
    );
    progress->setWindowTitle(tr("Updating %1").arg(kDisplayName));
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    connect(progress, &QProgressDialog::canceled, download, &UpdateDownload::cancel);
    connect(download, &UpdateDownload::progress, progress, [progress](qint64 received, qint64 total) {
      progress->setValue(static_cast<int>(received * 100 / qMax(total, qint64{1})));
    });
    progress->show();
  }

  connect(
      download, &UpdateDownload::finished, this,
      [this, download, progress, path, parent = QPointer(parent)](const QString &error) {
        download->deleteLater();
        const auto cancelled = progress && progress->wasCanceled();
        if (progress)
          progress->deleteLater();
        m_installing = false;

        if (!error.isEmpty()) {
          qWarning("couldn't download the update: %s", qPrintable(error));
          if (parent && !cancelled)
            QMessageBox::warning(parent, tr("Update"), tr("Couldn't download the update. %1").arg(error));
          return;
        }

        qInfo("downloaded and checked the update, starting: %s", qPrintable(QDir::toNativeSeparators(path)));
        if (runSetup(path))
          Q_EMIT installerStarted();
      }
  );

  download->start(release.setupUrl, path, release.setupSize, release.setupSha256);
}

bool Updater::runSetup(const QString &path)
{
#ifdef Q_OS_WIN
  // asks for permission now, rather than once setup is running, so a "no" leaves the app running
  const auto file = QDir::toNativeSeparators(path).toStdWString();
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOASYNC;
  info.lpVerb = L"runas";
  info.lpFile = file.c_str();
  // progress only, nothing to click; setup starts the app again when it's done
  info.lpParameters = L"/passive";
  info.nShow = SW_SHOWNORMAL;
  if (ShellExecuteExW(&info))
    return true;

  if (const auto error = GetLastError(); error == ERROR_CANCELLED)
    qInfo("the update was declined at the permission prompt");
  else
    qWarning("couldn't start the update, error: %lu", error);
#else
  Q_UNUSED(path)
#endif
  return false;
}
