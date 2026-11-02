/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Monitors.h"
#include "server/ClientProxy1_8.h"

#include <chrono>

//! Proxy for a protocol 1.8 client, with the Deskflow Plus features once the client says it has them
class ClientProxyPlus : public ClientProxy1_8
{
public:
  ClientProxyPlus(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxyPlus() override;

  deskflow::MonitorList getMonitors() const override;
  bool supportsFileTransfer() const override
  {
    return (m_features & deskflow::plus::kFeatureFileTransfer) != 0;
  }
  void sendTransfer(const deskflow::transfer::TransferMessage &message) override;
  bool supportsRemoteInput() const override
  {
    return (m_features & deskflow::plus::kFeatureRemoteInput) != 0;
  }
  void setRemoteCapture(bool capture) override;
  bool compressesClipboard() const override
  {
    return (m_features & deskflow::plus::kFeatureCompressedClipboard) != 0;
  }

protected:
  bool parseMessage(const uint8_t *code) override;
  void keepAlive() override;
  void resetHeartbeatRate() override;

private:
  bool recvMonitors();
  bool recvPlusFeatures();
  bool recvRemoteInput();
  void noteRoundTrip(std::chrono::steady_clock::duration time);

  uint32_t m_features = 0;

  // how long keep alives take to come back, to tell the user when the connection is slow
  std::chrono::steady_clock::time_point m_keepAliveSent;
  bool m_awaitingKeepAlive = false;
  int m_slowInARow = 0;
  int m_quickInARow = 0;
  bool m_slow = false;

  deskflow::MonitorList m_monitors;
  IEventQueue *m_events = nullptr;
};
