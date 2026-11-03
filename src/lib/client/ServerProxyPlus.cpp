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

#include <cstring>

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
  if (m_features & deskflow::plus::kFeatureQuickKeepAlive) {
    if (memcmp(code, kMsgCKeepAlive, 4) == 0)
      noteKeepAlive();
    // resetting the options puts the keep alives back to the standard rate
    if (memcmp(code, kMsgCResetOptions, 4) == 0) {
      const auto result = ServerProxy1_8::parseMessage(code);
      if (m_quickKeepAlives)
        setKeepAliveRate(deskflow::plus::kPlusKeepAliveRate);
      return result;
    }
    // on a slow network a keep alive can wait behind a large transfer, which shows the server is there
    if (isTransferMessage(code) || memcmp(code, kMsgDClipboard, 4) == 0)
      resetKeepAliveAlarm();
  }
  if ((m_features & deskflow::plus::kFeatureRemoteInput) && memcmp(code, kMsgDRemoteCapture, 4) == 0) {
    uint8_t capture = 0;
    if (!ProtocolUtil::readf(getStream(), kMsgDRemoteCapture + 4, &capture))
      return ConnectionResult::Disconnect;
    // 2 keeps the buttons held, carrying a drag
    getClient()->setRemoteCapture(capture != 0, capture == 2);
    return ConnectionResult::Okay;
  }
  if (!(m_features & deskflow::plus::kFeatureFileTransfer) || !isTransferMessage(code))
    return ServerProxy1_8::parseMessage(code);

  TransferMessage message;
  if (!readTransferMessage(getStream(), code, message))
    return ConnectionResult::Disconnect;
  getClient()->handleTransferMessage(message);
  return ConnectionResult::Okay;
}

void ServerProxyPlus::noteKeepAlive()
{
  // a 1.0.0 server agreed to quick keep alives but went on sending them every three seconds, which a client
  // expecting one a second gave up on; so they're only expected quickly once the server is seen sending them so
  constexpr auto kQuickKeepAliveGap = std::chrono::milliseconds(2000);

  const auto now = std::chrono::steady_clock::now();
  if (!m_quickKeepAlives && m_lastKeepAlive && now - *m_lastKeepAlive < kQuickKeepAliveGap) {
    LOG_DEBUG("the server sends keep alives each second, so a lost connection is noticed sooner");
    m_quickKeepAlives = true;
    setKeepAliveRate(deskflow::plus::kPlusKeepAliveRate);
  }
  m_lastKeepAlive = now;
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
