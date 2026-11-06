#pragma once
#include <shlobj.h>
#include <string>
#include <cwchar>

static std::wstring documentsPathOverride;
static bool documentsPathFailure = false;
// Every test binary gets an isolated Documents folder beside itself.
static HRESULT WINAPI TestDocumentsFolder(HWND, int, HANDLE, DWORD, LPWSTR path) {
  if (documentsPathFailure) return E_ACCESSDENIED;
  static std::wstring base = [] {
    wchar_t executable[4096]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, 4096);
    if (!length || length >= 4096) return std::wstring{};
    std::wstring folder(executable, length);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    return folder + L"documents-" + std::to_wstring(GetCurrentProcessId()) +
           L"-" + std::to_wstring(GetTickCount64());
  }();
  const auto &folder = documentsPathOverride.empty() ? base : documentsPathOverride;
  if (folder.empty() || folder.size() >= MAX_PATH) return E_FAIL;
  if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    return E_ACCESSDENIED;
  wcscpy_s(path, MAX_PATH, folder.c_str());
  return S_OK;
}
