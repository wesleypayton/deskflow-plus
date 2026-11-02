/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"

#include <QTest>

class TransferRouterTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void wire_everyMessageRoundTrips();
  void relay_clientToClient();
  void relay_fromServerItself();
  void start_needsAnOffer();
  void start_clipboardOfferOnlyOncePerComputer();
  void destinationRefusal_stopsTransfer();
  void spoofedSender_isIgnored();
  void requestForUnknownOffer_isCancelled();
  void disconnect_cancelsTransfers();

private:
  Log m_log;
};
