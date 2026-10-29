/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TransferRouterTests.h"

#include "deskflow/TransferAgent.h"
#include "io/IStream.h"
#include "server/TransferRouter.h"

#include <QTemporaryDir>

#include <cstring>
#include <deque>
#include <fstream>
#include <map>

using namespace deskflow::transfer;
using deskflow::server::TransferRouter;
namespace fs = std::filesystem;

namespace {

// what's written can be read back, like both ends of a connection in one
class LoopbackStream : public deskflow::IStream
{
public:
  void write(const void *buffer, uint32_t n) override
  {
    m_buffer.append(static_cast<const char *>(buffer), n);
  }

  uint32_t read(void *buffer, uint32_t n) override
  {
    const auto take = std::min<size_t>(n, m_buffer.size());
    if (buffer != nullptr)
      std::memcpy(buffer, m_buffer.data(), take);
    m_buffer.erase(0, take);
    return static_cast<uint32_t>(take);
  }

  void close() override
  {
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<LoopbackStream *>(this);
  }
  bool isReady() const override
  {
    return !m_buffer.empty();
  }
  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_buffer.size());
  }

private:
  std::string m_buffer;
};

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

fs::path root(const QTemporaryDir &dir)
{
  return pathFromUtf8(dir.path().toStdString());
}

// a server and computers wired together through queues, the way the network would be
struct Network
{
  struct Computer
  {
    std::unique_ptr<TransferAgent> agent;
    fs::path inbox;
    std::vector<std::vector<fs::path>> received;
    std::vector<std::string> failures;
  };

  std::map<std::string, Computer> computers;
  std::deque<std::pair<std::string, TransferMessage>> toServer;
  std::deque<std::pair<std::string, TransferMessage>> toComputers;
  TransferRouter router{[this](const std::string &to, const TransferMessage &message) {
    toComputers.emplace_back(to, message);
  }};

  void add(const std::string &name, const fs::path &inbox)
  {
    auto &computer = computers[name];
    computer.inbox = inbox;
    TransferAgent::Hooks hooks;
    hooks.send = [this, name](const TransferMessage &message) { toServer.emplace_back(name, message); };
    hooks.destination = [&computer](TransferKind) { return computer.inbox; };
    hooks.received = [&computer](TransferKind, const std::vector<fs::path> &items) {
      computer.received.push_back(items);
    };
    hooks.failed = [&computer](const std::string &reason) { computer.failures.push_back(reason); };
    computer.agent = std::make_unique<TransferAgent>(hooks);
  }

  // deliver everything until the network is quiet
  void run()
  {
    while (!toServer.empty() || !toComputers.empty()) {
      if (!toServer.empty()) {
        const auto [from, message] = toServer.front();
        toServer.pop_front();
        router.receive(from, message);
      } else {
        const auto [to, message] = toComputers.front();
        toComputers.pop_front();
        computers.at(to).agent->handle(message);
      }
    }
  }
};

TransferMessage roundTrip(const TransferMessage &message)
{
  LoopbackStream stream;
  writeTransferMessage(&stream, message);
  uint8_t code[4];
  stream.read(code, 4);
  TransferMessage read;
  if (!isTransferMessage(code) || !readTransferMessage(&stream, code, read))
    return {};
  return read;
}

} // namespace

void TransferRouterTests::wire_everyMessageRoundTrips()
{
  using enum TransferMessage::Type;
  const auto big = (uint64_t{5} << 32) + 7; // over 4 GB

  auto offer = roundTrip({Offer, 3, 12, TransferKind::Drag, big, "a.txt\nPhotos"});
  QVERIFY(offer.type == Offer && offer.id == 3 && offer.value == 12 && offer.kind == TransferKind::Drag);
  QCOMPARE(offer.size, big);
  QCOMPARE(offer.text, std::string("a.txt\nPhotos"));

  auto header = roundTrip({Header, 9, 0, {}, big, "Photos/b.jpg"});
  QVERIFY(header.type == Header && header.id == 9 && header.size == big && header.text == "Photos/b.jpg");

  const std::string binary("\0\x01\xff data", 8);
  auto data = roundTrip({Data, 9, 0, {}, 0, binary});
  QVERIFY(data.type == Data && data.text == binary);

  QVERIFY(roundTrip({Request, 9, 3, {}, 0, {}}).value == 3);
  QVERIFY(roundTrip({Start, 9, 0, TransferKind::Clipboard, 0, {}}).kind == TransferKind::Clipboard);
  QVERIFY(roundTrip({End, 9, 0, {}, 0, {}}).type == End);
  QVERIFY(roundTrip({Ack, 9, 40, {}, 0, {}}).value == 40);
  QVERIFY(roundTrip({Cancel, 9, 0, {}, 0, "disk full"}).text == "disk full");

  const uint8_t notTransfer[] = {'D', 'M', 'O', 'N'};
  QVERIFY(!isTransferMessage(notTransfer));
}

void TransferRouterTests::relay_clientToClient()
{
  QTemporaryDir source;
  QTemporaryDir inboxA;
  QTemporaryDir inboxB;
  const std::string big(kChunkSize * 40 + 17, 'x'); // well past the window, so acks must flow back
  writeFile(root(source) / "big.bin", big);
  writeFile(root(source) / "Folder" / "note.txt", "hello");

  Network network;
  network.add("laptop", root(inboxA));
  network.add("desk", root(inboxB));
  network.computers["laptop"].agent->offer(
      TransferKind::Clipboard, {root(source) / "big.bin", root(source) / "Folder"}
  );
  network.run();

  QVERIFY(network.router.start(TransferKind::Clipboard, "laptop", "desk"));
  network.run();

  auto &desk = network.computers["desk"];
  QVERIFY(desk.failures.empty());
  QCOMPARE(desk.received.size(), size_t{1});
  QCOMPARE(desk.received[0].size(), size_t{2});
  QVERIFY(readFile(root(inboxB) / "big.bin") == big);
  QCOMPARE(readFile(root(inboxB) / "Folder" / "note.txt"), std::string("hello"));
  QVERIFY(!network.router.busy());
  QVERIFY(!network.computers["laptop"].agent->busy());
}

void TransferRouterTests::relay_fromServerItself()
{
  QTemporaryDir source;
  QTemporaryDir inbox;
  writeFile(root(source) / "a.txt", "from the server");

  Network network;
  network.add("server", root(source));
  network.add("laptop", root(inbox));
  network.computers["server"].agent->offer(TransferKind::Drag, {root(source) / "a.txt"});
  network.run();

  QVERIFY(network.router.start(TransferKind::Drag, "server", "laptop"));
  network.run();
  QCOMPARE(readFile(root(inbox) / "a.txt"), std::string("from the server"));
}

void TransferRouterTests::start_needsAnOffer()
{
  Network network;
  QTemporaryDir inbox;
  network.add("laptop", root(inbox));
  network.add("desk", root(inbox));
  QVERIFY(!network.router.start(TransferKind::Clipboard, "laptop", "desk"));
  QVERIFY(!network.router.start(TransferKind::Clipboard, "laptop", "laptop"));
}

void TransferRouterTests::start_clipboardOfferOnlyOncePerComputer()
{
  QTemporaryDir source;
  QTemporaryDir inbox;
  writeFile(root(source) / "a.txt", "a");

  Network network;
  network.add("laptop", root(source));
  network.add("desk", root(inbox));
  network.computers["laptop"].agent->offer(TransferKind::Clipboard, {root(source) / "a.txt"});
  network.run();

  QVERIFY(network.router.start(TransferKind::Clipboard, "laptop", "desk"));
  network.run();
  QVERIFY(!network.router.start(TransferKind::Clipboard, "laptop", "desk"));

  // copying something else withdraws the offer
  network.computers["laptop"].agent->withdraw(TransferKind::Clipboard);
  network.run();
  QVERIFY(!network.router.latestOffer(TransferKind::Clipboard, "laptop"));

  // copying again is a new offer, which goes again
  network.computers["laptop"].agent->offer(TransferKind::Clipboard, {root(source) / "a.txt"});
  network.run();
  QVERIFY(network.router.start(TransferKind::Clipboard, "laptop", "desk"));
}

void TransferRouterTests::destinationRefusal_stopsTransfer()
{
  QTemporaryDir source;
  writeFile(root(source) / "a.txt", "a");

  Network network;
  network.add("laptop", root(source));
  network.add("desk", {}); // no destination folder, so it refuses
  network.computers["laptop"].agent->offer(TransferKind::Drag, {root(source) / "a.txt"});
  network.run();

  network.router.start(TransferKind::Drag, "laptop", "desk");
  network.run();
  QVERIFY(!network.router.busy());
  QVERIFY(!network.computers["laptop"].agent->busy());
}

void TransferRouterTests::spoofedSender_isIgnored()
{
  QTemporaryDir source;
  QTemporaryDir inbox;
  writeFile(root(source) / "a.txt", "a");

  Network network;
  network.add("laptop", root(source));
  network.add("desk", root(inbox));
  network.add("intruder", root(inbox));
  network.computers["laptop"].agent->offer(TransferKind::Drag, {root(source) / "a.txt"});
  network.run();
  QVERIFY(network.router.start(TransferKind::Drag, "laptop", "desk"));

  // before the real data flows, another computer tries to inject a file into the transfer
  network.toServer.emplace_front("intruder", TransferMessage{TransferMessage::Type::Header, 1, 0, {}, 4, "evil.exe"});
  network.run();

  QVERIFY(!fs::exists(root(inbox) / "evil.exe"));
  QCOMPARE(readFile(root(inbox) / "a.txt"), std::string("a"));
}

void TransferRouterTests::requestForUnknownOffer_isCancelled()
{
  Network network;
  QTemporaryDir inbox;
  network.add("laptop", root(inbox));

  // the server can only refer to an offer by id, and an unknown one gets nothing
  network.computers["laptop"].agent->handle({TransferMessage::Type::Request, 7, 99, {}, 0, {}});
  QCOMPARE(network.toServer.size(), size_t{1});
  QVERIFY(network.toServer.front().second.type == TransferMessage::Type::Cancel);
}

void TransferRouterTests::disconnect_cancelsTransfers()
{
  QTemporaryDir source;
  QTemporaryDir inbox;
  writeFile(root(source) / "big.bin", std::string(kChunkSize * 40, 'y'));

  Network network;
  network.add("laptop", root(source));
  network.add("desk", root(inbox));
  network.computers["laptop"].agent->offer(TransferKind::Drag, {root(source) / "big.bin"});
  network.run();
  QVERIFY(network.router.start(TransferKind::Drag, "laptop", "desk"));

  // let a few messages through, then the laptop goes away
  for (int i = 0; i < 10 && !network.toServer.empty(); ++i) {
    const auto [from, message] = network.toServer.front();
    network.toServer.pop_front();
    network.router.receive(from, message);
  }
  network.toServer.clear();
  network.router.disconnected("laptop");
  while (!network.toComputers.empty()) {
    const auto [to, message] = network.toComputers.front();
    network.toComputers.pop_front();
    if (to == "desk")
      network.computers.at(to).agent->handle(message);
  }

  QVERIFY(!network.router.busy());
  QVERIFY(!network.computers["desk"].agent->busy());
  QCOMPARE(network.computers["desk"].failures.size(), size_t{1});
  QVERIFY(fs::is_empty(root(inbox)));
  QVERIFY(!network.router.latestOffer(TransferKind::Drag, "laptop"));
}

QTEST_MAIN(TransferRouterTests)
