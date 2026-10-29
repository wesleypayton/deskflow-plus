/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <string>

namespace deskflow {
class IStream;
}

namespace deskflow::transfer {

//! Why files are being copied, which decides where they end up
enum class TransferKind : uint8_t
{
  Clipboard = 1, //!< copied files, put on the destination's clipboard
  Drag = 2       //!< files dragged across an edge, dropped where the button is released
};

//! One file transfer message, as sent over the wire between Deskflow Plus computers
/*!
Every computer announces what it can send with an Offer. The server starts a
transfer by sending Start to the destination and Request to the source, then
passes Header, Data and End from the source to the destination and Ack back.
Either side can Cancel. A source only ever sends files it offered itself.
*/
struct TransferMessage
{
  enum class Type
  {
    Offer,   //!< "DFOF": id = offer id, value = file count, size = total bytes, text = names, kind
    Request, //!< "DFRQ": id = transfer id, value = offer id
    Start,   //!< "DFST": id = transfer id, kind, size = total bytes
    Header,  //!< "DFHD": id = transfer id, value = 1 for an empty folder, size = file size, text = relative path
    Data,    //!< "DFDT": id = transfer id, text = bytes
    End,     //!< "DFEN": id = transfer id
    Ack,     //!< "DFAK": id = transfer id, value = messages handled
    Cancel,  //!< "DFCN": id = transfer id, text = reason
    Probe    //!< "DFPB" (server to client): value = 1 to look for a file drag at the cursor, 0 to stop
  };

  Type type = Type::Cancel;
  uint32_t id = 0;
  uint32_t value = 0;
  TransferKind kind = TransferKind::Clipboard;
  uint64_t size = 0;
  std::string text;
};

//! True if \c code is the four byte code of a transfer message
bool isTransferMessage(const uint8_t *code);

//! Read the rest of a transfer message whose code has already been read
bool readTransferMessage(deskflow::IStream *stream, const uint8_t *code, TransferMessage &message);

void writeTransferMessage(deskflow::IStream *stream, const TransferMessage &message);

} // namespace deskflow::transfer
