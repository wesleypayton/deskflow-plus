/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/TransferAgent.h"

namespace deskflow::transfer {

using enum TransferMessage::Type;

TransferAgent::TransferAgent(Hooks hooks, uint64_t maxReceiveBytes)
    : m_hooks(std::move(hooks)),
      m_maxReceiveBytes(maxReceiveBytes)
{
  // nothing to do
}

TransferAgent::~TransferAgent() = default;

uint32_t TransferAgent::offer(TransferKind kind, const std::vector<std::filesystem::path> &items)
{
  auto files = collectFiles(items);
  if (files.empty())
    return 0;

  TransferMessage message;
  message.type = Offer;
  message.id = m_nextOffer++;
  message.kind = kind;
  message.value = static_cast<uint32_t>(files.size());
  for (const auto &file : files)
    message.size += file.size;
  for (const auto &item : items) {
    if (!message.text.empty())
      message.text += '\n';
    message.text += pathToUtf8(item.filename());
  }

  m_offers.emplace(message.id, std::move(files));
  while (m_offers.size() > kOffersKept)
    m_offers.erase(m_offers.begin());

  m_hooks.send(message);
  return message.id;
}

void TransferAgent::handle(const TransferMessage &message)
{
  switch (message.type) {
  case Request: {
    const auto offer = m_offers.find(message.value);
    if (offer == m_offers.end()) {
      m_hooks.send({Cancel, message.id, 0, {}, 0, "the files are no longer offered"});
      return;
    }
    m_outgoing[message.id] = std::make_shared<FileSender>(offer->second);
    pump(message.id);
    break;
  }

  case Ack:
    if (const auto outgoing = m_outgoing.find(message.id); outgoing != m_outgoing.end()) {
      outgoing->second->acknowledge(message.value);
      pump(message.id);
    }
    break;

  case Cancel:
    cancel(message.id, message.text, false);
    break;

  case Start:
  case Header:
  case Data:
  case End:
    handleIncoming(message);
    break;

  case Offer:
    // only the server is interested in offers
    break;
  }
}

void TransferAgent::pump(uint32_t id)
{
  // an acknowledgement arriving while already pumping just lets the running loop carry on
  if (m_pumping.contains(id))
    return;

  const auto outgoing = m_outgoing.find(id);
  if (outgoing == m_outgoing.end())
    return;

  const auto keepAlive = outgoing->second;
  auto &sender = *keepAlive;
  m_pumping.insert(id);
  std::string error;
  while (m_outgoing.contains(id)) {
    auto next = sender.next(error);
    if (!next)
      break;

    TransferMessage message;
    message.id = id;
    switch (next->kind) {
      using enum FileSender::Message::Kind;
    case Header:
      message.type = TransferMessage::Type::Header;
      message.text = std::move(next->relativePath);
      message.size = next->size;
      break;
    case Data:
      message.type = TransferMessage::Type::Data;
      message.text = std::move(next->data);
      break;
    case End:
      message.type = TransferMessage::Type::End;
      break;
    }
    m_hooks.send(message);
  }
  m_pumping.erase(id);

  if (!error.empty()) {
    cancel(id, error, true);
  } else if (sender.finished()) {
    m_outgoing.erase(id);
  }
}

void TransferAgent::handleIncoming(const TransferMessage &message)
{
  if (message.type == Start) {
    const auto destination = m_hooks.destination ? m_hooks.destination(message.kind) : std::filesystem::path();
    if (destination.empty()) {
      m_hooks.send({Cancel, message.id, 0, {}, 0, "this computer isn't accepting files"});
      return;
    }
    m_incoming[message.id] = {message.kind, std::make_unique<FileReceiver>(destination, m_maxReceiveBytes)};
    return;
  }

  const auto incoming = m_incoming.find(message.id);
  if (incoming == m_incoming.end())
    return;

  auto &receiver = *incoming->second.receiver;
  std::string error;
  bool ok = true;
  if (message.type == Header)
    ok = receiver.beginFile(message.text, message.size, error);
  else if (message.type == Data)
    ok = receiver.write(message.text, error);
  else
    ok = receiver.finish(error);

  if (!ok) {
    cancel(message.id, error, true);
    return;
  }

  if (receiver.countMessage() || message.type == End)
    m_hooks.send({Ack, message.id, receiver.handled(), {}, 0, {}});

  if (message.type == End) {
    const auto kind = incoming->second.kind;
    const auto items = receiver.topLevelItems();
    m_incoming.erase(incoming);
    if (m_hooks.received)
      m_hooks.received(kind, items);
  }
}

void TransferAgent::cancel(uint32_t id, const std::string &reason, bool tellServer)
{
  const bool known = m_outgoing.erase(id) > 0 || m_incoming.erase(id) > 0;
  if (!known)
    return;

  if (tellServer)
    m_hooks.send({Cancel, id, 0, {}, 0, reason});
  if (m_hooks.failed)
    m_hooks.failed(reason);
}

} // namespace deskflow::transfer
