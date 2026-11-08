#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>
#include "storage_paths.h"

static bool failWrite = false, shortWrite = false, failFlush = false;
static unsigned flushes = 0, failFlushAt = 0;
static std::wstring blockedPublish, failedProfileKey, executableDirectory;
static BOOL WINAPI TestWrite(HANDLE h, LPCVOID data, DWORD size,
                             LPDWORD written, LPOVERLAPPED overlapped) {
  if (failWrite) { *written = 0; SetLastError(ERROR_DISK_FULL); return FALSE; }
  return WriteFile(h, data, shortWrite ? size / 2 : size, written, overlapped);
}
static BOOL WINAPI TestFlush(HANDLE h) {
  return failFlush || ++flushes == failFlushAt ? FALSE : FlushFileBuffers(h);
}
static BOOL WINAPI TestPublish(LPCWSTR from, LPCWSTR to, DWORD flags) {
  return blockedPublish == to ? FALSE : MoveFileExW(from, to, flags);
}
static BOOL WINAPI TestProfile(LPCWSTR section, LPCWSTR key, LPCWSTR value,
                               LPCWSTR path) {
  if (key && !failedProfileKey.empty() && failedProfileKey == key) return FALSE;
  return WritePrivateProfileStringW(section, key, value, path);
}
#define WriteFile TestWrite
#define FlushFileBuffers TestFlush
#define MoveFileExW TestPublish
#include "../src/core/json.h"
#define WritePrivateProfileStringW TestProfile
#include "../src/core/macrometa.cpp"
#undef WritePrivateProfileStringW
#undef MoveFileExW
#undef FlushFileBuffers
#undef WriteFile

Config g_cfg{};
std::wstring g_lastFile;
static std::wstring library;
std::wstring Macros_Dir() { return library; }
std::wstring IniDirectory() {
  auto directory = library + L"\\Ini";
  CreateDirectoryW(directory.c_str(), nullptr);
  return directory + L"\\";
}
std::wstring ExecutableDirectory() { return executableDirectory; }
static std::wstring copiedTranslation;
static bool failCopy = false;
static BOOL WINAPI TestCopy(LPCWSTR, LPCWSTR to, BOOL) {
  copiedTranslation = to;
  return !failCopy;
}
#define CopyFileW TestCopy
#include "../src/lang/lang.cpp"
#undef CopyFileW

static std::string Contents(const std::wstring &path) {
  std::string bytes;
  assert(json::ReadTextFile(path.c_str(), bytes));
  return bytes;
}
static void NoTemporaryFiles() {
  WIN32_FIND_DATAW data{};
  HANDLE found = FindFirstFileW((library + L"\\*.tmp-*").c_str(), &data);
  if (found != INVALID_HANDLE_VALUE) FindClose(found);
  assert(found == INVALID_HANDLE_VALUE);
}
int main() {
  wchar_t folder[MAX_PATH];
  assert(SUCCEEDED(TestDocumentsFolder(nullptr, 0, nullptr, 0, folder)));
  library = folder;
  const auto macro = library + L"\\saved.onty";
  assert(json::WriteTextFile(macro.c_str(), "original"));
  for (int failure = 0; failure < 4; ++failure) {
    failWrite = failure == 0;
    shortWrite = failure == 1;
    failFlush = failure == 2;
    blockedPublish = failure == 3 ? macro : L"";
    assert(!json::WriteTextFile(macro.c_str(), "replacement"));
    assert(Contents(macro) == "original");
    NoTemporaryFiles();
    failWrite = shortWrite = failFlush = false;
    blockedPublish.clear();
  }
  assert(json::WriteTextFile(macro.c_str(), "replacement"));
  assert(Contents(macro) == "replacement");
  assert(json::WriteTextFile(macro.c_str(), "") && Contents(macro).empty());
  assert(json::WriteTextFile(macro.c_str(), "locked original"));
  HANDLE locked = CreateFileW(macro.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
  assert(locked != INVALID_HANDLE_VALUE);
  assert(!json::WriteTextFile(macro.c_str(), "replacement"));
  CloseHandle(locked);
  assert(Contents(macro) == "locked original");
  NoTemporaryFiles();

  g_loaded = true;
  MacroMeta entry; entry.name = L"日本語"; entry.hkVk = VK_F5;
  entry.ovLoops = 1; entry.loops = 3;
  g_meta = {entry};
  assert(MacroMeta_Save());
  const auto ini = IniFile();
  const auto original = Contents(ini);
  assert(original.starts_with(std::string("\xFF\xFE", 2)));
  wchar_t savedName[MAX_PATH];
  GetPrivateProfileStringW(L"M0", L"name", L"", savedName, MAX_PATH, ini.c_str());
  assert(entry.name == savedName);
  assert(GetPrivateProfileIntW(L"M0", L"loops", 0, ini.c_str()) == 3);
  for (int failure = 0; failure < 8; ++failure) {
    g_meta[0].loops = 9;
    unsigned version = MacroMeta_Version();
    failWrite = failure == 0;
    failedProfileKey = failure == 1 ? L"count" : failure == 2 ? L"name" : L"";
    if (failure == 6) failedProfileKey = L"loops";
    if (failure == 7) failedProfileKey = L"speed_custom";
    blockedPublish = failure == 3 ? ini : L"";
    failFlush = failure == 4;
    flushes = 0; failFlushAt = failure == 5 ? 2 : 0;
    assert(!MacroMeta_Save());
    assert(Contents(ini) == original && MacroMeta_Version() == version);
    assert(g_meta[0].loops == 9);
    NoTemporaryFiles();
    failWrite = failFlush = false;
    failedProfileKey.clear(); blockedPublish.clear();
    failFlushAt = 0;
  }
  assert(MacroMeta_Save());
  assert(GetPrivateProfileIntW(L"M0", L"loops", 0, ini.c_str()) == 9);
  g_meta.clear();
  assert(MacroMeta_Save());
  assert(GetPrivateProfileIntW(L"Macros", L"count", -1, ini.c_str()) == 0);

  executableDirectory = L"C:\\" + std::wstring(300, L'x') + L"\\";
  assert(CustomLangPath() == executableDirectory + L"AutoOnty.lang.json");
  const auto translation = library + L"\\language.json";
  assert(json::WriteTextFile(translation.c_str(), "{\"strings\":{\"play\":\"Go\"}}"));
  std::wstring name;
  assert(LangImport(translation.c_str(), name));
  assert(copiedTranslation == CustomLangPath());
  assert(std::wstring(L(L_PLAY)) == L"Go");
  assert(json::WriteTextFile(translation.c_str(), "{\"strings\":{\"play\":\"Changed\"}}"));
  failCopy = true;
  assert(!LangImport(translation.c_str(), name));
  assert(std::wstring(L(L_PLAY)) == L"Go");
  failCopy = false;
  executableDirectory.clear();
  assert(CustomLangPath().empty() && !LangImport(translation.c_str(), name));
  NoTemporaryFiles();
  puts("Storage tests passed: failed/short writes, flush, locked files, publication, metadata, Unicode and long translation paths.");
}
