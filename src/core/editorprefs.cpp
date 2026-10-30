#include "core/editorprefs.h"
#include "ui/macros.h"
#include <algorithm>
#include <cwchar>
#include <vector>

namespace {
struct Entry {
  std::wstring path;
  EditorPreferences prefs;
};
std::vector<Entry> entries;
EditorPreferences temporary;
bool loaded = false;
std::wstring StorePath() { return Macros_Dir() + L"\\editor-settings.ini"; }
std::wstring Identity(const std::wstring &path) {
  if (path.empty())
    return {};
  DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (!n || n > 32768)
    return path;
  std::wstring full(n, L'\0');
  DWORD got = GetFullPathNameW(path.c_str(), n, full.data(), nullptr);
  if (!got || got >= n)
    return path;
  full.resize(got);
  return full;
}
bool Same(const std::wstring &a, const std::wstring &b) {
  return _wcsicmp(a.c_str(), b.c_str()) == 0;
}
void Load() {
  if (loaded)
    return;
  loaded = true;
  const auto file = StorePath();
  int count = (int)GetPrivateProfileIntW(L"Editor", L"count", 0, file.c_str());
  if (count < 0 || count > 5000)
    return;
  for (int i = 0; i < count; ++i) {
    wchar_t section[24];
    swprintf_s(section, L"P%d", i);
    std::wstring path(32768, L'\0');
    DWORD n = GetPrivateProfileStringW(section, L"path", L"", path.data(),
                                       (DWORD)path.size(), file.c_str());
    if (!n || n >= path.size() - 1)
      continue;
    path.resize(n);
    EditorPreferences prefs;
    prefs.exclude.mouse =
        GetPrivateProfileIntW(section, L"mouse", 0, file.c_str()) & 1023;
    prefs.exclude.wheel =
        GetPrivateProfileIntW(section, L"wheel", 0, file.c_str()) & 15;
    prefs.exclude.movement =
        GetPrivateProfileIntW(section, L"movement", 0, file.c_str()) != 0;
    wchar_t keys[258] = {};
    n = GetPrivateProfileStringW(section, L"keys", L"", keys, 258,
                                 file.c_str());
    if (n == 256)
      for (size_t k = 0; k < 256; ++k)
        prefs.exclude.keys[k] = keys[k] == L'1';
    prefs.customVisibility =
        GetPrivateProfileIntW(section, L"custom_visibility", 0, file.c_str()) !=
        0;
    prefs.visibility =
        GetPrivateProfileIntW(section, L"visibility", EV_ALL, file.c_str()) &
        EV_ALL;
    if (!prefs.Empty())
      entries.push_back({Identity(path), prefs});
  }
}
bool Save() {
  const auto file = StorePath(), temp = file + L".tmp";
  HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  const WORD bom = 0xFEFF;
  DWORD written = 0;
  bool ok = WriteFile(h, &bom, sizeof(bom), &written, nullptr) &&
            written == sizeof(bom);
  CloseHandle(h);
  wchar_t value[32];
  swprintf_s(value, L"%zu", entries.size());
  ok = WritePrivateProfileStringW(L"Editor", L"count", value, temp.c_str()) &&
       ok;
  for (size_t i = 0; i < entries.size() && ok; ++i) {
    wchar_t section[24];
    swprintf_s(section, L"P%zu", i);
    const auto &entry = entries[i];
    auto number = [&](const wchar_t *key, unsigned v) {
      swprintf_s(value, L"%u", v);
      ok = WritePrivateProfileStringW(section, key, value, temp.c_str()) && ok;
    };
    ok = WritePrivateProfileStringW(section, L"path", entry.path.c_str(),
                                    temp.c_str()) &&
         ok;
    number(L"mouse", entry.prefs.exclude.mouse);
    number(L"wheel", entry.prefs.exclude.wheel);
    number(L"movement", entry.prefs.exclude.movement ? 1 : 0);
    number(L"custom_visibility", entry.prefs.customVisibility ? 1 : 0);
    number(L"visibility", entry.prefs.visibility & EV_ALL);
    std::wstring keys(256, L'0');
    for (size_t k = 0; k < keys.size(); ++k)
      if (entry.prefs.exclude.keys[k])
        keys[k] = L'1';
    ok = WritePrivateProfileStringW(section, L"keys", keys.c_str(),
                                    temp.c_str()) &&
         ok;
  }
  // Cache flushing does not use the ordinary write success convention.
  WritePrivateProfileStringW(nullptr, nullptr, nullptr, temp.c_str());
  if (!ok) {
    DeleteFileW(temp.c_str());
    return false;
  }
  if (!MoveFileExW(temp.c_str(), file.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(temp.c_str());
    return false;
  }
  return true;
}
} // namespace

EditorPreferences EditorPrefs_Get(const std::wstring &path) {
  if (path.empty())
    return temporary;
  Load();
  const auto full = Identity(path);
  for (const auto &entry : entries)
    if (Same(entry.path, full))
      return entry.prefs;
  return {};
}
bool EditorPrefs_Set(const std::wstring &path, const EditorPreferences &prefs) {
  if (path.empty()) {
    temporary = prefs;
    return true;
  }
  Load();
  const auto full = Identity(path);
  auto previous = entries;
  auto found = std::find_if(entries.begin(), entries.end(),
                            [&](const Entry &e) { return Same(e.path, full); });
  if (prefs.Empty()) {
    if (found != entries.end())
      entries.erase(found);
  } else if (found != entries.end())
    found->prefs = prefs;
  else {
    if (entries.size() >= 5000)
      return false;
    entries.push_back({full, prefs});
  }
  if (Save())
    return true;
  entries.swap(previous);
  return false;
}
void EditorPrefs_ResetTemporary() { temporary = {}; }
bool EditorPrefs_SaveTemporary(const std::wstring &path) {
  if (!EditorPrefs_Set(path, temporary))
    return false;
  temporary = {};
  return true;
}
bool EditorPrefs_Rename(const std::wstring &oldPath,
                        const std::wstring &newPath) {
  if (Same(Identity(oldPath), Identity(newPath)))
    return true;
  auto prefs = EditorPrefs_Get(oldPath);
  if (!EditorPrefs_Set(newPath, prefs))
    return false;
  return EditorPrefs_Remove(oldPath);
}
bool EditorPrefs_Remove(const std::wstring &path) {
  return EditorPrefs_Set(path, {});
}
