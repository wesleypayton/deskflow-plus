/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardCompressionTests.h"

#include "deskflow/ClipboardCompression.h"

using namespace deskflow::clipboardCompression;
using namespace std::string_literals;

namespace {

// a 1089x914 32-bit bitmap like a copied screenshot of a window: rows of flat colour with some detail
std::string screenshot()
{
  constexpr int width = 1089;
  constexpr int height = 914;
  std::string pixels;
  pixels.reserve(size_t{width} * height * 4);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const bool text = (y / 18) % 3 == 1 && (x * 7 + y) % 11 < 3;
      const char shade = text ? char(0x20) : char(0xf0 - (y / 100));
      pixels.append({shade, shade, shade, char(0xff)});
    }
  }
  return pixels;
}

} // namespace

void ClipboardCompressionTests::roundTrip()
{
  const std::string data = "some text\0with a null and more text"s;
  std::string unpacked;
  QVERIFY(decompress(compress(data), unpacked));
  QCOMPARE(unpacked, data);

  QVERIFY(decompress(compress({}), unpacked));
  QVERIFY(unpacked.empty());
}

void ClipboardCompressionTests::screenshotFitsOnceCompressed()
{
  // the image that wouldn't copy: about 3.9 MB against the default 3 MB limit
  const auto image = screenshot();
  constexpr size_t limit = 3 * 1024 * 1024;
  QVERIFY(image.size() > limit);
  QVERIFY(!fitsLimit(image, limit, false));
  QVERIFY(fitsLimit(image, limit, true));

  std::string unpacked;
  QVERIFY(decompress(compress(image), unpacked));
  QVERIFY(unpacked == image);
}

void ClipboardCompressionTests::rejectsMalformed()
{
  std::string unpacked;
  QVERIFY(!decompress({}, unpacked));
  QVERIFY(!decompress("abc", unpacked));
  QVERIFY(!decompress(std::string("\0\0\0\x10garbage", 11), unpacked));
}

void ClipboardCompressionTests::rejectsTooLarge()
{
  // the size at the front claims more than is allowed, so it isn't unpacked at all
  auto packed = compress("small");
  packed[0] = char(0x7f);
  std::string unpacked;
  QVERIFY(!decompress(packed, unpacked));
}

QTEST_MAIN(ClipboardCompressionTests)
