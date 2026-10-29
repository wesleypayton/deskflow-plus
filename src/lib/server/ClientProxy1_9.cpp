/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events),
      m_events(events)
{
  // do nothing
}

deskflow::MonitorList ClientProxy1_9::getMonitors() const
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

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDMonitors, 4) == 0) {
    return recvMonitors();
  }
  return ClientProxy1_8::parseMessage(code);
}

bool ClientProxy1_9::recvMonitors()
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
