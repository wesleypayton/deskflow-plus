/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"

//! Proxy for a protocol 1.8 server, with the Deskflow Plus features once the server says it has them
class ServerProxyPlus : public ServerProxy1_8
{
public:
  ServerProxyPlus(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxyPlus() override = default;

  //! The server announced Deskflow Plus features: answer, and start using the ones both sides have
  void enablePlus(uint32_t serverFeatures);

  //! True once the server and this client have agreed on Deskflow Plus features
  bool isPlus() const
  {
    return m_features != 0;
  }

  //! The features both sides have
  uint32_t features() const
  {
    return m_features;
  }

protected:
  void sendExtraInfo() override;
  ConnectionResult parseMessage(const uint8_t *code) override;

private:
  uint32_t m_features = 0;
};
