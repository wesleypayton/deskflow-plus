/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigTests.h"

#include "server/Config.h"

class OnlySystemFilter : public InputFilter::Condition
{
public:
  Condition *clone() const override
  {
    return new OnlySystemFilter();
  }
  std::string format() const override
  {
    return "";
  }

  InputFilter::FilterStatus match(const Event &ev) override
  {
    return ev.getType() == EventTypes::System ? InputFilter::FilterStatus::Activate
                                              : InputFilter::FilterStatus::NoMatch;
  }
};

using namespace deskflow::server;
using namespace std::string_literals;

void ServerConfigTests::equalityCheck()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a != b);

  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);

  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a.addOption(std::string(), kOptionClipboardSharing, 1));
  QVERIFY(b.addOption(std::string(), kOptionClipboardSharing, 1));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  b.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  /* TODO Fix linking to the proper libs
  NetworkAddress addr1("localhost", 8080);
  addr1.resolve();
  NetworkAddress addr2("localhost", 8080);
  addr2.resolve();
  a.setDeskflowAddress(addr1);
  b.setDeskflowAddress(addr2);
  */
  QVERIFY(a == b);
}

void ServerConfigTests::equalityCheck_diff_options()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 0));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_alias()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  QVERIFY(a != b);

  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasB"));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_filters()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a != b);
}

// TODO FIX
/*
void ServerConfigTests::equalityCheck_diff_address()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  a.setDeskflowAddress(NetworkAddress(8000));
  b.setDeskflowAddress(NetworkAddress(9000));
  QVERIFY(a != b);
}
*/

void ServerConfigTests::equalityCheck_diff_neighbours1()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);
  QVERIFY(b != a);
}

void ServerConfigTests::equalityCheck_diff_neighbours2()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.25f, "computerB", 0.25f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_neighbours3()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerC", 0.5f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_layout()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(a.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(a != b);

  QVERIFY(b.setLayoutRect("computerA", {0, 0, 1920, 1200}));
  QVERIFY(a != b);

  QVERIFY(b.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(a == b);
}

void ServerConfigTests::layout_invalidRect()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(!config.setLayoutRect("unknown", {0, 0, 1920, 1080}));
  QVERIFY(!config.setLayoutRect("computerA", {0, 0, 0, 1080}));
  QVERIFY(!config.setLayoutRect("computerA", {0, 0, 1920, -1}));
  QVERIFY(!config.hasLayout());
}

void ServerConfigTests::layout_sideBySide()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {1920, 0, 1920, 1080}));
  QVERIFY(config.applyLayout());

  float position = 0.0f;
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.25f, &position), "computerB"s);
  QCOMPARE(position, 0.25f);
  QCOMPARE(config.getNeighbor("computerB", Direction::Left, 0.75f, &position), "computerA"s);
  QCOMPARE(position, 0.75f);

  QVERIFY(!config.hasNeighbor("computerA", Direction::Left));
  QVERIFY(!config.hasNeighbor("computerA", Direction::Top));
  QVERIFY(!config.hasNeighbor("computerA", Direction::Bottom));
  QVERIFY(!config.hasNeighbor("computerB", Direction::Right));
}

void ServerConfigTests::layout_partialSpan()
{
  // B is shorter and sits lower, so only the bottom half of A's right edge touches it
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {1920, 540, 1440, 900}));
  QVERIFY(config.applyLayout());

  float position = 0.0f;
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.25f, nullptr), ""s);

  // y = 810 on A is 270 pixels into B
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.75f, &position), "computerB"s);
  QCOMPARE(position, 0.3f);

  // y = 1260 on B is below A
  QCOMPARE(config.getNeighbor("computerB", Direction::Left, 0.8f, nullptr), ""s);

  // y = 630 on B is 630 pixels into A
  QCOMPARE(config.getNeighbor("computerB", Direction::Left, 0.1f, &position), "computerA"s);
  QCOMPARE(position, 630.0f / 1080.0f);
}

void ServerConfigTests::layout_twoNeighboursOnOneEdge()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.addComputer("computerC"));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 2560, 1440}));
  QVERIFY(config.setLayoutRect("computerB", {2560, 0, 1920, 720}));
  QVERIFY(config.setLayoutRect("computerC", {2560, 720, 1920, 720}));
  QVERIFY(config.applyLayout());

  float position = 0.0f;
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.25f, &position), "computerB"s);
  QCOMPARE(position, 0.5f);
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.75f, &position), "computerC"s);
  QCOMPARE(position, 0.5f);

  QCOMPARE(config.getNeighbor("computerB", Direction::Bottom, 0.5f, nullptr), "computerC"s);
  QCOMPARE(config.getNeighbor("computerC", Direction::Top, 0.5f, nullptr), "computerB"s);
}

void ServerConfigTests::layout_stacked()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {-480, -1440, 2560, 1440}));
  QVERIFY(config.applyLayout());

  // x = 960 on A is 1440 pixels into B
  float position = 0.0f;
  QCOMPARE(config.getNeighbor("computerA", Direction::Top, 0.5f, &position), "computerB"s);
  QCOMPARE(position, 1440.0f / 2560.0f);

  // x = 1952 on B is past the right edge of A
  QCOMPARE(config.getNeighbor("computerB", Direction::Bottom, 0.95f, nullptr), ""s);
  QVERIFY(!config.hasNeighbor("computerA", Direction::Left));
  QVERIFY(!config.hasNeighbor("computerA", Direction::Right));
}

void ServerConfigTests::layout_cornerOnly()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {1920, 1080, 1920, 1080}));
  QVERIFY(config.applyLayout());

  QVERIFY(!config.hasNeighbor("computerA", Direction::Right));
  QVERIFY(!config.hasNeighbor("computerA", Direction::Bottom));
  QVERIFY(!config.hasNeighbor("computerB", Direction::Left));
  QVERIFY(!config.hasNeighbor("computerB", Direction::Top));
}

void ServerConfigTests::layout_overlap()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.connect("computerA", Direction::Left, 0.0f, 1.0f, "computerB", 0.0f, 1.0f));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {1919, 0, 1920, 1080}));
  QVERIFY(!config.applyLayout());

  // links are left alone when the layout can't be applied
  QCOMPARE(config.getNeighbor("computerA", Direction::Left, 0.5f, nullptr), "computerB"s);
}

void ServerConfigTests::layout_replacesLinks()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));
  QVERIFY(config.connect("computerA", Direction::Left, 0.0f, 1.0f, "computerB", 0.0f, 1.0f));
  QVERIFY(config.setLayoutRect("computerA", {0, 0, 1920, 1080}));
  QVERIFY(config.setLayoutRect("computerB", {1920, 0, 1920, 1080}));
  QVERIFY(config.applyLayout());

  QVERIFY(!config.hasNeighbor("computerA", Direction::Left));
  QCOMPARE(config.getNeighbor("computerA", Direction::Right, 0.5f, nullptr), "computerB"s);
}

QTEST_MAIN(ServerConfigTests)
