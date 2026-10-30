/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class FileTransferTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void safePath_acceptsNormalNames();
  void safePath_rejectsEscapes();
  void safePath_rejectsNamesWindowsWouldMangle();
  void collect_walksFolders();
  void roundTrip_copiesEverything();
  void sender_waitsForAcknowledgement();
  void sender_reportsShrunkFile();
  void receiver_refusesUnsafeFolder();
  void receiver_neverOverwrites();
  void receiver_enforcesLimit();
  void receiver_refusesMalformedStreams();
  void receiver_cancelRemovesPartialFile();
};
