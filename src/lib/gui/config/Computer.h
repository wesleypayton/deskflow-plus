/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ComputerConfig.h"

#include "common/KeyboardModifier.h"
#include "common/QSettingsProxy.h"

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QStringList>

class QSettings;
class QTextStream;
class ComputerSettingsDialog;

class Computer : public ComputerConfig
{
  friend class ComputerSettingsDialog;
  friend class ComputerSetupModel;

  friend QDataStream &operator<<(QDataStream &outStream, const Computer &computer)
  {
    return outStream << computer.name() << computer.switchCornerSize() << computer.aliases() << computer.modifiers()
                     << computer.switchCorners() << computer.fixes() << computer.isServer() << computer.m_layoutRect
                     << computer.m_monitors;
  }

  friend QDataStream &operator>>(QDataStream &inStream, Computer &computer)
  {
    return inStream >> computer.m_Name >> computer.m_SwitchCornerSize >> computer.m_Aliases >> computer.m_Modifiers >>
           computer.m_SwitchCorners >> computer.m_Fixes >> computer.m_isServer >> computer.m_layoutRect >>
           computer.m_monitors;
  }

public:
  explicit Computer(const QString &name = QString());

  [[nodiscard]] const QPixmap &pixmap() const
  {
    return m_Pixmap;
  }
  [[nodiscard]] const QString &name() const
  {
    return m_Name;
  }
  [[nodiscard]] const QStringList &aliases() const
  {
    return m_Aliases;
  }

  [[nodiscard]] bool isNull() const
  {
    return m_Name.isEmpty();
  }
  [[nodiscard]] int modifier(int m) const
  {
    return m_Modifiers[m] == static_cast<int>(KeyboardModifier::DefaultMod) ? m : m_Modifiers[m];
  }
  [[nodiscard]] const QList<int> &modifiers() const
  {
    return m_Modifiers;
  }
  [[nodiscard]] bool switchCorner(int c) const
  {
    return m_SwitchCorners[c];
  }
  [[nodiscard]] const QList<bool> &switchCorners() const
  {
    return m_SwitchCorners;
  }
  [[nodiscard]] int switchCornerSize() const
  {
    return m_SwitchCornerSize;
  }
  [[nodiscard]] bool fix(const Fix f) const
  {
    return m_Fixes[static_cast<int8_t>(f)];
  }
  [[nodiscard]] const QList<bool> &fixes() const
  {
    return m_Fixes;
  }

  void loadSettings(QSettingsProxy &settings);
  void saveSettings(QSettingsProxy &settings) const;
  [[nodiscard]] QString screensSection() const;

  [[nodiscard]] bool swapped() const
  {
    return m_Swapped;
  }

  void setName(const QString &name)
  {
    m_Name = name;
  }
  [[nodiscard]] bool isServer() const
  {
    return m_isServer;
  }
  void markAsServer()
  {
    m_isServer = true;
  }

  //! Where the computer sits in the shared layout space, or a null rect if it hasn't been placed
  [[nodiscard]] const QRect &layoutRect() const
  {
    return m_layoutRect;
  }
  [[nodiscard]] bool hasLayout() const
  {
    return !m_layoutRect.isEmpty();
  }
  void setLayoutRect(const QRect &rect)
  {
    m_layoutRect = rect;
  }
  void moveLayoutTo(const QPoint &topLeft)
  {
    m_layoutRect.moveTopLeft(topLeft);
  }

  //! Monitor rectangles relative to the top left of layoutRect(), one covering it all if unknown
  [[nodiscard]] QList<QRect> monitors() const;

  //! Set the computer's size and monitors from absolute monitor rectangles, keeping its layout position
  void setMonitors(const QList<QRect> &monitors);

  bool operator==(const Computer &computer) const;

protected:
  QStringList &aliases()
  {
    return m_Aliases;
  }
  void setModifier(const KeyboardModifier m, const int n)
  {
    m_Modifiers[static_cast<int8_t>(m)] = n;
  }
  QList<int> &modifiers()
  {
    return m_Modifiers;
  }
  void addAlias(const QString &alias)
  {
    m_Aliases.append(alias);
  }
  void setSwitchCorner(const SwitchCorner c, const bool on)
  {
    m_SwitchCorners[static_cast<int8_t>(c)] = on;
  }
  QList<bool> &switchCorners()
  {
    return m_SwitchCorners;
  }
  void setSwitchCornerSize(const int val)
  {
    m_SwitchCornerSize = val;
  }
  void setFix(const Fix f, const bool on)
  {
    m_Fixes[static_cast<int8_t>(f)] = on;
  }
  QList<bool> &fixes()
  {
    return m_Fixes;
  }
  void setSwapped(const bool on)
  {
    m_Swapped = on;
  }

private:
  QPixmap m_Pixmap = QIcon::fromTheme("video-display").pixmap(QSize(96, 96));
  QString m_Name = {};
  QStringList m_Aliases = {};
  QList<int> m_Modifiers = {0, 1, 2, 3, 4, 5, 6};
  QList<bool> m_SwitchCorners = {false, false, false, false};
  int m_SwitchCornerSize = 0;
  QList<bool> m_Fixes{false, false, false, false};
  bool m_Swapped = false;
  bool m_isServer = false;
  QRect m_layoutRect;
  QList<QRect> m_monitors;
};
