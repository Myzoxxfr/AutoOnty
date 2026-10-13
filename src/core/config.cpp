#include "core/config.h"
#include <cstdlib>
#include <cwchar>
#include <shlobj.h>
#include <string>

Config g_cfg = {};
HWND g_hwnd = nullptr;
HINSTANCE g_hInst = nullptr;

static std::wstring IniPath() {
  wchar_t p[MAX_PATH];
  GetModuleFileNameW(nullptr, p, MAX_PATH);
  wchar_t *s = wcsrchr(p, L'\\');
  if (s)
    *(s + 1) = 0;
  lstrcatW(p, L"AutoOnty.ini");
  return p;
}

std::vector<std::wstring> g_recentFiles;

void RecentAdd(const std::wstring &path) {
  if (path.empty())
    return;
  for (auto it = g_recentFiles.begin(); it != g_recentFiles.end(); ++it) {
    if (_wcsicmp(it->c_str(), path.c_str()) == 0) {
      g_recentFiles.erase(it);
      break;
    }
  }
  g_recentFiles.insert(g_recentFiles.begin(), path);
  if (g_recentFiles.size() > 8)
    g_recentFiles.resize(8);
  ConfigSave();
}

void RecentClear() {
  g_recentFiles.clear();
  ConfigSave();
}

static int GetSystemDefaultTheme() {
  DWORD val = 0;
  DWORD sz = sizeof(val);
  if (RegGetValueW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val,
          &sz) == ERROR_SUCCESS) {
    return (val != 0) ? 1 : 0;
  }
  return 0;
}

void ConfigLoad() {
  std::wstring ini = IniPath();
  g_cfg.x = GetPrivateProfileIntW(L"AutoOnty", L"window_x", -1, ini.c_str());
  g_cfg.y = GetPrivateProfileIntW(L"AutoOnty", L"window_y", -1, ini.c_str());
  g_cfg.speed = GetPrivateProfileIntW(L"AutoOnty", L"speed", 1, ini.c_str());
  wchar_t spCustBuf[32] = L"";
  GetPrivateProfileStringW(L"AutoOnty", L"speed_custom_val", L"", spCustBuf, 32,
                           ini.c_str());
  if (spCustBuf[0]) {
    for (int i = 0; spCustBuf[i]; i++)
      if (spCustBuf[i] == L',')
        spCustBuf[i] = L'.';
    g_cfg.speedCustom = wcstod(spCustBuf, nullptr);
  } else {
    int oldVal =
        GetPrivateProfileIntW(L"AutoOnty", L"speed_custom", 8, ini.c_str());
    g_cfg.speedCustom = (double)oldVal;
  }
  if (g_cfg.speedCustom < 0.01)
    g_cfg.speedCustom = 0.01;
  if (g_cfg.speedCustom > 1000.0)
    g_cfg.speedCustom = 1000.0;
  g_cfg.loops = GetPrivateProfileIntW(L"AutoOnty", L"loops", 1, ini.c_str());
  g_cfg.continuous =
      GetPrivateProfileIntW(L"AutoOnty", L"continuous", 0, ini.c_str());
  g_cfg.topmost =
      GetPrivateProfileIntW(L"AutoOnty", L"topmost", 0, ini.c_str());
  int defTheme = GetSystemDefaultTheme();
  g_cfg.theme =
      GetPrivateProfileIntW(L"AutoOnty", L"theme", defTheme, ini.c_str());
  g_cfg.lang = GetPrivateProfileIntW(L"AutoOnty", L"lang", 1, ini.c_str());
  g_cfg.statusbarTop =
      GetPrivateProfileIntW(L"AutoOnty", L"statusbar_top", 1, ini.c_str());
  g_cfg.statusbarShow =
      GetPrivateProfileIntW(L"AutoOnty", L"statusbar_show", 1, ini.c_str());
  g_cfg.statusbarFollowCursor = GetPrivateProfileIntW(
      L"AutoOnty", L"statusbar_follow_monitor", 1, ini.c_str());
  {
    // Older versions had Pause / Scroll Lock / Esc tick boxes; carry the first
    // enabled one over as the new single panic key.
    int pp = GetPrivateProfileIntW(L"AutoOnty", L"panic_pause", 1, ini.c_str());
    int ps = GetPrivateProfileIntW(L"AutoOnty", L"panic_scroll", 1, ini.c_str());
    int pe = GetPrivateProfileIntW(L"AutoOnty", L"panic_esc", 0, ini.c_str());
    int defVk = pp ? VK_PAUSE : (ps ? VK_SCROLL : (pe ? VK_ESCAPE : VK_PAUSE));
    g_cfg.panicVk =
        GetPrivateProfileIntW(L"AutoOnty", L"panic_vk", defVk, ini.c_str());
    g_cfg.panicMods =
        GetPrivateProfileIntW(L"AutoOnty", L"panic_mods", 0, ini.c_str());
    if (g_cfg.panicMods < 0 || g_cfg.panicMods > 15 || g_cfg.panicVk < 0 ||
        g_cfg.panicVk > 255) {
      g_cfg.panicVk = VK_PAUSE;
      g_cfg.panicMods = 0;
    }
  }
  g_cfg.recMouseMove =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_mouse_move", 1, ini.c_str());
  g_cfg.recLeft =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_left", 1, ini.c_str());
  g_cfg.recRight =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_right", 1, ini.c_str());
  g_cfg.recMiddle =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_middle", 1, ini.c_str());
  g_cfg.recWheelUp =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_wheel_up", 1, ini.c_str());
  g_cfg.recWheelDown =
      GetPrivateProfileIntW(L"AutoOnty", L"rec_wheel_down", 1, ini.c_str());
  g_cfg.edKeys =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_keys", 1, ini.c_str());
  g_cfg.edLeft =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_left", 1, ini.c_str());
  g_cfg.edRight =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_right", 1, ini.c_str());
  g_cfg.edMiddle =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_middle", 1, ini.c_str());
  g_cfg.edWheelUp =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_wheel_up", 1, ini.c_str());
  g_cfg.edWheelDown =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_wheel_down", 1, ini.c_str());
  g_cfg.edMoves =
      GetPrivateProfileIntW(L"AutoOnty", L"ed_moves", 0, ini.c_str());
  g_cfg.panicMouseMove =
      GetPrivateProfileIntW(L"AutoOnty", L"panic_mouse", 0, ini.c_str());
  g_cfg.autoupdate =
      GetPrivateProfileIntW(L"AutoOnty", L"autoupdate", 1, ini.c_str());
  g_cfg.trayIcon =
      GetPrivateProfileIntW(L"AutoOnty", L"tray_icon", 0, ini.c_str());
  if (g_cfg.trayIcon < 0 || g_cfg.trayIcon > 2)
    g_cfg.trayIcon = 0;
  g_cfg.uiIcon = GetPrivateProfileIntW(L"AutoOnty", L"ui_icon", 0, ini.c_str());
  if (g_cfg.uiIcon < 0 || g_cfg.uiIcon > 2)
    g_cfg.uiIcon = 0;
  g_cfg.autoMinimize =
      GetPrivateProfileIntW(L"AutoOnty", L"auto_minimize", 0, ini.c_str());
  if (g_cfg.speed < -5 || g_cfg.speed > 1000)
    g_cfg.speed = 1;
  if (g_cfg.loops < 1 || g_cfg.loops > 99999)
    g_cfg.loops = 1;
  if (g_cfg.theme < 0 || g_cfg.theme > 2)
    g_cfg.theme = defTheme;
  if (g_cfg.lang < 0 || g_cfg.lang > 2)
    g_cfg.lang = 1;
  g_cfg.recordMods =
      GetPrivateProfileIntW(L"AutoOnty", L"record_mods", -1, ini.c_str());
  g_cfg.recordVk =
      GetPrivateProfileIntW(L"AutoOnty", L"record_vk", -1, ini.c_str());
  g_cfg.playMods =
      GetPrivateProfileIntW(L"AutoOnty", L"play_mods", -1, ini.c_str());
  g_cfg.playVk =
      GetPrivateProfileIntW(L"AutoOnty", L"play_vk", -1, ini.c_str());
  if (g_cfg.recordMods < 0 || g_cfg.recordMods > 15 || g_cfg.recordVk < 0) {
    int k = GetPrivateProfileIntW(L"AutoOnty", L"record_key", 0, ini.c_str());
    if (k < 0 || k > 3)
      k = 0;
    static const int mods[4] = {7, 0, 0, 0};
    static const int vk[4] = {0x52, VK_SNAPSHOT, VK_F8, VK_F12};
    g_cfg.recordMods = mods[k];
    g_cfg.recordVk = vk[k];
  }
  if (g_cfg.playMods < 0 || g_cfg.playMods > 15 || g_cfg.playVk < 0) {
    int k = GetPrivateProfileIntW(L"AutoOnty", L"play_key", 0, ini.c_str());
    if (k < 0 || k > 3)
      k = 0;
    static const int mods[4] = {7, 0, 0, 0};
    static const int vk[4] = {0x50, VK_SNAPSHOT, VK_F8, VK_F12};
    g_cfg.playMods = mods[k];
    g_cfg.playVk = vk[k];
  }
  if (g_cfg.recordVk == g_cfg.playVk && g_cfg.recordMods == g_cfg.playMods) {
    g_cfg.playMods = 7;
    g_cfg.playVk = 0x50;
  }
  auto clampI = [](int &v, int lo, int hi, int def) {
    if (v < lo || v > hi)
      v = def;
  };
  g_cfg.acMode = GetPrivateProfileIntW(L"Autoclick", L"mode", 0, ini.c_str());
  g_cfg.acHours = GetPrivateProfileIntW(L"Autoclick", L"hours", 0, ini.c_str());
  g_cfg.acMins = GetPrivateProfileIntW(L"Autoclick", L"mins", 0, ini.c_str());
  g_cfg.acSecs = GetPrivateProfileIntW(L"Autoclick", L"secs", 0, ini.c_str());
  g_cfg.acMs = GetPrivateProfileIntW(L"Autoclick", L"ms", 100, ini.c_str());
  g_cfg.acButton =
      GetPrivateProfileIntW(L"Autoclick", L"button", 0, ini.c_str());
  g_cfg.acDouble =
      GetPrivateProfileIntW(L"Autoclick", L"double", 0, ini.c_str());
  g_cfg.acRepeatUntilStop =
      GetPrivateProfileIntW(L"Autoclick", L"repeat_until_stop", 1, ini.c_str());
  g_cfg.acRepeatCount =
      GetPrivateProfileIntW(L"Autoclick", L"repeat_count", 1, ini.c_str());
  g_cfg.acHasPos =
      GetPrivateProfileIntW(L"Autoclick", L"has_pos", 0, ini.c_str());
  {
    // Parsed from text: coordinates can be negative on multi-monitor setups.
    wchar_t pb[32] = L"";
    GetPrivateProfileStringW(L"Autoclick", L"pos_x", L"0", pb, 32, ini.c_str());
    g_cfg.acX = _wtoi(pb);
    GetPrivateProfileStringW(L"Autoclick", L"pos_y", L"0", pb, 32, ini.c_str());
    g_cfg.acY = _wtoi(pb);
  }
  g_cfg.acMods =
      GetPrivateProfileIntW(L"Autoclick", L"hotkey_mods", -1, ini.c_str());
  g_cfg.acVk =
      GetPrivateProfileIntW(L"Autoclick", L"hotkey_vk", -1, ini.c_str());
  g_cfg.acPosMods =
      GetPrivateProfileIntW(L"Autoclick", L"pos_hotkey_mods", -1, ini.c_str());
  g_cfg.acPosVk =
      GetPrivateProfileIntW(L"Autoclick", L"pos_hotkey_vk", -1, ini.c_str());
  clampI(g_cfg.acMode, 0, 1, 0);
  clampI(g_cfg.acHours, 0, 99, 0);
  clampI(g_cfg.acMins, 0, 59, 0);
  clampI(g_cfg.acSecs, 0, 59, 0);
  clampI(g_cfg.acMs, 0, 999, 100);
  if (g_cfg.acHours == 0 && g_cfg.acMins == 0 && g_cfg.acSecs == 0 &&
      g_cfg.acMs < 1)
    g_cfg.acMs = 1;
  clampI(g_cfg.acButton, 0, 2, 0);
  clampI(g_cfg.acDouble, 0, 1, 0);
  clampI(g_cfg.acRepeatUntilStop, 0, 1, 1);
  clampI(g_cfg.acRepeatCount, 1, 999999, 1);
  clampI(g_cfg.acHasPos, 0, 1, 0);
  if (g_cfg.acMods < 0 || g_cfg.acMods > 15 || g_cfg.acVk < 0) {
    g_cfg.acMods = 7;
    g_cfg.acVk = 0x41; // Ctrl+Shift+Alt+A
  }
  if (g_cfg.acPosMods < 0 || g_cfg.acPosMods > 15 || g_cfg.acPosVk < 0) {
    g_cfg.acPosMods = 7;
    g_cfg.acPosVk = 0x53; // Ctrl+Shift+Alt+S
  }
  {
    // All four global hotkeys must be distinct, otherwise RegisterHotKey fails
    // silently for the duplicate. Reset the autoclick ones if they clash.
    auto same = [](int m1, int v1, int m2, int v2) {
      return v1 && v1 == v2 && m1 == m2;
    };
    if (same(g_cfg.acMods, g_cfg.acVk, g_cfg.recordMods, g_cfg.recordVk) ||
        same(g_cfg.acMods, g_cfg.acVk, g_cfg.playMods, g_cfg.playVk)) {
      g_cfg.acMods = 7;
      g_cfg.acVk = 0x41;
    }
    if (same(g_cfg.acPosMods, g_cfg.acPosVk, g_cfg.recordMods,
             g_cfg.recordVk) ||
        same(g_cfg.acPosMods, g_cfg.acPosVk, g_cfg.playMods, g_cfg.playVk) ||
        same(g_cfg.acPosMods, g_cfg.acPosVk, g_cfg.acMods, g_cfg.acVk)) {
      g_cfg.acPosMods = 7;
      g_cfg.acPosVk = 0x53;
    }
  }
  g_recentFiles.clear();
  for (int i = 0; i < 8; i++) {
    wchar_t rk[16], rv[MAX_PATH];
    swprintf_s(rk, L"recent_%d", i);
    GetPrivateProfileStringW(L"Recent", rk, L"", rv, MAX_PATH, ini.c_str());
    if (rv[0])
      g_recentFiles.push_back(rv);
  }
}

void ConfigSave() {
  std::wstring ini = IniPath();
  wchar_t b[32];
  auto w = [&](const wchar_t *k, int v) {
    swprintf_s(b, L"%d", v);
    WritePrivateProfileStringW(L"AutoOnty", k, b, ini.c_str());
  };
  w(L"window_x", g_cfg.x);
  w(L"window_y", g_cfg.y);
  w(L"speed", g_cfg.speed);
  swprintf_s(b, L"%.4g", g_cfg.speedCustom);
  WritePrivateProfileStringW(L"AutoOnty", L"speed_custom_val", b, ini.c_str());
  w(L"loops", g_cfg.loops);
  w(L"continuous", g_cfg.continuous);
  w(L"topmost", g_cfg.topmost);
  w(L"theme", g_cfg.theme);
  w(L"lang", g_cfg.lang);
  w(L"statusbar_top", g_cfg.statusbarTop);
  w(L"statusbar_show", g_cfg.statusbarShow);
  w(L"statusbar_follow_monitor", g_cfg.statusbarFollowCursor);
  w(L"panic_mods", g_cfg.panicMods);
  w(L"panic_vk", g_cfg.panicVk);
  w(L"rec_mouse_move", g_cfg.recMouseMove);
  w(L"rec_left", g_cfg.recLeft);
  w(L"rec_right", g_cfg.recRight);
  w(L"rec_middle", g_cfg.recMiddle);
  w(L"rec_wheel_up", g_cfg.recWheelUp);
  w(L"rec_wheel_down", g_cfg.recWheelDown);
  w(L"ed_keys", g_cfg.edKeys);
  w(L"ed_left", g_cfg.edLeft);
  w(L"ed_right", g_cfg.edRight);
  w(L"ed_middle", g_cfg.edMiddle);
  w(L"ed_wheel_up", g_cfg.edWheelUp);
  w(L"ed_wheel_down", g_cfg.edWheelDown);
  w(L"ed_moves", g_cfg.edMoves);
  w(L"panic_mouse", g_cfg.panicMouseMove);
  w(L"autoupdate", g_cfg.autoupdate);
  w(L"tray_icon", g_cfg.trayIcon);
  w(L"ui_icon", g_cfg.uiIcon);
  w(L"auto_minimize", g_cfg.autoMinimize);
  w(L"record_mods", g_cfg.recordMods);
  w(L"record_vk", g_cfg.recordVk);
  w(L"play_mods", g_cfg.playMods);
  w(L"play_vk", g_cfg.playVk);
  auto wa = [&](const wchar_t *k, int v) {
    swprintf_s(b, L"%d", v);
    WritePrivateProfileStringW(L"Autoclick", k, b, ini.c_str());
  };
  wa(L"mode", g_cfg.acMode);
  wa(L"hours", g_cfg.acHours);
  wa(L"mins", g_cfg.acMins);
  wa(L"secs", g_cfg.acSecs);
  wa(L"ms", g_cfg.acMs);
  wa(L"button", g_cfg.acButton);
  wa(L"double", g_cfg.acDouble);
  wa(L"repeat_until_stop", g_cfg.acRepeatUntilStop);
  wa(L"repeat_count", g_cfg.acRepeatCount);
  wa(L"has_pos", g_cfg.acHasPos);
  wa(L"pos_x", g_cfg.acX);
  wa(L"pos_y", g_cfg.acY);
  wa(L"hotkey_mods", g_cfg.acMods);
  wa(L"hotkey_vk", g_cfg.acVk);
  wa(L"pos_hotkey_mods", g_cfg.acPosMods);
  wa(L"pos_hotkey_vk", g_cfg.acPosVk);
  WritePrivateProfileStringW(L"AutoOnty", L"record_key", nullptr, ini.c_str());
  WritePrivateProfileStringW(L"AutoOnty", L"play_key", nullptr, ini.c_str());
  WritePrivateProfileSectionW(L"Recent", L"", ini.c_str());
  for (size_t i = 0; i < g_recentFiles.size(); i++) {
    wchar_t rk[16];
    swprintf_s(rk, L"recent_%zu", i);
    WritePrivateProfileStringW(L"Recent", rk, g_recentFiles[i].c_str(),
                               ini.c_str());
  }
}

bool IsFileAssociationRegistered() {
  HKEY k = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.onty", 0, KEY_READ,
                    &k) != ERROR_SUCCESS)
    return false;
  wchar_t val[128] = L"";
  DWORD sz = sizeof(val);
  LSTATUS st = RegQueryValueExW(k, nullptr, nullptr, nullptr, (LPBYTE)val, &sz);
  RegCloseKey(k);
  return (st == ERROR_SUCCESS && wcscmp(val, L"AutoOnty.Macro") == 0);
}

bool RegisterFileAssociation(bool enable) {
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  const wchar_t *exeName = wcsrchr(exe, L'\\');
  exeName = exeName ? exeName + 1 : L"AutoOnty.exe";

  if (enable) {
    HKEY kExt = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.onty", 0,
                        nullptr, 0, KEY_WRITE, nullptr, &kExt,
                        nullptr) == ERROR_SUCCESS) {
      const wchar_t progId[] = L"AutoOnty.Macro";
      RegSetValueExW(kExt, nullptr, 0, REG_SZ, (const BYTE *)progId,
                     (DWORD)((wcslen(progId) + 1) * sizeof(wchar_t)));
      RegCloseKey(kExt);
    }

    HKEY kOw = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, L"Software\\Classes\\.onty\\OpenWithProgids", 0,
            nullptr, 0, KEY_WRITE, nullptr, &kOw, nullptr) == ERROR_SUCCESS) {
      const wchar_t dummy[] = L"";
      RegSetValueExW(kOw, L"AutoOnty.Macro", 0, REG_SZ, (const BYTE *)dummy,
                     (DWORD)(sizeof(wchar_t)));
      RegCloseKey(kOw);
    }

    HKEY kProg = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\AutoOnty.Macro",
                        0, nullptr, 0, KEY_WRITE, nullptr, &kProg,
                        nullptr) == ERROR_SUCCESS) {
      const wchar_t desc[] = L"AutoOnty Macro File";
      RegSetValueExW(kProg, nullptr, 0, REG_SZ, (const BYTE *)desc,
                     (DWORD)((wcslen(desc) + 1) * sizeof(wchar_t)));
      RegCloseKey(kProg);
    }

    HKEY kIcon = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Classes\\AutoOnty.Macro\\DefaultIcon", 0,
                        nullptr, 0, KEY_WRITE, nullptr, &kIcon,
                        nullptr) == ERROR_SUCCESS) {
      std::wstring iconStr = std::wstring(L"\"") + exe + L"\",0";
      RegSetValueExW(kIcon, nullptr, 0, REG_SZ, (const BYTE *)iconStr.c_str(),
                     (DWORD)((iconStr.size() + 1) * sizeof(wchar_t)));
      RegCloseKey(kIcon);
    }

    HKEY kCmd = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Classes\\AutoOnty.Macro\\shell\\open\\command", 0,
            nullptr, 0, KEY_WRITE, nullptr, &kCmd, nullptr) == ERROR_SUCCESS) {
      std::wstring cmdStr = std::wstring(L"\"") + exe + L"\" \"%1\"";
      RegSetValueExW(kCmd, nullptr, 0, REG_SZ, (const BYTE *)cmdStr.c_str(),
                     (DWORD)((cmdStr.size() + 1) * sizeof(wchar_t)));
      RegCloseKey(kCmd);
    }

    std::wstring appCmdKey =
        std::wstring(L"Software\\Classes\\Applications\\") + exeName +
        L"\\shell\\open\\command";
    HKEY kApp = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, appCmdKey.c_str(), 0, nullptr, 0,
                        KEY_WRITE, nullptr, &kApp, nullptr) == ERROR_SUCCESS) {
      std::wstring cmdStr = std::wstring(L"\"") + exe + L"\" \"%1\"";
      RegSetValueExW(kApp, nullptr, 0, REG_SZ, (const BYTE *)cmdStr.c_str(),
                     (DWORD)((cmdStr.size() + 1) * sizeof(wchar_t)));
      RegCloseKey(kApp);
    }
  } else {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.onty");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\AutoOnty.Macro");
    std::wstring appKey =
        std::wstring(L"Software\\Classes\\Applications\\") + exeName;
    RegDeleteTreeW(HKEY_CURRENT_USER, appKey.c_str());
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return true;
}
