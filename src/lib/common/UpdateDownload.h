/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Downloads an update's setup to a file, which is kept only when its size and SHA-256 are the
// ones the release lists.
class UpdateDownload : public QObject
{
  Q_OBJECT
public:
  explicit UpdateDownload(QNetworkAccessManager *network, QObject *parent = nullptr);

  void start(const QUrl &url, const QString &path, qint64 size, const QByteArray &sha256);
  void cancel();

Q_SIGNALS:
  void progress(qint64 received, qint64 total);
  //! \p error is empty when the file arrived whole and unchanged
  void finished(const QString &error);

private:
  void readAvailable();
  void replyFinished();
  void fail(const QString &error);

  QNetworkAccessManager *m_network;
  QNetworkReply *m_reply = nullptr;
  QFile m_file;
  QCryptographicHash m_hash{QCryptographicHash::Sha256};
  qint64 m_size = 0;
  QByteArray m_sha256;
  bool m_cancelled = false;
};
