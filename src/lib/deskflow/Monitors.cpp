/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/Monitors.h"

namespace deskflow {

std::vector<uint32_t> encodeMonitors(const MonitorList &monitors)
{
  std::vector<uint32_t> data;
  data.reserve(monitors.size() * 4);
  for (const auto &monitor : monitors) {
    data.push_back(static_cast<uint32_t>(monitor.x));
    data.push_back(static_cast<uint32_t>(monitor.y));
    data.push_back(static_cast<uint32_t>(monitor.width));
    data.push_back(static_cast<uint32_t>(monitor.height));
  }
  return data;
}

MonitorList decodeMonitors(const std::vector<uint32_t> &data, int32_t x, int32_t y, int32_t width, int32_t height)
{
  if (data.empty() || data.size() % 4 != 0 || data.size() / 4 > kMaxMonitors) {
    return {};
  }

  // widen so a hostile rectangle can't overflow the bounds check
  const auto right = static_cast<int64_t>(x) + width;
  const auto bottom = static_cast<int64_t>(y) + height;

  MonitorList monitors;
  monitors.reserve(data.size() / 4);
  for (size_t i = 0; i < data.size(); i += 4) {
    const MonitorRect monitor{
        static_cast<int32_t>(data[i]), static_cast<int32_t>(data[i + 1]), static_cast<int32_t>(data[i + 2]),
        static_cast<int32_t>(data[i + 3])
    };
    if (monitor.width <= 0 || monitor.height <= 0 || monitor.x < x || monitor.y < y ||
        static_cast<int64_t>(monitor.x) + monitor.width > right ||
        static_cast<int64_t>(monitor.y) + monitor.height > bottom) {
      return {};
    }
    monitors.push_back(monitor);
  }
  return monitors;
}

} // namespace deskflow
