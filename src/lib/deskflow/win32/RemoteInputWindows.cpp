/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/RemoteInput.h"

#include "base/DirectionTypes.h"
#include "base/Log.h"

#include <Windows.h>

#include <array>
#include <future>
#include <mutex>
#include <thread>
#include <utility>

namespace deskflow::remote::platform {

namespace {

constexpr UINT kCaptureMessage = WM_APP + 1;

// how often to repeat an edge while the cursor is held against it
constexpr ULONGLONG kEdgeRepeatMs = 150;

// more than this waiting means the main thread has stopped reading, so drop moves rather than grow forever
constexpr size_t kMaxWaiting = 1024;

std::mutex g_mutex;
std::vector<RemoteInput> g_waiting;
std::function<void()> g_wake;
std::thread g_thread;
DWORD g_threadId = 0;

// the rest is only used on the hook thread
HHOOK g_mouseHook = nullptr;
HHOOK g_keyboardHook = nullptr;
HWND g_hider = nullptr;
HCURSOR g_blankCursor = nullptr;
bool g_capturing = false;
POINT g_park{};

// where each press went, so its release goes the same way
std::array<bool, 256> g_keysSent{};
std::array<bool, 256> g_keysLocal{};
std::array<bool, 6> g_buttonsSent{};
std::array<bool, 6> g_buttonsLocal{};

Direction g_lastEdge = Direction::NoDirection;
ULONGLONG g_lastEdgeTime = 0;

void queue(const RemoteInput &input)
{
  bool wake = false;
  {
    std::scoped_lock lock(g_mutex);
    if (input.type == InputType::Move) {
      if (!g_waiting.empty() && g_waiting.back().type == InputType::Move) {
        g_waiting.back().a += input.a;
        g_waiting.back().b += input.b;
        return;
      }
      if (g_waiting.size() >= kMaxWaiting)
        return;
    }
    wake = g_waiting.empty();
    g_waiting.push_back(input);
  }
  if (wake && g_wake)
    g_wake();
}

bool anyButtonLocal()
{
  for (const auto down : g_buttonsLocal) {
    if (down)
      return true;
  }
  return false;
}

// the cursor pushed against an edge of the whole desktop, by this computer's own mouse
void checkEdge(POINT pt)
{
  const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

  using enum Direction;
  auto side = NoDirection;
  if (pt.x <= left)
    side = Left;
  else if (pt.x >= right - 1)
    side = Right;
  else if (pt.y <= top)
    side = Top;
  else if (pt.y >= bottom - 1)
    side = Bottom;

  // dragging a window against an edge snaps it, so leave that alone
  if (side == NoDirection || anyButtonLocal()) {
    g_lastEdge = NoDirection;
    return;
  }

  const auto now = GetTickCount64();
  if (side == g_lastEdge && now - g_lastEdgeTime < kEdgeRepeatMs)
    return;
  g_lastEdge = side;
  g_lastEdgeTime = now;
  queue({InputType::Edge, static_cast<int32_t>(side), pt.x, pt.y});
}

bool handleButton(size_t button, bool down)
{
  if (down) {
    if (g_capturing) {
      g_buttonsSent.at(button) = true;
      queue({InputType::Button, static_cast<int32_t>(button), 1});
      return true;
    }
    g_buttonsLocal.at(button) = true;
    return false;
  }

  if (g_buttonsSent.at(button)) {
    g_buttonsSent.at(button) = false;
    queue({InputType::Button, static_cast<int32_t>(button), 0});
    return true;
  }
  g_buttonsLocal.at(button) = false;
  return false;
}

bool handleMouse(WPARAM message, const MSLLHOOKSTRUCT &info)
{
  const auto wheel = static_cast<int16_t>(HIWORD(info.mouseData));
  const size_t xButton = HIWORD(info.mouseData) == XBUTTON1 ? 4 : 5;
  switch (message) {
  case WM_MOUSEMOVE:
    if (g_capturing) {
      // the move is kept from happening, so the cursor stays where it was parked
      if (const auto dx = info.pt.x - g_park.x, dy = info.pt.y - g_park.y; dx != 0 || dy != 0)
        queue({InputType::Move, dx, dy});
      return true;
    }
    checkEdge(info.pt);
    return false;

  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    return handleButton(1, message == WM_LBUTTONDOWN);
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    return handleButton(2, message == WM_RBUTTONDOWN);
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    return handleButton(3, message == WM_MBUTTONDOWN);
  case WM_XBUTTONDOWN:
  case WM_XBUTTONUP:
    return handleButton(xButton, message == WM_XBUTTONDOWN);

  case WM_MOUSEWHEEL:
    if (g_capturing)
      queue({InputType::Wheel, 0, wheel});
    return g_capturing;
  case WM_MOUSEHWHEEL:
    if (g_capturing)
      queue({InputType::Wheel, wheel, 0});
    return g_capturing;

  default:
    return false;
  }
}

bool handleKey(const KBDLLHOOKSTRUCT &info)
{
  const auto vk = static_cast<size_t>(info.vkCode & 0xff);
  const bool up = (info.flags & LLKHF_UP) != 0;
  int32_t flags = up ? kKeyReleased : 0;
  if (info.flags & LLKHF_EXTENDED)
    flags |= kKeyExtended;
  const RemoteInput input{InputType::Key, static_cast<int32_t>(info.vkCode), static_cast<int32_t>(info.scanCode), flags};

  if (!up) {
    // a key held down from before capturing keeps repeating here
    if (g_capturing && !g_keysLocal.at(vk)) {
      g_keysSent.at(vk) = true;
      queue(input);
      return true;
    }
    g_keysLocal.at(vk) = true;
    return false;
  }

  if (g_keysSent.at(vk)) {
    g_keysSent.at(vk) = false;
    queue(input);
    return true;
  }
  g_keysLocal.at(vk) = false;
  return false;
}

LRESULT CALLBACK mouseHook(int code, WPARAM wParam, LPARAM lParam)
{
  // input played back by Deskflow itself is left alone
  if (const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
      code == HC_ACTION && !(info->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) && handleMouse(wParam, *info))
    return 1;
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK keyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
  if (const auto *info = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
      code == HC_ACTION && !(info->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED)) && handleKey(*info))
    return 1;
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

HCURSOR createBlankCursor()
{
  const int w = GetSystemMetrics(SM_CXCURSOR);
  const int h = GetSystemMetrics(SM_CYCURSOR);
  std::vector<BYTE> andMask(static_cast<size_t>(w * h / 8), 0xff);
  std::vector<BYTE> xorMask(andMask.size(), 0x00);
  return CreateCursor(GetModuleHandleW(nullptr), 0, 0, w, h, andMask.data(), xorMask.data());
}

// a one pixel window under the parked cursor, so the cursor isn't seen while the input goes elsewhere
HWND createHider()
{
  g_blankCursor = createBlankCursor();

  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.hCursor = g_blankCursor;
  windowClass.lpszClassName = L"DeskflowPlusRemoteInputHider";
  RegisterClassExW(&windowClass);

  const auto hider = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, windowClass.lpszClassName, L"", WS_POPUP,
      0, 0, 1, 1, nullptr, nullptr, windowClass.hInstance, nullptr
  );
  if (hider != nullptr)
    SetLayeredWindowAttributes(hider, 0, 1, LWA_ALPHA);
  return hider;
}

void capture(bool on)
{
  if (on == g_capturing)
    return;

  if (on) {
    // park in the middle of the monitor, away from the edges, so moves in every direction can be measured
    POINT pt{};
    GetCursorPos(&pt);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &monitor);
    const auto x = (monitor.rcMonitor.left + monitor.rcMonitor.right) / 2;
    const auto y = (monitor.rcMonitor.top + monitor.rcMonitor.bottom) / 2;
    if (g_hider != nullptr)
      SetWindowPos(g_hider, HWND_TOPMOST, x, y, 1, 1, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetCursorPos(x, y);
    GetCursorPos(&g_park);
    LOG_DEBUG("capturing this computer's mouse and keyboard, cursor parked at %d,%d", g_park.x, g_park.y);
  } else {
    if (g_hider != nullptr)
      ShowWindow(g_hider, SW_HIDE);
    LOG_DEBUG("stopped capturing this computer's mouse and keyboard");
  }
  g_capturing = on;
  g_lastEdge = Direction::NoDirection;
}

void threadMain(std::promise<bool> &started)
{
  // make the message queue before anyone can post to it
  MSG msg;
  PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

  g_hider = createHider();
  g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
  g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, GetModuleHandleW(nullptr), 0);
  const bool hooked = g_mouseHook != nullptr && g_keyboardHook != nullptr;
  started.set_value(hooked);

  while (hooked && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.hwnd == nullptr && msg.message == kCaptureMessage) {
      capture(msg.wParam != 0);
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  capture(false);
  if (g_mouseHook != nullptr)
    UnhookWindowsHookEx(g_mouseHook);
  if (g_keyboardHook != nullptr)
    UnhookWindowsHookEx(g_keyboardHook);
  g_mouseHook = nullptr;
  g_keyboardHook = nullptr;
  if (g_hider != nullptr)
    DestroyWindow(g_hider);
  g_hider = nullptr;
  if (g_blankCursor != nullptr)
    DestroyCursor(g_blankCursor);
  g_blankCursor = nullptr;
  g_keysSent = {};
  g_keysLocal = {};
  g_buttonsSent = {};
  g_buttonsLocal = {};
}

} // namespace

bool startWatching(std::function<void()> wake)
{
  if (g_thread.joinable())
    return true;

  g_wake = std::move(wake);
  std::promise<bool> started;
  auto result = started.get_future();
  g_thread = std::thread([&started] { threadMain(started); });
  g_threadId = GetThreadId(g_thread.native_handle());
  if (result.get())
    return true;

  LOG_WARN("unable to watch this computer's mouse and keyboard, error %lu", GetLastError());
  stopWatching();
  return false;
}

void stopWatching()
{
  if (g_thread.joinable()) {
    PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
    g_thread.join();
  }
  g_threadId = 0;
  g_wake = nullptr;
  std::scoped_lock lock(g_mutex);
  g_waiting.clear();
}

void setCapture(bool capture)
{
  if (g_threadId != 0)
    PostThreadMessageW(g_threadId, kCaptureMessage, capture ? 1 : 0, 0);
}

std::vector<RemoteInput> takeInput()
{
  std::scoped_lock lock(g_mutex);
  return std::exchange(g_waiting, {});
}

void playBack(const RemoteInput &input)
{
  std::array<INPUT, 2> inputs{};
  UINT count = 1;
  switch (input.type) {
  case InputType::Button: {
    static constexpr std::array<std::array<DWORD, 2>, 6> kFlags{
        {{0, 0},
         {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_LEFTDOWN},
         {MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_RIGHTDOWN},
         {MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_MIDDLEDOWN},
         {MOUSEEVENTF_XUP, MOUSEEVENTF_XDOWN},
         {MOUSEEVENTF_XUP, MOUSEEVENTF_XDOWN}}
    };
    if (input.a < 1 || input.a > 5)
      return;
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = kFlags.at(static_cast<size_t>(input.a)).at(input.b != 0 ? 1 : 0);
    if (input.a >= 4)
      inputs[0].mi.mouseData = input.a == 4 ? XBUTTON1 : XBUTTON2;
    break;
  }

  case InputType::Wheel:
    count = 0;
    if (input.b != 0) {
      inputs[count].type = INPUT_MOUSE;
      inputs[count].mi.dwFlags = MOUSEEVENTF_WHEEL;
      inputs[count++].mi.mouseData = static_cast<DWORD>(input.b);
    }
    if (input.a != 0) {
      inputs[count].type = INPUT_MOUSE;
      inputs[count].mi.dwFlags = MOUSEEVENTF_HWHEEL;
      inputs[count++].mi.mouseData = static_cast<DWORD>(input.a);
    }
    break;

  case InputType::Key:
    if (input.a <= 0 || input.a > 0xfe)
      return;
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(input.a);
    inputs[0].ki.wScan = static_cast<WORD>(input.b);
    if (input.c & kKeyExtended)
      inputs[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (input.c & kKeyReleased)
      inputs[0].ki.dwFlags |= KEYEVENTF_KEYUP;
    break;

  default:
    return;
  }

  if (count > 0)
    SendInput(count, inputs.data(), sizeof(INPUT));
}

bool moveCursorBy(int32_t dx, int32_t dy, int32_t &x, int32_t &y)
{
  POINT pt{};
  if (!GetCursorPos(&pt) || !SetCursorPos(pt.x + dx, pt.y + dy) || !GetCursorPos(&pt))
    return false;
  x = pt.x;
  y = pt.y;
  return true;
}

} // namespace deskflow::remote::platform
