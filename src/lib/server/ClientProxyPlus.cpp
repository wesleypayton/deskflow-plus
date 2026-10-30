/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxyPlus.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/RemoteInput.h"
#include "deskflow/TransferProtocol.h"
#include "server/Server.h"

#include <array>
#include <cstring>

ClientProxyPlus::ClientProxyPlus(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events),
      m_events(events)
{
  // do nothing
}

deskflow::MonitorList ClientProxyPlus::getMonitors() const
{
  // the monitors are checked against the shape when they arrive, but a newer
  // shape may have arrived since without a monitor list to go with it
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  getShape(x, y, w, h);
  if (deskflow::decodeMonitors(deskflow::encodeMonitors(m_monitors), x, y, w, h).empty()) {
    return ClientProxy1_8::getMonitors();
  }
  return m_monitors;
}

bool ClientProxyPlus::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDPlusFeatures, 4) == 0) {
    return recvPlusFeatures();
  }
  if (memcmp(code, kMsgDMonitors, 4) == 0 && (m_features & deskflow::plus::kFeatureMonitors)) {
    return recvMonitors();
  }
  if (memcmp(code, kMsgDRemoteInput, 4) == 0 && supportsRemoteInput()) {
    return recvRemoteInput();
  }
  if ((m_features & deskflow::plus::kFeatureFileTransfer) && deskflow::transfer::isTransferMessage(code)) {
    deskflow::transfer::TransferMessage message;
    if (!deskflow::transfer::readTransferMessage(getStream(), code, message))
      return false;
    getServer()->handleTransferMessage(this, message);
    return true;
  }
  return ClientProxy1_8::parseMessage(code);
}

void ClientProxyPlus::sendTransfer(const deskflow::transfer::TransferMessage &message)
{
  deskflow::transfer::writeTransferMessage(getStream(), message);
}

void ClientProxyPlus::setRemoteCapture(bool capture)
{
  ProtocolUtil::writef(getStream(), kMsgDRemoteCapture, capture ? 1 : 0);
}

bool ClientProxyPlus::recvRemoteInput()
{
  uint8_t type = 0;
  std::array<uint32_t, 4> values{};
  if (!ProtocolUtil::readf(getStream(), kMsgDRemoteInput + 4, &type, &values[0], &values[1], &values[2], &values[3])) {
    return false;
  }

  deskflow::remote::RemoteInput input;
  input.type = static_cast<deskflow::remote::InputType>(type);
  input.a = static_cast<int32_t>(values[0]);
  input.b = static_cast<int32_t>(values[1]);
  input.c = static_cast<int32_t>(values[2]);
  input.d = static_cast<int32_t>(values[3]);
  getServer()->handleRemoteInput(this, input);
  return true;
}

bool ClientProxyPlus::recvPlusFeatures()
{
  uint32_t features = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgDPlusFeatures + 4, &features)) {
    return false;
  }

  m_features = features & deskflow::plus::kFeatures;
  LOG_INFO("client \"%s\" is Deskflow Plus, using features 0x%x", getName().c_str(), m_features);
  return true;
}

bool ClientProxyPlus::recvMonitors()
{
  std::vector<uint32_t> data;
  if (!ProtocolUtil::readf(getStream(), kMsgDMonitors + 4, &data)) {
    return false;
  }

  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  getShape(x, y, w, h);
  m_monitors = deskflow::decodeMonitors(data, x, y, w, h);
  if (m_monitors.empty()) {
    // not fatal, the client is still usable as one big monitor
    LOG_WARN("ignoring invalid monitor list from \"%s\"", getName().c_str());
    return true;
  }

  LOG_DEBUG("received client \"%s\" monitors:", getName().c_str());
  for (const auto &monitor : m_monitors) {
    LOG_DEBUG("  %d,%d %dx%d", monitor.x, monitor.y, monitor.width, monitor.height);
  }

  m_events->addEvent(Event(EventTypes::ComputerShapeChanged, getEventTarget()));
  return true;
}
