/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ipc/UserShell.h"

#include "base/Log.h"
#include "deskflow/ipc/CoreIpcServer.h"

#include <QMetaObject>

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>

#include <array>
#endif

namespace deskflow::core::ipc::userShell {

namespace {

// long enough for the GUI to look at the window under the cursor, short enough not to stall a transfer for long
constexpr auto kAnswerTimeout = std::chrono::seconds(2);

std::mutex g_mutex;
std::condition_variable g_answered;
bool g_connected = false;
std::filesystem::path g_tempFolder;
std::vector<std::filesystem::path> g_dragFiles;
uint64_t g_lastRequest = 0;
std::map<uint64_t, std::optional<deskflow::shell::DropFolder>> g_requests;

bool runningAsSystem()
{
#ifdef _WIN32
  static const bool system = [] {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
      return false;
    alignas(TOKEN_USER) std::array<BYTE, sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE> buffer{};
    DWORD size = 0;
    const bool result =
        GetTokenInformation(token, TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &size) &&
        IsWellKnownSid(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, WinLocalSystemSid);
    CloseHandle(token);
    return result;
  }();
  return system;
#else
  return false;
#endif
}

} // namespace

bool actsForUser()
{
  return !runningAsSystem();
}

bool delegated()
{
  if (!runningAsSystem())
    return false;
  std::scoped_lock lock(g_mutex);
  return g_connected;
}

void send(const QString &command, const QString &args)
{
  // the socket belongs to the IPC server's thread
  auto &server = CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [&server, command, args] { server.sendToUserShell(command, args); }, Qt::QueuedConnection
  );
}

std::vector<std::filesystem::path> dragFiles()
{
  std::scoped_lock lock(g_mutex);
  return g_dragFiles;
}

void setDragFiles(std::vector<std::filesystem::path> files)
{
  std::scoped_lock lock(g_mutex);
  g_dragFiles = std::move(files);
}

std::optional<deskflow::shell::DropFolder> dropFolder()
{
  std::unique_lock lock(g_mutex);
  const auto id = ++g_lastRequest;
  g_requests[id] = std::nullopt;
  lock.unlock();

  send(QStringLiteral("shellDropFolder"), QString::number(id));

  lock.lock();
  g_answered.wait_for(lock, kAnswerTimeout, [id] { return g_requests[id].has_value() || !g_connected; });
  auto folder = g_requests[id];
  g_requests.erase(id);
  return folder;
}

std::filesystem::path tempFolder()
{
  std::scoped_lock lock(g_mutex);
  return g_tempFolder;
}

void helperConnected(const std::filesystem::path &tempFolder)
{
  {
    std::scoped_lock lock(g_mutex);
    g_connected = true;
    g_tempFolder = tempFolder;
  }
  if (runningAsSystem())
    LOG_INFO("the Deskflow Plus window will handle dragged and dropped files for the signed in user");
}

void helperDisconnected()
{
  {
    std::scoped_lock lock(g_mutex);
    g_connected = false;
    g_dragFiles.clear();
  }
  g_answered.notify_all();
}

void deliverDropFolder(uint64_t id, const deskflow::shell::DropFolder &folder)
{
  {
    std::scoped_lock lock(g_mutex);
    if (const auto request = g_requests.find(id); request != g_requests.end())
      request->second = folder;
  }
  g_answered.notify_all();
}

} // namespace deskflow::core::ipc::userShell
