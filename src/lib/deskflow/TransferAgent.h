/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransfer.h"
#include "deskflow/TransferProtocol.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace deskflow::transfer {

//! One computer's side of file transfers: what it offers, what it sends and what it receives
/*!
The agent doesn't know about the network or the platform. It sends messages
through Hooks::send and asks Hooks::destination where incoming files go, and
reports finished and failed transfers through the other hooks.
*/
class TransferAgent
{
public:
  struct Hooks
  {
    //! deliver a message towards the server
    std::function<void(const TransferMessage &)> send;

    //! the folder an incoming transfer is written to, or empty to refuse it
    std::function<std::filesystem::path(TransferKind)> destination;

    //! an incoming transfer finished; \c items are the files and folders it created
    std::function<void(TransferKind, const std::vector<std::filesystem::path> &items)> received;

    //! a transfer, in either direction, failed or was cancelled
    std::function<void(const std::string &reason)> failed;
  };

  explicit TransferAgent(Hooks hooks, uint64_t maxReceiveBytes = UINT64_MAX);
  ~TransferAgent();

  //! Announce files this computer can send
  /*!
  Returns the offer id, or 0 if there were no files to offer. Only the most
  recent offers are kept; a request for an older one is refused.
  */
  uint32_t offer(TransferKind kind, const std::vector<std::filesystem::path> &items);

  //! Tell the server this computer no longer offers files of this kind, e.g. after copying text
  void withdraw(TransferKind kind);

  //! Handle a message from the server
  void handle(const TransferMessage &message);

  //! True while any transfer is in progress
  bool busy() const
  {
    return !m_outgoing.empty() || !m_incoming.empty();
  }

  //! Offers older than the newest few are forgotten
  static constexpr size_t kOffersKept = 4;

private:
  struct Incoming
  {
    TransferKind kind;
    std::unique_ptr<FileReceiver> receiver;
  };

  void pump(uint32_t id);
  void handleIncoming(const TransferMessage &message);
  void cancel(uint32_t id, const std::string &reason, bool tellServer);

  Hooks m_hooks;
  uint64_t m_maxReceiveBytes;
  uint32_t m_nextOffer = 1;
  std::map<uint32_t, std::vector<FileEntry>> m_offers;
  // shared so a send that loops straight back into the agent can't free a sender mid pump
  std::map<uint32_t, std::shared_ptr<FileSender>> m_outgoing;
  std::set<uint32_t> m_pumping;
  std::map<uint32_t, Incoming> m_incoming;
};

} // namespace deskflow::transfer
