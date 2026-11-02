/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "wix-custom.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Include after Windows.h
#include <MsiQuery.h>

#include <filesystem>
#include <stdio.h>
#include <string>
#include <vector>

namespace {
// Warning: DLL will crash with error code 1603 if we exceed this.
const auto kLogLineMax = 1024;

// Prefixes log messages with the app name so they're easier to find/filter.
const std::string kLogPrefix = std::string(kAppId) + " installer: ";

// Note: Resized to log line max when used.
static std::string s_logMessageBuffer; // NOSONAR - Must be mutable.
} // namespace

// This log output can be viewed by using the DebugView program.
#define MS_LOG_DEBUG(message, ...)                                                                                     \
  s_logMessageBuffer.resize(kLogLineMax);                                                                              \
  sprintf(s_logMessageBuffer.data(), message, __VA_ARGS__);                                                            \
  OutputDebugStringA((kLogPrefix + s_logMessageBuffer + "\n").c_str())

extern "C" __declspec(dllexport) UINT __stdcall CheckVCRedist(MSIHANDLE hInstall)
{
  const auto kKeyName = TEXT(kRegKey);
  const auto kValueName = TEXT("Minor");
  const auto kProperty = "VC_REDIST_VERSION_OK";

  MS_LOG_DEBUG("checking for msvc redist v%d.%d", kWindowsRuntimeMajor, kWindowsRuntimeMinor);

  HKEY hKey;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, kKeyName, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    MS_LOG_DEBUG("msvc redist registry key not found");
    return ERROR_FUNCTION_FAILED;
  }

  MS_LOG_DEBUG("msvc redist registry key found, querying minor version");

  DWORD minorVersion = 0;
  DWORD size = sizeof(DWORD);
  RegQueryValueEx(hKey, kValueName, nullptr, nullptr, (LPBYTE)&minorVersion, &size);
  RegCloseKey(hKey);

  MS_LOG_DEBUG("msvc redist minor version: %lu", minorVersion);

  if (minorVersion < kWindowsRuntimeMinor) {
    MS_LOG_DEBUG("msvc redist minor version %lu too low, expected >= %d", minorVersion, kWindowsRuntimeMinor);
    // Returning success allows the installer will show a friendly error message.
    return ERROR_SUCCESS;
  }

  MS_LOG_DEBUG("msvc redist version ok, setting: %s", kProperty);
  if (MsiSetProperty(hInstall, kProperty, "ok") != ERROR_SUCCESS) {
    MS_LOG_DEBUG("failed to set property: %s", kProperty);
    return ERROR_FUNCTION_FAILED;
  }

  MS_LOG_DEBUG("msvc redist version check successful");
  return ERROR_SUCCESS;
}

// The test builds before 1.0, numbered 1.27.0.x, were installed by a setup .exe with this upgrade code. Setup
// won't install over a higher number of itself, so 1.0's has a new code, and this removes the test build's own
// entry in Apps once the MSI has replaced its files, along with the copy of its setup Windows kept.
extern "C" __declspec(dllexport) UINT __stdcall RemoveTestBuilds(MSIHANDLE)
{
  const auto kUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  const auto kTestBuildUpgradeCode = L"{3B8E5C71-2A4F-4D96-B0E3-7C1F9A6D2E45}";

  HKEY uninstall = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kUninstallKey, 0, KEY_READ | KEY_WRITE, &uninstall) != ERROR_SUCCESS)
    return ERROR_SUCCESS;

  std::vector<std::wstring> bundles;
  for (DWORD i = 0;; ++i) {
    wchar_t name[256];
    DWORD nameLength = 256;
    if (RegEnumKeyExW(uninstall, i, name, &nameLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
      break;

    // a list of codes, each ended by a null, and the list by another
    std::vector<wchar_t> codes(1024, L'\0');
    DWORD bytes = static_cast<DWORD>((codes.size() - 2) * sizeof(wchar_t));
    if (RegGetValueW(uninstall, name, L"BundleUpgradeCode", RRF_RT_REG_MULTI_SZ, nullptr, codes.data(), &bytes) !=
        ERROR_SUCCESS)
      continue;
    for (const wchar_t *code = codes.data(); *code != L'\0'; code += wcslen(code) + 1) {
      if (_wcsicmp(code, kTestBuildUpgradeCode) == 0) {
        bundles.emplace_back(name);
        break;
      }
    }
  }

  for (const auto &bundle : bundles) {
    std::vector<wchar_t> cache(MAX_PATH * 2, L'\0');
    DWORD bytes = static_cast<DWORD>((cache.size() - 1) * sizeof(wchar_t));
    if (RegGetValueW(uninstall, bundle.c_str(), L"BundleCachePath", RRF_RT_REG_SZ, nullptr, cache.data(), &bytes) ==
        ERROR_SUCCESS) {
      // only the folder named for the bundle, in Windows' package cache
      const auto folder = std::filesystem::path(cache.data()).parent_path();
      std::error_code error;
      if (folder.filename() == bundle && folder.parent_path().filename() == L"Package Cache")
        std::filesystem::remove_all(folder, error);
    }
    RegDeleteTreeW(uninstall, bundle.c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\Installer\\Dependencies\\" + bundle).c_str());
    MS_LOG_DEBUG("removed the test build's entry %ls", bundle.c_str());
  }

  RegCloseKey(uninstall);
  return ERROR_SUCCESS;
}
