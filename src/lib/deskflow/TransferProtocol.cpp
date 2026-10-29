/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/TransferProtocol.h"

#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

namespace deskflow::transfer {

namespace {

using enum TransferMessage::Type;

struct Format
{
  TransferMessage::Type type;
  const char *const &format;
};

const Format kFormats[] = {
    {Offer, kMsgDFileOffer},   {Request, kMsgDFileRequest}, {Start, kMsgDFileStart},
    {Header, kMsgDFileHeader}, {Data, kMsgDFileData},       {End, kMsgDFileEnd},
    {Ack, kMsgDFileAck},       {Cancel, kMsgDFileCancel},   {Probe, kMsgDFileProbe},
};

const char *formatFor(TransferMessage::Type type)
{
  for (const auto &entry : kFormats) {
    if (entry.type == type)
      return entry.format;
  }
  return nullptr;
}

// sizes travel as two 4 byte halves, the widest integer the wire format has
uint32_t high(uint64_t value)
{
  return static_cast<uint32_t>(value >> 32);
}

uint32_t low(uint64_t value)
{
  return static_cast<uint32_t>(value & 0xffffffffu);
}

uint64_t join(uint32_t highHalf, uint32_t lowHalf)
{
  return (static_cast<uint64_t>(highHalf) << 32) | lowHalf;
}

} // namespace

bool isTransferMessage(const uint8_t *code)
{
  for (const auto &entry : kFormats) {
    if (memcmp(code, entry.format, 4) == 0)
      return true;
  }
  return false;
}

bool readTransferMessage(deskflow::IStream *stream, const uint8_t *code, TransferMessage &message)
{
  for (const auto &entry : kFormats) {
    if (memcmp(code, entry.format, 4) != 0)
      continue;

    message = TransferMessage();
    message.type = entry.type;
    const auto *args = entry.format + 4;
    uint32_t kind = 0;
    uint32_t sizeHigh = 0;
    uint32_t sizeLow = 0;
    bool ok = false;
    switch (entry.type) {
    case Offer:
      ok = ProtocolUtil::readf(stream, args, &message.id, &kind, &message.value, &sizeHigh, &sizeLow, &message.text);
      break;
    case Request:
    case Ack:
    case Probe:
      ok = ProtocolUtil::readf(stream, args, &message.id, &message.value);
      break;
    case Start:
      ok = ProtocolUtil::readf(stream, args, &message.id, &kind);
      break;
    case Header:
      ok = ProtocolUtil::readf(stream, args, &message.id, &message.value, &sizeHigh, &sizeLow, &message.text);
      break;
    case Data:
    case Cancel:
      ok = ProtocolUtil::readf(stream, args, &message.id, &message.text);
      break;
    case End:
      ok = ProtocolUtil::readf(stream, args, &message.id);
      break;
    }

    if (kind != 0 && kind != static_cast<uint32_t>(TransferKind::Clipboard) &&
        kind != static_cast<uint32_t>(TransferKind::Drag))
      return false;
    if (kind != 0)
      message.kind = static_cast<TransferKind>(kind);
    message.size = join(sizeHigh, sizeLow);
    return ok;
  }
  return false;
}

void writeTransferMessage(deskflow::IStream *stream, const TransferMessage &message)
{
  const auto *format = formatFor(message.type);
  const auto kind = static_cast<uint32_t>(message.kind);
  switch (message.type) {
  case Offer:
    ProtocolUtil::writef(
        stream, format, message.id, kind, message.value, high(message.size), low(message.size), &message.text
    );
    break;
  case Request:
  case Ack:
  case Probe:
    ProtocolUtil::writef(stream, format, message.id, message.value);
    break;
  case Start:
    ProtocolUtil::writef(stream, format, message.id, kind);
    break;
  case Header:
    ProtocolUtil::writef(
        stream, format, message.id, message.value, high(message.size), low(message.size), &message.text
    );
    break;
  case Data:
  case Cancel:
    ProtocolUtil::writef(stream, format, message.id, &message.text);
    break;
  case End:
    ProtocolUtil::writef(stream, format, message.id);
    break;
  }
}

} // namespace deskflow::transfer
