/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"

#include "base/Log.h"

#include <Windows.h>
#include <exdisp.h>
#include <ole2.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <future>
#include <mutex>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace deskflow::transfer::platform {

namespace {

constexpr UINT kShowMessage = WM_APP + 1;
constexpr UINT kHideMessage = WM_APP + 2;

// big enough that the drag loop notices it under a cursor pushed against an edge
constexpr int kProbeSize = 64;

std::vector<std::filesystem::path> filesFromDrop(HDROP drop)
{
  std::vector<std::filesystem::path> files;
  const auto count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
  for (UINT i = 0; i < count; ++i) {
    std::wstring path(DragQueryFileW(drop, i, nullptr, 0) + 1, L'\0');
    path.resize(DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size())));
    files.emplace_back(path);
  }
  return files;
}

// a cursor pinned against an edge doesn't move, so nothing looks at it again until it does: nudge it in and
// back, which both the drag loop and Deskflow's own edge check see as movement
void nudgeCursor(DWORD firstFlags = 0)
{
  std::array<INPUT, 3> inputs{};
  for (auto &input : inputs)
    input.type = INPUT_MOUSE;
  inputs[0].mi.dwFlags = firstFlags;
  inputs[1].mi.dx = -1;
  inputs[1].mi.dwFlags = MOUSEEVENTF_MOVE;
  inputs[2].mi.dx = 1;
  inputs[2].mi.dwFlags = MOUSEEVENTF_MOVE;
  const auto first = firstFlags == 0 ? 1 : 0;
  SendInput(static_cast<UINT>(inputs.size() - first), inputs.data() + first, sizeof(INPUT));
}

//! Records what a drag carries and accepts nothing, so the source never deletes or moves anything
class ProbeTarget : public IDropTarget
{
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
  {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
      *object = static_cast<IDropTarget *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  // the target lives as long as the probe, so the count is only for COM's benefit
  ULONG STDMETHODCALLTYPE AddRef() override
  {
    return ++m_refs;
  }
  ULONG STDMETHODCALLTYPE Release() override
  {
    return --m_refs;
  }

  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *data, DWORD, POINTL, DWORD *effect) override
  {
    FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    std::vector<std::filesystem::path> files;
    if (SUCCEEDED(data->GetData(&format, &medium))) {
      if (auto *drop = static_cast<HDROP>(GlobalLock(medium.hGlobal)); drop != nullptr) {
        files = filesFromDrop(drop);
        GlobalUnlock(medium.hGlobal);
      }
      ReleaseStgMedium(&medium);
    }

    LOG_DEBUG("drag probe entered by a drag of %d files", static_cast<int>(files.size()));
    {
      std::scoped_lock lock(m_mutex);
      m_files = files;
      *effect = m_files.empty() ? DROPEFFECT_NONE : DROPEFFECT_COPY;
    }

    // let the edge check see the files without waiting for the user to move
    if (!files.empty())
      nudgeCursor();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD *effect) override
  {
    std::scoped_lock lock(m_mutex);
    *effect = m_files.empty() ? DROPEFFECT_NONE : DROPEFFECT_COPY;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE DragLeave() override
  {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Drop(IDataObject *, DWORD, POINTL, DWORD *effect) override
  {
    // the files go to the other computer; here the drag just ends
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  std::vector<std::filesystem::path> files()
  {
    std::scoped_lock lock(m_mutex);
    return m_files;
  }

  void clear()
  {
    std::scoped_lock lock(m_mutex);
    m_files.clear();
  }

private:
  std::atomic<ULONG> m_refs{1};
  std::mutex m_mutex;
  std::vector<std::filesystem::path> m_files;
};

//! An almost invisible window registered as a drop target, on its own thread with its own message loop
class Probe
{
public:
  static Probe &instance()
  {
    static Probe probe;
    return probe;
  }

  void show()
  {
    if (!start())
      return;
    POINT cursor{};
    GetCursorPos(&cursor);
    PostMessageW(
        m_window, kShowMessage, static_cast<WPARAM>(static_cast<intptr_t>(cursor.x)), static_cast<LPARAM>(cursor.y)
    );
  }

  void hide()
  {
    if (m_window != nullptr)
      PostMessageW(m_window, kHideMessage, 0, 0);
    m_target.clear();
  }

  std::vector<std::filesystem::path> files()
  {
    return m_target.files();
  }

  ~Probe()
  {
    // the process is exiting; ask the thread to stop but don't wait on it
    if (m_thread.joinable()) {
      PostThreadMessageW(GetThreadId(m_thread.native_handle()), WM_QUIT, 0, 0);
      m_thread.detach();
    }
  }

private:
  Probe() = default;

  bool start()
  {
    if (m_thread.joinable())
      return m_window != nullptr;

    std::promise<HWND> ready;
    auto window = ready.get_future();
    m_thread = std::thread([this, &ready] { run(ready); });
    m_window = window.get();
    return m_window != nullptr;
  }

  void run(std::promise<HWND> &ready)
  {
    OleInitialize(nullptr);

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"DeskflowDragProbe";
    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, windowClass.lpszClassName, L"", WS_POPUP,
        0, 0, kProbeSize, kProbeSize, nullptr, nullptr, windowClass.hInstance, nullptr
    );

    // nearly invisible, as a fully transparent window isn't hit tested and so never sees the drag
    if (window != nullptr) {
      SetLayeredWindowAttributes(window, 0, 1, LWA_ALPHA);
      if (FAILED(RegisterDragDrop(window, &m_target))) {
        LOG_WARN("couldn't register the drag probe as a drop target");
        DestroyWindow(window);
        window = nullptr;
      }
    }
    ready.set_value(window);
    if (window == nullptr) {
      OleUninitialize();
      return;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    RevokeDragDrop(window);
    DestroyWindow(window);
    OleUninitialize();
  }

  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if (message == kShowMessage) {
      const auto x = static_cast<int>(static_cast<intptr_t>(wParam));
      const auto y = static_cast<int>(lParam);
      LOG_DEBUG("drag probe shown at %d,%d", x, y);
      SetWindowPos(
          window, HWND_TOPMOST, x - kProbeSize / 2, y - kProbeSize / 2, kProbeSize, kProbeSize,
          SWP_NOACTIVATE | SWP_SHOWWINDOW
      );

      // the drag loop only notices the window when the cursor moves
      nudgeCursor();
      return 0;
    }
    if (message == kHideMessage) {
      ShowWindow(window, SW_HIDE);
      return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }

  ProbeTarget m_target;
  std::thread m_thread;
  HWND m_window = nullptr;
};

std::filesystem::path knownFolder(REFKNOWNFOLDERID id)
{
  PWSTR path = nullptr;
  std::filesystem::path result;
  if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &path)))
    result = path;
  CoTaskMemFree(path);
  return result;
}

// the folder an Explorer window is showing
std::filesystem::path explorerFolder(HWND window)
{
  ComPtr<IShellWindows> windows;
  if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&windows))))
    return {};

  long count = 0;
  windows->get_Count(&count);
  for (long i = 0; i < count; ++i) {
    VARIANT index;
    VariantInit(&index);
    V_VT(&index) = VT_I4;
    V_I4(&index) = i;

    ComPtr<IDispatch> dispatch;
    if (windows->Item(index, &dispatch) != S_OK || !dispatch)
      continue;

    ComPtr<IWebBrowserApp> app;
    SHANDLE_PTR handle = 0;
    if (FAILED(dispatch.As(&app)) || FAILED(app->get_HWND(&handle)) || reinterpret_cast<HWND>(handle) != window)
      continue;

    ComPtr<IServiceProvider> services;
    ComPtr<IShellBrowser> browser;
    ComPtr<IShellView> view;
    ComPtr<IFolderView> folderView;
    ComPtr<IPersistFolder2> folder;
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(app.As(&services)) || FAILED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser))) ||
        FAILED(browser->QueryActiveShellView(&view)) || FAILED(view.As(&folderView)) ||
        FAILED(folderView->GetFolder(IID_PPV_ARGS(&folder))) || FAILED(folder->GetCurFolder(&pidl)))
      continue;

    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_FILESYSPATH, &path)))
      result = path;
    CoTaskMemFree(path);
    CoTaskMemFree(pidl);
    return result;
  }
  return {};
}

DropFolder findDropFolder()
{
  POINT cursor{};
  GetCursorPos(&cursor);
  HWND window = GetAncestor(WindowFromPoint(cursor), GA_ROOT);

  wchar_t className[64] = {};
  GetClassNameW(window, className, 64);
  const std::wstring name(className);

  if (name == L"CabinetWClass" || name == L"ExploreWClass") {
    if (auto folder = explorerFolder(window); !folder.empty())
      return {folder, false};
  }

  if (name == L"Progman" || name == L"WorkerW") {
    if (auto desktop = knownFolder(FOLDERID_Desktop); !desktop.empty())
      return {desktop, false};
  }

  // dropped on something that isn't a folder: keep the files somewhere sensible and show them
  const auto downloads = knownFolder(FOLDERID_Downloads);
  if (downloads.empty())
    return {};
  std::error_code ec;
  std::filesystem::create_directories(downloads / "Deskflow", ec);
  return {downloads / "Deskflow", true};
}

// shell calls need COM, which the caller's thread may not have set up the way they need it
template <typename Function> void withCom(Function function)
{
  std::thread([&function] {
    const bool ready = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    function();
    if (ready)
      CoUninitialize();
  }).join();
}

} // namespace

void startDragProbe()
{
  Probe::instance().show();
}

std::vector<std::filesystem::path> dragProbeFiles()
{
  return Probe::instance().files();
}

void endLocalDrag()
{
  // release over the drop target, then nudge so the edge is checked again now the button is up
  nudgeCursor(MOUSEEVENTF_LEFTUP);
}

void stopDragProbe()
{
  Probe::instance().hide();
}

DropFolder dropFolderAtCursor()
{
  DropFolder folder;
  withCom([&folder] { folder = findDropFolder(); });
  return folder;
}

void revealInFileManager(const std::vector<std::filesystem::path> &items)
{
  if (items.empty())
    return;

  withCom([&items] {
    PIDLIST_ABSOLUTE parent = ILCreateFromPathW(items.front().parent_path().c_str());
    std::vector<PIDLIST_ABSOLUTE> children;
    for (const auto &item : items) {
      if (auto *child = ILCreateFromPathW(item.c_str()); child != nullptr)
        children.push_back(child);
    }
    if (parent != nullptr) {
      SHOpenFolderAndSelectItems(
          parent, static_cast<UINT>(children.size()), const_cast<PCUITEMID_CHILD_ARRAY>(children.data()), 0
      );
    }
    for (auto *child : children)
      ILFree(child);
    ILFree(parent);
  });
}

} // namespace deskflow::transfer::platform
