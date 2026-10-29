/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/TransferRouter.h"

#include "base/Log.h"

#include <vector>

namespace deskflow::server {

using Type = deskflow::transfer::TransferMessage::Type;

TransferRouter::TransferRouter(Deliver deliver) : m_deliver(std::move(deliver))
{
  // nothing to do
}

void TransferRouter::receive(const std::string &from, const Message &message)
{
  if (message.type == Type::Offer) {
    if (message.value == 0) {
      m_offers.erase({from, message.kind});
      return;
    }
    LOG_DEBUG(
        "\"%s\" offers %u files (%llu bytes)", from.c_str(), message.value,
        static_cast<unsigned long long>(message.size)
    );
    m_offers[{from, message.kind}] = {message.id, message.value, message.size, message.text};
    return;
  }

  const auto route = m_routes.find(message.id);
  if (route == m_routes.end())
    return;

  // only pass a message on from the end of the transfer that should send it
  switch (message.type) {
  case Type::Header:
  case Type::Data:
  case Type::End:
    if (from != route->second.source)
      return;
    m_deliver(route->second.destination, message);
    if (message.type == Type::End) {
      LOG_INFO("sent files from \"%s\" to \"%s\"", route->second.source.c_str(), route->second.destination.c_str());
      m_routes.erase(route);
    }
    break;

  case Type::Ack:
    if (from == route->second.destination)
      m_deliver(route->second.source, message);
    break;

  case Type::Cancel: {
    if (from != route->second.source && from != route->second.destination)
      return;
    const auto other = from == route->second.source ? route->second.destination : route->second.source;
    LOG_WARN("file transfer to \"%s\" stopped: %s", route->second.destination.c_str(), message.text.c_str());
    m_routes.erase(route);
    m_deliver(other, message);
    break;
  }

  case Type::Offer:
  case Type::Request:
  case Type::Start:
  case Type::Probe:
    // only the server sends these
    break;
  }
}

bool TransferRouter::start(Kind kind, const std::string &source, const std::string &destination)
{
  if (source == destination)
    return false;

  const auto offer = m_offers.find({source, kind});
  if (offer == m_offers.end())
    return false;

  const auto delivery = std::make_tuple(source, offer->second.id, destination);
  if (kind == Kind::Clipboard && m_delivered.contains(delivery))
    return false;
  m_delivered.insert(delivery);

  const auto id = m_nextId++;
  m_routes[id] = {source, destination};
  LOG_INFO("sending %u files from \"%s\" to \"%s\"", offer->second.count, source.c_str(), destination.c_str());

  // the destination gets ready first, as the source may start sending at once
  Message startMessage;
  startMessage.type = Type::Start;
  startMessage.id = id;
  startMessage.kind = kind;
  m_deliver(destination, startMessage);

  if (!m_routes.contains(id))
    return false; // the destination refused straight away

  Message request;
  request.type = Type::Request;
  request.id = id;
  request.value = offer->second.id;
  m_deliver(source, request);
  return true;
}

std::optional<TransferRouter::Offer> TransferRouter::latestOffer(Kind kind, const std::string &computer) const
{
  const auto offer = m_offers.find({computer, kind});
  if (offer == m_offers.end())
    return std::nullopt;
  return offer->second;
}

void TransferRouter::forgetOffer(Kind kind, const std::string &computer)
{
  m_offers.erase({computer, kind});
}

void TransferRouter::disconnected(const std::string &computer)
{
  std::erase_if(m_offers, [&computer](const auto &offer) { return offer.first.first == computer; });

  std::vector<std::pair<std::string, Message>> cancels;
  for (auto route = m_routes.begin(); route != m_routes.end();) {
    if (route->second.source == computer || route->second.destination == computer) {
      Message cancel;
      cancel.type = Type::Cancel;
      cancel.id = route->first;
      cancel.text = "\"" + computer + "\" disconnected";
      const auto other = route->second.source == computer ? route->second.destination : route->second.source;
      cancels.emplace_back(other, cancel);
      route = m_routes.erase(route);
    } else {
      ++route;
    }
  }

  for (const auto &[other, cancel] : cancels)
    m_deliver(other, cancel);
}

} // namespace deskflow::server
