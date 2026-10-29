/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTransferTests.h"

#include "deskflow/FileTransfer.h"

#include <QTemporaryDir>

#include <fstream>

using namespace deskflow::transfer;
namespace fs = std::filesystem;

namespace {

fs::path root(const QTemporaryDir &dir)
{
  return pathFromUtf8(dir.path().toStdString());
}

void writeFile(const fs::path &path, const std::string &content)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << content;
}

std::string readFile(const fs::path &path)
{
  std::ifstream stream(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(stream), {});
}

// a pattern that differs from chunk to chunk, so misplaced chunks are caught
std::string pattern(size_t size)
{
  std::string data(size, '\0');
  for (size_t i = 0; i < size; ++i)
    data[i] = static_cast<char>((i * 31 + i / kChunkSize) & 0xff);
  return data;
}

// pass every message from sender to receiver, acknowledging the way the protocol does
bool transfer(FileSender &sender, FileReceiver &receiver, std::string &error)
{
  using enum FileSender::Message::Kind;
  while (!sender.finished()) {
    auto message = sender.next(error);
    if (!message) {
      if (!error.empty())
        return false;
      sender.acknowledge(receiver.handled());
      continue;
    }

    bool ok = true;
    if (message->kind == Header)
      ok = receiver.beginFile(message->relativePath, message->size, error);
    else if (message->kind == Data)
      ok = receiver.write(message->data, error);
    else
      ok = receiver.finish(error);
    if (!ok)
      return false;

    if (receiver.countMessage())
      sender.acknowledge(receiver.handled());
  }
  return true;
}

} // namespace

void FileTransferTests::safePath_acceptsNormalNames()
{
  QVERIFY(safeRelativePath("report.pdf"));
  QVERIFY(safeRelativePath("Photos/2026/beach.jpg"));
  QVERIFY(safeRelativePath("caf\xc3\xa9/\xc3\xbc.txt"));
  QVERIFY(safeRelativePath(".gitignore"));
  QCOMPARE(*safeRelativePath("a/b/c.txt"), fs::path("a") / "b" / "c.txt");
}

void FileTransferTests::safePath_rejectsEscapes()
{
  QVERIFY(!safeRelativePath(""));
  QVERIFY(!safeRelativePath("../secret"));
  QVERIFY(!safeRelativePath("a/../../b"));
  QVERIFY(!safeRelativePath("./a"));
  QVERIFY(!safeRelativePath("/etc/passwd"));
  QVERIFY(!safeRelativePath("C:/Windows/system.ini"));
  QVERIFY(!safeRelativePath("a\\..\\b"));
  QVERIFY(!safeRelativePath("a//b"));
  QVERIFY(!safeRelativePath("a/"));
  QVERIFY(!safeRelativePath("file.txt:hidden"));
  QVERIFY(!safeRelativePath(std::string(kMaxPathLength + 1, 'a')));
}

void FileTransferTests::safePath_rejectsNamesWindowsWouldMangle()
{
  QVERIFY(!safeRelativePath("con"));
  QVERIFY(!safeRelativePath("folder/NUL.txt"));
  QVERIFY(!safeRelativePath("COM1"));
  QVERIFY(!safeRelativePath("lpt9.log"));
  QVERIFY(!safeRelativePath("name."));
  QVERIFY(!safeRelativePath("name "));
  QVERIFY(!safeRelativePath("tab\there"));
  QVERIFY(!safeRelativePath("what?.txt"));

  // only the exact device names are reserved
  QVERIFY(safeRelativePath("console.txt"));
  QVERIFY(safeRelativePath("COM10"));
}

void FileTransferTests::collect_walksFolders()
{
  QTemporaryDir dir;
  const auto base = root(dir);
  writeFile(base / "single.txt", "one");
  writeFile(base / "Photos" / "a.jpg", "aa");
  writeFile(base / "Photos" / "2026" / "b.jpg", "bbb");

  auto files = collectFiles({base / "single.txt", base / "Photos", base / "missing.txt"});
  std::ranges::sort(files, {}, &FileEntry::relativePath);

  QCOMPARE(files.size(), size_t{3});
  QCOMPARE(files[0].relativePath, std::string("Photos/2026/b.jpg"));
  QCOMPARE(files[0].size, uint64_t{3});
  QCOMPARE(files[1].relativePath, std::string("Photos/a.jpg"));
  QCOMPARE(files[2].relativePath, std::string("single.txt"));
}

void FileTransferTests::roundTrip_copiesEverything()
{
  QTemporaryDir from;
  QTemporaryDir to;
  const auto source = root(from);
  const auto big = pattern(kChunkSize * 5 + 123);
  writeFile(source / "big.bin", big);
  writeFile(source / "empty.txt", "");
  writeFile(source / "Folder" / "inner" / "note.txt", "hello");

  FileSender sender(collectFiles({source / "big.bin", source / "empty.txt", source / "Folder"}));
  FileReceiver receiver(root(to), UINT64_MAX);
  std::string error;
  QVERIFY2(transfer(sender, receiver, error), error.c_str());

  const auto destination = root(to);
  QVERIFY(readFile(destination / "big.bin") == big);
  QVERIFY(fs::exists(destination / "empty.txt"));
  QCOMPARE(fs::file_size(destination / "empty.txt"), uintmax_t{0});
  QCOMPARE(readFile(destination / "Folder" / "inner" / "note.txt"), std::string("hello"));

  // nothing half written is left behind, and the top level items are what a paste would show
  for (const auto &entry : fs::recursive_directory_iterator(destination))
    QVERIFY(entry.path().extension() != ".part");
  QCOMPARE(receiver.topLevelItems().size(), size_t{3});
}

void FileTransferTests::sender_waitsForAcknowledgement()
{
  QTemporaryDir dir;
  writeFile(root(dir) / "big.bin", pattern(kChunkSize * (kWindow + 10)));
  FileSender sender(collectFiles({root(dir) / "big.bin"}));

  std::string error;
  uint32_t sent = 0;
  while (sender.next(error))
    ++sent;
  QVERIFY(error.empty());
  QCOMPARE(sent, kWindow);

  // acknowledging lets exactly that many more through
  sender.acknowledge(4);
  sent = 0;
  while (sender.next(error))
    ++sent;
  QCOMPARE(sent, uint32_t{4});
}

void FileTransferTests::sender_reportsShrunkFile()
{
  QTemporaryDir dir;
  const auto path = root(dir) / "shrinks.bin";
  writeFile(path, pattern(kChunkSize * 3));
  FileSender sender(collectFiles({path}));
  writeFile(path, "short now");

  std::string error;
  while (sender.next(error)) {
    // keep going until it notices
  }
  QVERIFY(error.find("changed while it was being sent") != std::string::npos);
}

void FileTransferTests::receiver_neverOverwrites()
{
  QTemporaryDir from;
  QTemporaryDir to;
  writeFile(root(from) / "a.txt", "new");
  writeFile(root(from) / "Photos" / "p.jpg", "new photo");
  writeFile(root(to) / "a.txt", "old");
  writeFile(root(to) / "Photos" / "p.jpg", "old photo");

  FileSender sender(collectFiles({root(from) / "a.txt", root(from) / "Photos"}));
  FileReceiver receiver(root(to), UINT64_MAX);
  std::string error;
  QVERIFY2(transfer(sender, receiver, error), error.c_str());

  QCOMPARE(readFile(root(to) / "a.txt"), std::string("old"));
  QCOMPARE(readFile(root(to) / "a (2).txt"), std::string("new"));
  QCOMPARE(readFile(root(to) / "Photos" / "p.jpg"), std::string("old photo"));
  QCOMPARE(readFile(root(to) / "Photos (2)" / "p.jpg"), std::string("new photo"));
}

void FileTransferTests::receiver_enforcesLimit()
{
  QTemporaryDir to;
  FileReceiver receiver(root(to), 100);
  std::string error;
  QVERIFY(receiver.beginFile("a.bin", 60, error));
  QVERIFY(receiver.write(std::string(60, 'x'), error));
  QVERIFY(!receiver.beginFile("b.bin", 41, error));
  QVERIFY(error.find("limit") != std::string::npos);
}

void FileTransferTests::receiver_refusesMalformedStreams()
{
  QTemporaryDir to;
  std::string error;

  FileReceiver noHeader(root(to), UINT64_MAX);
  QVERIFY(!noHeader.write("data", error));

  FileReceiver tooMuch(root(to), UINT64_MAX);
  QVERIFY(tooMuch.beginFile("a.bin", 3, error));
  QVERIFY(!tooMuch.write("four", error));

  FileReceiver cutShort(root(to), UINT64_MAX);
  QVERIFY(cutShort.beginFile("b.bin", 10, error));
  QVERIFY(cutShort.write("12345", error));
  QVERIFY(!cutShort.finish(error));

  FileReceiver unsafe(root(to), UINT64_MAX);
  QVERIFY(!unsafe.beginFile("../escape.txt", 1, error));
  QVERIFY(!fs::exists(root(to).parent_path() / "escape.txt"));
}

void FileTransferTests::receiver_cancelRemovesPartialFile()
{
  QTemporaryDir to;
  {
    FileReceiver receiver(root(to), UINT64_MAX);
    std::string error;
    QVERIFY(receiver.beginFile("a.bin", 10, error));
    QVERIFY(receiver.write("12345", error));
    receiver.cancel();
  }
  QVERIFY(fs::is_empty(root(to)));
}

QTEST_MAIN(FileTransferTests)
