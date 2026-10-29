/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Monitors.h"
#include "server/ClientProxy1_8.h"

//! Proxy for client implementing protocol version 1.9
class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  deskflow::MonitorList getMonitors() const override;
  bool supportsFileTransfer() const override
  {
    return true;
  }
  void sendTransfer(const deskflow::transfer::TransferMessage &message) override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvMonitors();

  deskflow::MonitorList m_monitors;
  IEventQueue *m_events = nullptr;
};
