/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <string>

// Clipboard data compressed for the network, between Deskflow Plus computers that both have
// kFeatureCompressedClipboard. A copied screenshot is a raw bitmap, often several times larger
// than the clipboard limit, that compresses to a fraction of it.
namespace deskflow::clipboardCompression {

//! Unpacked clipboards larger than this are refused, so a small message can't take all the memory
inline constexpr size_t kMaxUnpackedSize = 512 * 1024 * 1024;

std::string compress(const std::string &data);

//! False if \p packed isn't compressed clipboard data, or would unpack to more than kMaxUnpackedSize
bool decompress(const std::string &packed, std::string &data);

//! Whether \p data takes no more than \p limit bytes on the network, where it's compressed if \p compressed
bool fitsLimit(const std::string &data, size_t limit, bool compressed);

} // namespace deskflow::clipboardCompression
