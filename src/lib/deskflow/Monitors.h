/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace deskflow {

//! A monitor's rectangle in the same coordinate space as IComputer::getShape()
struct MonitorRect
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;

  bool operator==(const MonitorRect &) const = default;
};

using MonitorList = std::vector<MonitorRect>;

//! Most monitors a computer may report
inline constexpr size_t kMaxMonitors = 16;

//! Flatten monitors to the kMsgDMonitors wire list: x, y, width, height per monitor
std::vector<uint32_t> encodeMonitors(const MonitorList &monitors);

//! Read monitors from the kMsgDMonitors wire list
/*!
Returns an empty list if the data is malformed: not four values per monitor,
no monitors or more than kMaxMonitors, an empty monitor, or a monitor that
reaches outside the computer shape given by \c x, \c y, \c width and \c height.
*/
MonitorList decodeMonitors(const std::vector<uint32_t> &data, int32_t x, int32_t y, int32_t width, int32_t height);

} // namespace deskflow
