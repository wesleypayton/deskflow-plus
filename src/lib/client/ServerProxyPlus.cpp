/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxyPlus.h"

#include "base/Log.h"
#include "client/Client.h"
#include "deskflow/Monitors.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/TransferProtocol.h"

ServerProxyPlus::ServerProxyPlus(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events)
{
}

void ServerProxyPlus::enablePlus(uint32_t serverFeatures)
{
  if (isPlus())
    return;

  m_features = serverFeatures & deskflow::plus::kFeatures;
  LOG_INFO("server is Deskflow Plus, using features 0x%x", m_features);
  ProtocolUtil::writef(getStream(), kMsgDPlusFeatures, deskflow::plus::kFeatures);

  // the computer info went before we knew, so send the monitors now
  sendExtraInfo();
}

ServerProxy::ConnectionResult ServerProxyPlus::parseMessage(const uint8_t *code)
{
  using namespace deskflow::transfer;
  if (!(m_features & deskflow::plus::kFeatureFileTransfer) || !isTransferMessage(code))
    return ServerProxy1_8::parseMessage(code);

  TransferMessage message;
  if (!readTransferMessage(getStream(), code, message))
    return ConnectionResult::Disconnect;
  getClient()->handleTransferMessage(message);
  return ConnectionResult::Okay;
}

void ServerProxyPlus::sendExtraInfo()
{
  // a stock server wouldn't understand
  if (!(m_features & deskflow::plus::kFeatureMonitors))
    return;

  auto monitors = getClient()->getMonitors();
  if (monitors.size() > deskflow::kMaxMonitors) {
    monitors.resize(deskflow::kMaxMonitors);
  }

  LOG_VERBOSE("sending %d monitors", static_cast<int>(monitors.size()));
  const auto data = deskflow::encodeMonitors(monitors);
  ProtocolUtil::writef(getStream(), kMsgDMonitors, &data);
}
