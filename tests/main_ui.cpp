#include <windows.h>
#include <cassert>
#include <cstdio>
#include "../src/engine/autoclick.h"
#include "../src/engine/engine.h"
#include "../src/core/targeting.h"

// Exercise the real menu/status/picker procedures without desktop input,
// visible windows, cursor changes, focus changes or global hotkey registration.
static std::wstring pickedExe;
static int pickerShows = 0, pickerCaptures = 0, pickerReleases = 0;
static bool uiFocus = true, uiHover = true, uiHeld = true;
static int starts = 0;
static bool startedHeld = false;
static std::wstring UiWindowExe(HWND) { return pickedExe; }
static HWND WINAPI UiWindowFromPoint(POINT) { return (HWND)1; }
static BOOL WINAPI UiCursor(LPPOINT point) { *point = {25,25}; return TRUE; }
static BOOL WINAPI UiShow(HWND, int) { ++pickerShows; return TRUE; }
static BOOL WINAPI UiForeground(HWND) { return TRUE; }
static HWND WINAPI UiCapture(HWND) { ++pickerCaptures; return nullptr; }
static BOOL WINAPI UiRelease() { ++pickerReleases; return TRUE; }
static bool UiTargetFocused(const std::wstring &) { return uiFocus; }
static bool UiTargetReady() { return uiHover; }
static bool UiHotkeyHeld(int, int) { return uiHeld; }
static bool UiStart(bool held) {
  ++starts; startedHeld = held; g_acRunning = true; g_acHeld = held; return true;
}
#define WindowExe UiWindowExe
#define WindowFromPoint UiWindowFromPoint
#define GetCursorPos UiCursor
#define ShowWindow UiShow
#define SetForegroundWindow UiForeground
#define SetCapture UiCapture
#define ReleaseCapture UiRelease
#define TargetFocused UiTargetFocused
#define Autoclick_TargetReady UiTargetReady
#define HotkeyHeld UiHotkeyHeld
#define Autoclick_Start UiStart
#include "../src/main.cpp"
#undef WindowExe
#undef WindowFromPoint
#undef GetCursorPos
#undef ShowWindow
#undef SetForegroundWindow
#undef SetCapture
#undef ReleaseCapture
#undef TargetFocused
#undef Autoclick_TargetReady
#undef HotkeyHeld
#undef Autoclick_Start

static bool HasText(const std::wstring &text, const wchar_t *part) {
  return text.find(part) != std::wstring::npos;
}
int main() {
  _set_error_mode(_OUT_TO_STDERR);
  g_hInst = GetModuleHandleW(nullptr);
  g_cfg.lang = 1; g_cfg.theme = 0; g_cfg.statusbarShow = 0;
  g_cfg.acVk = 'A'; g_cfg.acMods = HK_CTRL|HK_SHIFT|HK_ALT;
  LangInit(); GfxInit();

  HMENU root = CreatePopupMenu();
  g_cfg.acTargets = L"game.exe;other.exe";
  TargetMenu(root, true);
  HMENU menu = GetSubMenu(root, 0);
  assert(menu && GetMenuItemCount(menu) == 4);
  assert(GetMenuState(menu, IDM_AC_TARGET_BASE, MF_BYCOMMAND) & MF_CHECKED);
  assert(GetMenuState(menu, IDM_AC_TARGET_BASE+1, MF_BYCOMMAND) & MF_CHECKED);
  wchar_t text[512]{};
  GetMenuStringW(menu, IDM_AC_TARGET_BASE, text, 512, MF_BYCOMMAND);
  assert(HasText(text, L"game.exe") && HasText(text, L"\u00d7"));
  GetMenuStringW(root, 0, text, 512, MF_BYPOSITION);
  assert(HasText(text, L"game.exe;other.exe"));
  DestroyMenu(root);

  root = CreatePopupMenu();
  g_cfg.acHold = g_cfg.acPositionJitter = 0; g_cfg.acRadius = 3;
  BuildAutoclickMenu(root);
  menu = GetSubMenu(root, 0);
  assert(!(GetMenuState(menu, IDM_AC_HOLD, MF_BYCOMMAND) & MF_CHECKED));
  assert(!(GetMenuState(menu, IDM_AC_POSITION_JITTER, MF_BYCOMMAND) & MF_CHECKED));
  GetMenuStringW(menu, IDM_AC_RADIUS, text, 512, MF_BYCOMMAND);
  assert(HasText(text, L"3"));
  DestroyMenu(root);

  // Selection is only committed after the entire click is swallowed.
  pickedExe = L"game.exe"; pickerDone = pickerClicked = false; pickerResult.clear();
  PickerProc(nullptr, WM_LBUTTONDOWN, 0, 0);
  assert(!pickerDone && pickerClicked && pickerResult == L"game.exe");
  assert(pickerShows == 2 && pickerCaptures == 1);
  PickerProc(nullptr, WM_LBUTTONUP, 0, 0);
  assert(pickerDone && pickerReleases == 1);
  // Own/unresolvable windows leave the picker active.
  pickedExe.clear(); pickerDone = pickerClicked = false;
  PickerProc(nullptr, WM_LBUTTONDOWN, 0, 0);
  PickerProc(nullptr, WM_LBUTTONUP, 0, 0);
  assert(!pickerDone && pickerResult.empty());
  // Escape and close discard a pending down selection.
  pickerResult = L"pending.exe";
  PickerProc(nullptr, WM_KEYDOWN, VK_ESCAPE, 0);
  assert(pickerDone && pickerResult.empty());
  pickerDone = false; pickerResult = L"pending.exe";
  PickerProc(nullptr, WM_CLOSE, 0, 0);
  assert(pickerDone && pickerResult.empty());

  // No global action can run while the picker owns input.
  pickerWindow = (HWND)1; g_acRunning = false;
  DoCmd(IDM_AC_TOGGLE);
  MainProc(nullptr, WM_HOTKEY, HK_ID_AC, 0);
  assert(starts == 0 && !g_acRunning);
  pickerWindow = nullptr;

  // Hold starts once, repeat cannot restart, key-up resets the latch promptly.
  g_cfg.acHold = 1; g_cfg.acTargets.clear();
  MainProc(nullptr, WM_HOTKEY, HK_ID_AC, 0);
  assert(starts == 1 && startedHeld && g_acHotkeyLatched);
  g_acRunning = false; // Simulate finite completion while key is still held.
  MainProc(nullptr, WM_HOTKEY, HK_ID_AC, 0);
  assert(starts == 1);
  KBDLLHOOKSTRUCT key{}; key.vkCode = 'A';
  AcReleaseProc(HC_ACTION, WM_KEYUP, (LPARAM)&key);
  assert(!g_acHotkeyLatched);
  MainProc(nullptr, WM_HOTKEY, HK_ID_AC, 0);
  assert(starts == 2 && startedHeld);
  g_acRunning = false;
  // Toolbar start remains a toggle-style run even in hold mode.
  DoCmd(IDM_AC_TOGGLE);
  assert(starts == 3 && !startedHeld);
  g_acRunning = false;
  g_acHotkeyLatched = false; uiHeld = false;
  MainProc(nullptr, WM_HOTKEY, HK_ID_AC, 0);
  assert(starts == 3); // A stale hotkey message after release cannot start.

  // Both HUD and dock use snapshot target names rather than edited settings.
  g_acRunning = false;
  g_cfg.acTargets = L"<snapshot.exe>";
  assert(Autoclick_Start(false)); // Actual worker waits, so never injects input.
  g_cfg.acTargets = L"edited.exe";
  uiFocus = true; uiHover = false;
  assert(HasText(EngineStatusText(), L"snapshot.exe"));
  assert(!HasText(EngineStatusText(), L"edited.exe"));
  assert(HasText(DockStatusText(), L"snapshot.exe"));
  Autoclick_StopAndWait(INFINITE);
  uiFocus = false; uiHover = true; g_acRunning = true; g_acHeld = true;
  assert(HasText(EngineStatusText(), L(L_RELEASE_STOP)));
  g_acHeld = false;
  assert(HasText(EngineStatusText(), L(L_STOP_BIND)));
  g_acRunning = false;
  GfxShutdown();
  puts("Main UI tests passed: checked app menus, picker release/cancel, picker command guard, hold repeat latch, toolbar mode and snapshot status.");
}
