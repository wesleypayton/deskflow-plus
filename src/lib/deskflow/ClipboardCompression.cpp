/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardCompression.h"

#include <QByteArray>

namespace deskflow::clipboardCompression {

namespace {

// fast rather than small: screenshots shrink most of the way at the lowest level, and this runs as the cursor
// crosses over
constexpr int kLevel = 1;

// qCompress starts with the unpacked size, big-endian
constexpr size_t kHeaderSize = 4;

} // namespace

std::string compress(const std::string &data)
{
  const auto packed =
      qCompress(reinterpret_cast<const uchar *>(data.data()), static_cast<qsizetype>(data.size()), kLevel);
  return packed.toStdString();
}

bool decompress(const std::string &packed, std::string &data)
{
  if (packed.size() < kHeaderSize)
    return false;

  const auto *header = reinterpret_cast<const unsigned char *>(packed.data());
  const size_t size = (size_t{header[0]} << 24) | (size_t{header[1]} << 16) | (size_t{header[2]} << 8) | header[3];
  if (size > kMaxUnpackedSize)
    return false;

  const auto unpacked =
      qUncompress(reinterpret_cast<const uchar *>(packed.data()), static_cast<qsizetype>(packed.size()));
  if (static_cast<size_t>(unpacked.size()) != size)
    return false;

  data = unpacked.toStdString();
  return true;
}

bool fitsLimit(const std::string &data, size_t limit, bool compressed)
{
  return data.size() <= limit || (compressed && compress(data).size() <= limit);
}

} // namespace deskflow::clipboardCompression
