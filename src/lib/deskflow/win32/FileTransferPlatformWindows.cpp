/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"

#include "base/Log.h"

#include <Windows.h>
#include <shlobj.h>

#include <chrono>
#include <cstring>
#include <string>

namespace deskflow::transfer::platform {

namespace {

// clipboard inboxes older than this are cleared out when a new one is made
constexpr auto kInboxLifetime = std::chrono::hours(24);

// another program may hold the clipboard for a moment
bool openClipboard(HWND owner)
{
  for (int attempt = 0; attempt < 10; ++attempt) {
    if (OpenClipboard(owner))
      return true;
    Sleep(20);
  }
  LOG_WARN("couldn't open the clipboard for file transfer");
  return false;
}

// setting clipboard data needs a window to own it
HWND clipboardWindow()
{
  static HWND window =
      CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
  return window;
}

HGLOBAL copyToGlobal(const void *data, size_t size)
{
  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
  if (global == nullptr)
    return nullptr;
  if (size > 0)
    std::memcpy(GlobalLock(global), data, size);
  GlobalUnlock(global);
  return global;
}

} // namespace

std::vector<std::filesystem::path> filesOnClipboard()
{
  std::vector<std::filesystem::path> items;
  if (!IsClipboardFormatAvailable(CF_HDROP) || !openClipboard(nullptr))
    return items;

  if (auto *drop = static_cast<HDROP>(GetClipboardData(CF_HDROP)); drop != nullptr) {
    const auto count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
      std::wstring path(DragQueryFileW(drop, i, nullptr, 0) + 1, L'\0');
      path.resize(DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size())));
      items.emplace_back(path);
    }
  }

  CloseClipboard();
  return items;
}

bool putFilesOnClipboard(const std::vector<std::filesystem::path> &items)
{
  if (items.empty() || !openClipboard(clipboardWindow()))
    return false;

  // keep any text that's there; the rest of the clipboard is replaced
  std::wstring text;
  if (const auto handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
    if (const auto *chars = static_cast<const wchar_t *>(GlobalLock(handle)); chars != nullptr) {
      text = chars;
      GlobalUnlock(handle);
    }
  }

  EmptyClipboard();

  // a DROPFILES header followed by the paths, each null terminated, with an extra null at the end
  std::wstring paths;
  for (const auto &item : items) {
    paths += item.native();
    paths += L'\0';
  }
  paths += L'\0';

  DROPFILES header{};
  header.pFiles = sizeof(DROPFILES);
  header.fWide = TRUE;
  std::string drop(sizeof(DROPFILES) + paths.size() * sizeof(wchar_t), '\0');
  std::memcpy(drop.data(), &header, sizeof(header));
  std::memcpy(drop.data() + sizeof(header), paths.data(), paths.size() * sizeof(wchar_t));

  bool ok = SetClipboardData(CF_HDROP, copyToGlobal(drop.data(), drop.size())) != nullptr;

  // pasting should copy, not move, the received files
  const DWORD effect = DROPEFFECT_COPY;
  SetClipboardData(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT), copyToGlobal(&effect, sizeof(effect)));

  if (!text.empty())
    SetClipboardData(CF_UNICODETEXT, copyToGlobal(text.c_str(), (text.size() + 1) * sizeof(wchar_t)));

  // the same marker MSWindowsClipboard uses, so the clipboard sync doesn't treat this as a new copy
  const char marker = 0;
  SetClipboardData(RegisterClipboardFormatW(L"Deskflow Ownership"), copyToGlobal(&marker, sizeof(marker)));

  CloseClipboard();
  return ok;
}

std::filesystem::path clipboardInbox()
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const auto base = fs::temp_directory_path(ec) / "Deskflow" / "Clipboard";
  if (ec)
    return {};

  // clear out what earlier pastes left behind
  const auto now = fs::file_time_type::clock::now();
  for (const auto &entry : fs::directory_iterator(base, ec)) {
    if (now - entry.last_write_time(ec) > kInboxLifetime)
      fs::remove_all(entry.path(), ec);
  }

  const auto stamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
          .count();
  const auto inbox = base / std::to_string(stamp);
  fs::create_directories(inbox, ec);
  return ec ? fs::path() : inbox;
}

} // namespace deskflow::transfer::platform
