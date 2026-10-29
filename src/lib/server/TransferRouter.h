/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/TransferProtocol.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

namespace deskflow::server {

//! Starts file transfers between computers and relays their messages
/*!
Every file goes from its source computer through the server to its
destination; the router remembers which computer is at each end of each
transfer and only passes a message on when it comes from the right end.
*/
class TransferRouter
{
public:
  using Kind = deskflow::transfer::TransferKind;
  using Message = deskflow::transfer::TransferMessage;

  //! Send a message to a computer, by name
  using Deliver = std::function<void(const std::string &computer, const Message &)>;

  struct Offer
  {
    uint32_t id = 0;
    uint32_t count = 0;
    uint64_t size = 0;
    std::string names;
  };

  explicit TransferRouter(Deliver deliver);

  //! A message from a computer: offers are remembered, transfer messages are passed on
  void receive(const std::string &from, const Message &message);

  //! Copy a computer's latest offer of this kind to another computer
  /*!
  Returns false if there is nothing to send, or the computer is the same at
  both ends. A clipboard offer is only ever copied once to each computer.
  */
  bool start(Kind kind, const std::string &source, const std::string &destination);

  //! The latest offer of this kind from a computer, if it made one
  std::optional<Offer> latestOffer(Kind kind, const std::string &computer) const;

  //! Forget a computer's offers, e.g. when its clipboard no longer holds files
  void forgetOffer(Kind kind, const std::string &computer);

  //! A computer went away: cancel its transfers and forget its offers
  void disconnected(const std::string &computer);

  //! True while any transfer is running
  bool busy() const
  {
    return !m_routes.empty();
  }

private:
  struct Route
  {
    std::string source;
    std::string destination;
  };

  Deliver m_deliver;
  uint32_t m_nextId = 1;
  std::map<std::pair<std::string, Kind>, Offer> m_offers;
  std::map<uint32_t, Route> m_routes;
  std::set<std::tuple<std::string, uint32_t, std::string>> m_delivered;
};

} // namespace deskflow::server
