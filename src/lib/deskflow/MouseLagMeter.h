/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

namespace deskflow {

/**
 * @brief Notices when the cursor stutters, and which step of getting a mouse move across was late.
 *
 * A stutter looks the same on screen wherever it comes from, so each step is timed where it happens:
 * the computer with the mouse handing its moves on, the moves crossing the network, and the other
 * computer moving its cursor. What was seen is collected and handed back every few seconds, so the
 * log gets a line per bad patch rather than one per move.
 */
class MouseLagMeter
{
public:
  using Clock = std::chrono::steady_clock;

  //! Moves this far apart, then several at once, means they were held up on the way rather than the hand pausing
  static constexpr std::chrono::milliseconds kLateGap{40};
  //! A longer wait than this is the mouse lying still
  static constexpr std::chrono::milliseconds kIdleGap{2000};
  //! How many moves have to arrive together to count as having been held up
  static constexpr int kLateMoves = 4;
  //! A single step taking this long is felt
  static constexpr std::chrono::milliseconds kSlowStep{25};
  //! How often what was seen is reported
  static constexpr std::chrono::seconds kReportEvery{5};

  struct Report
  {
    //! Times a run of moves arrived together after a gap
    int lateBursts = 0;
    int longestGapMs = 0;
    int mostMovesAtOnce = 0;
    //! Times a timed step (handing a move on, or moving the cursor) was slow
    int slowSteps = 0;
    int slowestStepMs = 0;
  };

  //! A move arrived from the other computer
  void moveArrived(Clock::time_point now)
  {
    if (m_movesInBatch++ == 0 && m_lastMove != Clock::time_point{})
      m_gapBeforeBatch = now - m_lastMove;
    m_lastMove = now;
  }

  //! Everything that had arrived has been read
  void batchEnded()
  {
    if (m_movesInBatch >= kLateMoves && m_gapBeforeBatch >= kLateGap && m_gapBeforeBatch < kIdleGap) {
      ++m_report.lateBursts;
      // (std::max) rather than std::max, Windows.h defines a max macro
      m_report.longestGapMs = (std::max)(m_report.longestGapMs, toMs(m_gapBeforeBatch));
      m_report.mostMovesAtOnce = (std::max)(m_report.mostMovesAtOnce, m_movesInBatch);
    }
    m_movesInBatch = 0;
    m_gapBeforeBatch = {};
  }

  //! A step that should be instant took this long
  void stepTook(Clock::duration took)
  {
    if (took >= kSlowStep) {
      ++m_report.slowSteps;
      m_report.slowestStepMs = (std::max)(m_report.slowestStepMs, toMs(took));
    }
  }

  //! What was seen since the last report, if anything was and it's time to say so
  std::optional<Report> takeReport(Clock::time_point now)
  {
    if (m_report.lateBursts == 0 && m_report.slowSteps == 0)
      return std::nullopt;
    if (m_lastReport != Clock::time_point{} && now - m_lastReport < kReportEvery)
      return std::nullopt;
    m_lastReport = now;
    const auto report = m_report;
    m_report = {};
    return report;
  }

private:
  static int toMs(Clock::duration time)
  {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
  }

  Clock::time_point m_lastMove;
  Clock::time_point m_lastReport;
  Clock::duration m_gapBeforeBatch{};
  int m_movesInBatch = 0;
  Report m_report;
};

} // namespace deskflow
