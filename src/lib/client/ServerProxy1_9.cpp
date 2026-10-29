/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "base/Log.h"
#include "client/Client.h"
#include "deskflow/Monitors.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/TransferProtocol.h"

ServerProxy1_9::ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events)
{
}

ServerProxy::ConnectionResult ServerProxy1_9::parseMessage(const uint8_t *code)
{
  using namespace deskflow::transfer;
  if (!isTransferMessage(code))
    return ServerProxy1_8::parseMessage(code);

  TransferMessage message;
  if (!readTransferMessage(getStream(), code, message))
    return ConnectionResult::Disconnect;
  getClient()->handleTransferMessage(message);
  return ConnectionResult::Okay;
}

void ServerProxy1_9::sendExtraInfo()
{
  auto monitors = getClient()->getMonitors();
  if (monitors.size() > deskflow::kMaxMonitors) {
    monitors.resize(deskflow::kMaxMonitors);
  }

  LOG_VERBOSE("sending %d monitors", static_cast<int>(monitors.size()));
  const auto data = deskflow::encodeMonitors(monitors);
  ProtocolUtil::writef(getStream(), kMsgDMonitors, &data);
}
