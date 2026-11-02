/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfig.h"

#include "common/Hotkey.h"
#include "common/Settings.h"
#include "gui/LayoutGeometry.h"
#include "gui/LocalMonitors.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QRectF>

#include <algorithm>
#include <array>

using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

const int serverDefaultIndex = 7;

ServerConfig::ServerConfig(int columns, int rows) : m_computers(columns), m_columns(columns), m_rows(rows)
{
  recall();
}

bool ServerConfig::save(const QString &fileName) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  save(file);
  file.close();

  return true;
}

bool ServerConfig::operator==(const ServerConfig &sc) const
{
  return m_computers == sc.m_computers && //
         m_Hotkeys == sc.m_Hotkeys;       //
}

void ServerConfig::save(QFile &file) const
{
  QTextStream outStream(&file);
  outStream << *this;
}

void ServerConfig::setupComputers()
{
  computers().clear();
  hotkeys().clear();

  // There must always be computer objects for each cell in the computers QList.
  // Unused computers are identified by having an empty name.
  for (int i = 0; i < m_columns * m_rows; i++)
    addComputer(Computer());
}

void ServerConfig::commit()
{
  qDebug("committing server config");

  settings().beginGroup("internalConfig");
  settings().remove("");

  settings().beginWriteArray("screens");
  for (int i = 0; i < computers().size(); i++) {
    settings().setArrayIndex(i);
    const auto &computer = computers()[i];
    computer.saveSettings(settings());
    auto computerName = Settings::value(Settings::Core::ComputerName).toString();
    if (computer.isServer() && computerName != computer.name()) {
      Settings::setValue(Settings::Core::ComputerName, computer.name());
    }
  }
  settings().endArray();

  settings().beginWriteArray("hotkeys");
  for (int i = 0; i < hotkeys().size(); i++) {
    settings().setArrayIndex(i);
    hotkeys()[i].saveSettings(settings().get());
  }
  settings().endArray();

  settings().endGroup();
}

void ServerConfig::recall()
{
  qDebug("recalling server config");

  settings().beginGroup("internalConfig");

  m_columns = Settings::value(Settings::Server::GridWidth).toInt();
  m_rows = Settings::value(Settings::Server::GridHeight).toInt();

  // we need to know the number of columns and rows before we can set up
  // ourselves
  setupComputers();

  int numComputers = settings().beginReadArray("screens");
  Q_ASSERT(numComputers <= computers().size());
  for (int i = 0; i < numComputers; i++) {
    settings().setArrayIndex(i);
    computers()[i].loadSettings(settings());
    if (getServerName() == computers()[i].name()) {
      computers()[i].markAsServer();
    }
  }
  settings().endArray();

  ensureLayout();

  int numHotkeys = settings().beginReadArray("hotkeys");
  for (int i = 0; i < numHotkeys; i++) {
    settings().setArrayIndex(i);
    Hotkey h;
    h.loadSettings(settings().get());
    hotkeys().append(h);
  }
  settings().endArray();

  settings().endGroup();
}

void ServerConfig::ensureLayout()
{
  using namespace deskflow::gui;

  const auto local = localMonitors();
  QRect localBounds;
  for (const auto &monitor : local)
    localBounds = localBounds.united(monitor);

  // settings from before the free-form layout only have grid cells, so lay the grid out once
  const bool anyPlaced = std::ranges::any_of(computers(), [](const Computer &computer) {
    return !computer.isNull() && computer.hasLayout();
  });
  if (!anyPlaced) {
    QList<QSize> sizes;
    qsizetype anchor = -1;
    for (qsizetype i = 0; i < computers().size(); ++i) {
      const auto &computer = computers().at(i);
      if (computer.isNull()) {
        sizes.append(QSize());
      } else if (computer.isServer() && !localBounds.isEmpty()) {
        sizes.append(localBounds.size());
        anchor = i;
      } else {
        sizes.append(layout::kDefaultComputerSize);
      }
    }

    const auto rects = layout::gridToLayout(sizes, m_columns, anchor);
    for (qsizetype i = 0; i < rects.size(); ++i) {
      if (!rects.at(i).isNull())
        computers()[i].setLayoutRect(rects.at(i));
    }
  }

  // the server's own monitors are always known, the clients' are kept from when they were last seen
  for (auto &computer : computers()) {
    if (!computer.isNull() && computer.isServer() && !local.isEmpty())
      computer.setMonitors(local);
  }

  for (auto &computer : computers()) {
    if (!computer.isNull())
      m_computers.placeInLayout(computer);
  }
}

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config)
{
  outStream << "section: layout" << Qt::endl;

  // the server rejects overlapping computers, so leave out any that would stop it starting
  QList<QRectF> written;
  for (const auto &computer : config.computers()) {
    if (computer.isNull() || !computer.hasLayout())
      continue;

    const auto &rect = computer.layoutRect();
    if (deskflow::gui::layout::overlapsAny(QRectF(rect), written)) {
      qWarning() << "leaving" << computer.name() << "out of the layout because it overlaps another computer";
      continue;
    }
    written.append(QRectF(rect));

    outStream << "\t" << computer.name() << " = " << rect.x() << "," << rect.y() << "," << rect.width() << ","
              << rect.height() << Qt::endl;
  }

  outStream << "end" << Qt::endl << Qt::endl;

  outStream << "section: options" << Qt::endl;

  for (const Hotkey &hotkey : config.hotkeys())
    outStream << hotkey;

  outStream << "end" << Qt::endl << Qt::endl;

  return outStream;
}

int ServerConfig::numComputers() const
{
  int rval = 0;

  for (const Computer &s : computers()) {
    if (!s.isNull())
      rval++;
  }

  return rval;
}

QString ServerConfig::getServerName() const
{
  return Settings::value(Settings::Core::ComputerName).toString();
}

void ServerConfig::updateServerName()
{
  for (auto &computer : computers()) {
    if (computer.isServer()) {
      computer.setName(Settings::value(Settings::Core::ComputerName).toString());
      break;
    }
  }
}

QString ServerConfig::configFile() const
{
  return Settings::value(Settings::Server::ExternalConfigFile).toString();
}

bool ServerConfig::useExternalConfig() const
{
  return Settings::value(Settings::Server::ExternalConfig).toBool();
}

bool ServerConfig::isFull() const
{
  bool isFull = true;

  for (const auto &computer : computers()) {
    if (computer.isNull()) {
      isFull = false;
      break;
    }
  }

  return isFull;
}

bool ServerConfig::computerExists(const QString &computerName) const
{
  bool isExists = false;

  for (const auto &computer : computers()) {
    if (!computer.isNull() && computer.name() == computerName) {
      isExists = true;
      break;
    }
  }

  return isExists;
}

void ServerConfig::addClient(const QString &clientName)
{
  int serverIndex = -1;
  const auto computerName = Settings::value(Settings::Core::ComputerName).toString();

  if (findComputerName(computerName, serverIndex)) {
    m_computers[serverIndex].markAsServer();
  } else {
    fixNoServer(computerName, serverIndex);
  }

  m_computers.addComputerByPriority(Computer(clientName));
}

bool ServerConfig::updateComputerMonitors(const QString &computerName, const QList<QRect> &monitors)
{
  if (monitors.isEmpty())
    return false;

  for (auto &computer : computers()) {
    // the server's own monitors are read locally
    if (computer.isNull() || computer.isServer() || computer.name() != computerName)
      continue;

    Computer updated(computer);
    updated.setMonitors(monitors);
    if (updated == computer)
      return false;

    // grow or shrink away from the neighbours: keep each corner in turn and take the first that fits
    QList<QRectF> others;
    for (const auto &other : std::as_const(m_computers)) {
      if (&other != &computer && !other.isNull() && other.hasLayout())
        others.append(QRectF(other.layoutRect()));
    }
    const auto old = computer.layoutRect();
    const auto size = updated.layoutRect().size();
    const std::array corners = {
        old.topLeft(), QPoint(old.x() + old.width() - size.width(), old.y()),
        QPoint(old.x(), old.y() + old.height() - size.height()),
        QPoint(old.x() + old.width() - size.width(), old.y() + old.height() - size.height())
    };
    for (const auto &corner : corners) {
      if (!deskflow::gui::layout::overlapsAny(QRectF(QRect(corner, size)), others)) {
        updated.moveLayoutTo(corner);
        break;
      }
    }

    computer = updated;
    m_computers.placeInLayout(computer);
    return true;
  }
  return false;
}

void ServerConfig::setConfigFile(const QString &configFile) const
{
  Settings::setValue(Settings::Server::ExternalConfigFile, configFile);
}

void ServerConfig::setUseExternalConfig(bool useExternalConfig) const
{
  Settings::setValue(Settings::Server::ExternalConfig, useExternalConfig);
}

bool ServerConfig::findComputerName(const QString &name, int &index)
{
  bool found = false;
  for (int i = 0; i < computers().size(); i++) {
    if (!computers()[i].isNull() && computers()[i].name().compare(name) == 0) {
      index = i;
      found = true;
      break;
    }
  }
  return found;
}

bool ServerConfig::fixNoServer(const QString &name, int &index)
{
  bool fixed = false;
  if (computers()[serverDefaultIndex].isNull()) {
    m_computers[serverDefaultIndex].setName(name);
    m_computers[serverDefaultIndex].markAsServer();
    index = serverDefaultIndex;
    fixed = true;
  }

  return fixed;
}

QSettingsProxy &ServerConfig::settings()
{
  return Settings::proxy();
}
