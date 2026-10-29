#include "core/macrometa.h"
#include "core/config.h"
#include "ui/macros.h"
#include <stdlib.h>

static std::vector<MacroMeta> g_meta;
static bool g_loaded = false;
static unsigned g_ver = 1;

static const std::wstring &LibDir() {
  static std::wstring dir = Macros_Dir();
  return dir;
}

static std::wstring IniFile() { return LibDir() + L"\\macros.ini"; }

static bool SameText(const std::wstring &a, const std::wstring &b) {
  return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

static bool ValidSpeed(int s) {
  return (s >= -5 && s <= 5) || s == 8 || s == 10 || s == 50 || s == 100 ||
         s == 999;
}

// Library file -> its name, or empty if it's somewhere else.
static std::wstring LibName(const std::wstring &path) {
  size_t s = path.find_last_of(L"\\/");
  if (s == std::wstring::npos)
    return std::wstring();
  if (!SameText(path.substr(0, s), LibDir()))
    return std::wstring();
  std::wstring f = path.substr(s + 1);
  size_t dot = f.find_last_of(L'.');
  if (dot == std::wstring::npos || dot == 0 ||
      _wcsicmp(f.c_str() + dot, L".onty") != 0)
    return std::wstring();
  f.erase(dot);
  return f;
}

static void EnsureLoaded() {
  if (g_loaded)
    return;
  g_loaded = true;
  std::wstring ini = IniFile();
  if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
    return;
  int n = (int)GetPrivateProfileIntW(L"Macros", L"count", 0, ini.c_str());
  if (n < 0)
    n = 0;
  if (n > 5000)
    n = 5000;
  for (int i = 0; i < n; i++) {
    wchar_t sec[16], nm[MAX_PATH], b[64];
    swprintf_s(sec, L"M%d", i);
    GetPrivateProfileStringW(sec, L"name", L"", nm, MAX_PATH, ini.c_str());
    if (!nm[0])
      continue;
    MacroMeta m;
    m.name = nm;
    m.hkMods = (int)GetPrivateProfileIntW(sec, L"hk_mods", 0, ini.c_str());
    m.hkVk = (int)GetPrivateProfileIntW(sec, L"hk_vk", 0, ini.c_str());
    if (m.hkMods < 0 || m.hkMods > 15 || m.hkVk < 0 || m.hkVk > 255) {
      m.hkMods = 0;
      m.hkVk = 0;
    }
    m.ovLoops = GetPrivateProfileIntW(sec, L"ov_loops", 0, ini.c_str()) ? 1 : 0;
    m.loops = (int)GetPrivateProfileIntW(sec, L"loops", 1, ini.c_str());
    if (m.loops < 1)
      m.loops = 1;
    if (m.loops > 99999)
      m.loops = 99999;
    m.continuous =
        GetPrivateProfileIntW(sec, L"continuous", 0, ini.c_str()) ? 1 : 0;
    m.ovSpeed = GetPrivateProfileIntW(sec, L"ov_speed", 0, ini.c_str()) ? 1 : 0;
    m.speed = (int)GetPrivateProfileIntW(sec, L"speed", 1, ini.c_str());
    if (!ValidSpeed(m.speed)) {
      m.speed = 1;
      m.ovSpeed = 0;
    }
    GetPrivateProfileStringW(sec, L"speed_custom", L"1", b, 64, ini.c_str());
    m.speedCustom = wcstod(b, nullptr);
    if (m.speedCustom < 0.01)
      m.speedCustom = 0.01;
    if (m.speedCustom > 1000.0)
      m.speedCustom = 1000.0;
    g_meta.push_back(m);
  }
  // Drop entries whose .onty file is gone (deleted / renamed in Explorer),
  // otherwise their hotkey stays registered and blocks the key with no row
  // in the library to remove it. Skipped if the library folder itself is
  // unreachable so a missing drive can't wipe everything.
  DWORD da = GetFileAttributesW(LibDir().c_str());
  if (da != INVALID_FILE_ATTRIBUTES && (da & FILE_ATTRIBUTE_DIRECTORY)) {
    for (size_t i = g_meta.size(); i-- > 0;) {
      std::wstring f = LibDir() + L"\\" + g_meta[i].name + L".onty";
      if (GetFileAttributesW(f.c_str()) == INVALID_FILE_ATTRIBUTES)
        g_meta.erase(g_meta.begin() + (long long)i);
    }
  }
}

std::vector<MacroMeta> &MacroMeta_All() {
  EnsureLoaded();
  return g_meta;
}

MacroMeta *MacroMeta_Find(const std::wstring &path) {
  EnsureLoaded();
  std::wstring nm = LibName(path);
  if (nm.empty())
    return nullptr;
  for (MacroMeta &m : g_meta)
    if (SameText(m.name, nm))
      return &m;
  return nullptr;
}

MacroMeta *MacroMeta_Edit(const std::wstring &path) {
  MacroMeta *m = MacroMeta_Find(path);
  if (m)
    return m;
  std::wstring nm = LibName(path);
  if (nm.empty())
    return nullptr;
  MacroMeta n;
  n.name = nm;
  g_meta.push_back(n);
  return &g_meta.back();
}

void MacroMeta_Rename(const std::wstring &oldPath, const std::wstring &newPath) {
  MacroMeta *m = MacroMeta_Find(oldPath);
  if (!m)
    return;
  std::wstring nn = LibName(newPath);
  if (nn.empty()) { // moved out of the library: the extras don't follow
    MacroMeta_Remove(oldPath);
    return;
  }
  m->name = nn;
  g_ver++;
  MacroMeta_Save();
}

void MacroMeta_Remove(const std::wstring &path) {
  EnsureLoaded();
  std::wstring nm = LibName(path);
  if (nm.empty())
    return;
  for (size_t i = 0; i < g_meta.size(); i++)
    if (SameText(g_meta[i].name, nm)) {
      g_meta.erase(g_meta.begin() + (long long)i);
      g_ver++;
      break;
    }
  MacroMeta_Save();
}

static bool HasAnything(const MacroMeta &m) {
  return m.hkVk != 0 || m.ovLoops || m.ovSpeed;
}

void MacroMeta_Save() {
  EnsureLoaded();
  g_ver++;
  for (size_t i = g_meta.size(); i-- > 0;)
    if (!HasAnything(g_meta[i]))
      g_meta.erase(g_meta.begin() + (long long)i);
  const std::wstring finalIni = IniFile();
  // Written to a temp file first and swapped in at the end, so a crash or
  // power loss mid-save can't leave a truncated macros.ini.
  std::wstring ini = finalIni + L".tmp";
  // UTF-16 file (BOM) so macro names with any characters survive the INI API.
  HANDLE f = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE)
    return;
  const BYTE bom[2] = {0xFF, 0xFE};
  DWORD wr = 0;
  WriteFile(f, bom, 2, &wr, nullptr);
  CloseHandle(f);
  wchar_t b[64];
  swprintf_s(b, L"%d", (int)g_meta.size());
  WritePrivateProfileStringW(L"Macros", L"count", b, ini.c_str());
  for (size_t i = 0; i < g_meta.size(); i++) {
    const MacroMeta &m = g_meta[i];
    wchar_t sec[16];
    swprintf_s(sec, L"M%d", (int)i);
    auto wi = [&](const wchar_t *k, int v) {
      swprintf_s(b, L"%d", v);
      WritePrivateProfileStringW(sec, k, b, ini.c_str());
    };
    WritePrivateProfileStringW(sec, L"name", m.name.c_str(), ini.c_str());
    wi(L"hk_mods", m.hkMods);
    wi(L"hk_vk", m.hkVk);
    wi(L"ov_loops", m.ovLoops);
    wi(L"loops", m.loops);
    wi(L"continuous", m.continuous);
    wi(L"ov_speed", m.ovSpeed);
    wi(L"speed", m.speed);
    swprintf_s(b, L"%.4g", m.speedCustom);
    WritePrivateProfileStringW(sec, L"speed_custom", b, ini.c_str());
  }
  WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini.c_str()); // flush
  if (!MoveFileExW(ini.c_str(), finalIni.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    DeleteFileW(ini.c_str());
}

const MacroMeta *MacroMeta_ByHotkey(int mods, int vk, const MacroMeta *except) {
  EnsureLoaded();
  if (!vk)
    return nullptr;
  for (const MacroMeta &m : g_meta)
    if (&m != except && m.hkVk == vk && m.hkMods == mods)
      return &m;
  return nullptr;
}

unsigned MacroMeta_Version() { return g_ver; }

static std::wstring g_ignPath;
static bool g_ignLoops = false, g_ignSpeed = false;

void MacroMeta_IgnoreOverrides(const std::wstring &path, bool loops,
                               bool speed) {
  g_ignPath = path;
  g_ignLoops = loops;
  g_ignSpeed = speed;
}

static bool Ignored(bool flag) {
  return flag && !g_ignPath.empty() && SameText(g_ignPath, g_lastFile);
}

static const MacroMeta *CurMeta() {
  if (g_lastFile.empty())
    return nullptr;
  return MacroMeta_Find(g_lastFile);
}

int Eff_Loops() {
  const MacroMeta *m = CurMeta();
  return (m && m->ovLoops && !Ignored(g_ignLoops)) ? m->loops : g_cfg.loops;
}

int Eff_Continuous() {
  const MacroMeta *m = CurMeta();
  return (m && m->ovLoops && !Ignored(g_ignLoops)) ? m->continuous
                                                   : g_cfg.continuous;
}

int Eff_Speed() {
  const MacroMeta *m = CurMeta();
  return (m && m->ovSpeed && !Ignored(g_ignSpeed)) ? m->speed : g_cfg.speed;
}

double Eff_SpeedCustom() {
  const MacroMeta *m = CurMeta();
  return (m && m->ovSpeed && !Ignored(g_ignSpeed)) ? m->speedCustom
                                                    : g_cfg.speedCustom;
}
