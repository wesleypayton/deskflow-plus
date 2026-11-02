/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateDownload.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

UpdateDownload::UpdateDownload(QNetworkAccessManager *network, QObject *parent) : QObject(parent), m_network{network}
{
  // nothing
}

void UpdateDownload::start(const QUrl &url, const QString &path, qint64 size, const QByteArray &sha256)
{
  m_size = size;
  m_sha256 = sha256;
  m_file.setFileName(path);
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    // let the caller connect to the result before it arrives
    QMetaObject::invokeMethod(this, [this] { fail(m_file.errorString()); }, Qt::QueuedConnection);
    return;
  }

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  m_reply = m_network->get(request);
  connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownload::readAvailable);
  connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
    Q_EMIT progress(received, m_size);
  });
  connect(m_reply, &QNetworkReply::finished, this, &UpdateDownload::replyFinished);
}

void UpdateDownload::cancel()
{
  m_cancelled = true;
  if (m_reply != nullptr)
    m_reply->abort();
}

void UpdateDownload::readAvailable()
{
  const auto data = m_reply->readAll();
  m_hash.addData(data);
  if (m_file.write(data) != data.size()) {
    m_reply->abort();
    return;
  }
  // a file larger than the release says can't be the one it lists
  if (m_file.size() > m_size)
    m_reply->abort();
}

void UpdateDownload::replyFinished()
{
  // an aborted reply has nothing more to read
  if (m_reply->isOpen())
    readAvailable();
  m_reply->deleteLater();
  const auto error = m_reply->error();
  const auto errorText = m_reply->errorString();
  m_reply = nullptr;

  if (m_cancelled)
    fail(tr("The download was cancelled."));
  else if (m_file.error() != QFileDevice::NoError)
    fail(m_file.errorString());
  else if (m_file.size() != m_size)
    fail(tr("The file is %1 bytes, but the release lists %2.").arg(m_file.size()).arg(m_size));
  else if (error != QNetworkReply::NoError)
    fail(errorText);
  else if (m_hash.result().toHex() != m_sha256)
    fail(tr("The file isn't the one the release lists: its SHA-256 doesn't match."));
  else {
    m_file.close();
    Q_EMIT finished({});
  }
}

void UpdateDownload::fail(const QString &error)
{
  m_file.close();
  m_file.remove();
  Q_EMIT finished(error);
}
