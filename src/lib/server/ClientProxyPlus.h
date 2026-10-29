/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Monitors.h"
#include "server/ClientProxy1_8.h"

//! Proxy for a protocol 1.8 client, with the Deskflow Plus features once the client says it has them
class ClientProxyPlus : public ClientProxy1_8
{
public:
  ClientProxyPlus(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxyPlus() override = default;

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

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvMonitors();
  bool recvPlusFeatures();
  bool recvRemoteInput();

  uint32_t m_features = 0;

  deskflow::MonitorList m_monitors;
  IEventQueue *m_events = nullptr;
};
