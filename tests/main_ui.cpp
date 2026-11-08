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
static int imageListsOpened = 0;
static void UiShowImages(HWND) { ++imageListsOpened; }
static int imagePickersOpened = 0;
static bool acceptImagePicker = false;
static bool UiSelectImages(HWND) { ++imagePickersOpened; return acceptImagePicker; }
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
#include "../src/ui/macros.h"
#include <shellapi.h>
static bool acceptLoad = false, acceptDiscard = false;
static int loadCalls = 0, playCalls = 0, unhookCalls = 0, finishedDrops = 0;
static std::wstring loadedPath;
static bool UiLoadFile(HWND, const std::wstring &path) {
  ++loadCalls; loadedPath = path; return acceptLoad;
}
static bool UiDiscard(HWND) { return acceptDiscard; }
static bool UiPlay() { ++playCalls; return false; }
static BOOL WINAPI UiUnhook(HHOOK) { ++unhookCalls; return TRUE; }
static BOOL WINAPI UiBeep(UINT) { return TRUE; }
static UINT WINAPI UiQueryDrop(HDROP, UINT, LPWSTR path, UINT length) {
  lstrcpynW(path, L"drop.onty", length); return 9;
}
static void WINAPI UiFinishDrop(HDROP) { ++finishedDrops; }
#define Macros_LoadFile UiLoadFile
#define Macros_ConfirmDiscard UiDiscard
#define Engine_Play UiPlay
#define UnhookWindowsHookEx UiUnhook
#define MessageBeep UiBeep
#define DragQueryFileW UiQueryDrop
#define DragFinish UiFinishDrop
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
#define Macros_ShowAutoclickImages UiShowImages
#define Macros_SelectAutoclickImages UiSelectImages
#include "../src/main.cpp"
#undef Macros_LoadFile
#undef Macros_ConfirmDiscard
#undef Engine_Play
#undef UnhookWindowsHookEx
#undef MessageBeep
#undef DragQueryFileW
#undef DragFinish
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
#undef Macros_ShowAutoclickImages
#undef Macros_SelectAutoclickImages
#include "storage_paths.h"
#define SHGetFolderPathW TestDocumentsFolder
#include "../src/core/config.cpp"
#undef SHGetFolderPathW

static bool HasText(const std::wstring &text, const wchar_t *part) {
  return text.find(part) != std::wstring::npos;
}
static void AdditionalMainTests() {
  Config previous = g_cfg;
  g_state = ST_IDLE; g_acRunning = false;
  std::wstring command = L"AutoOnty.exe --file missing.onty --play --speed 2 --close-after";
  COPYDATASTRUCT request{};
  request.dwData = 2; request.cbData = DWORD((command.size() + 1) * sizeof(wchar_t));
  request.lpData = command.data();
  g_closeAfterPlay = false;
  acceptLoad = false;
  MainProc(nullptr, WM_COPYDATA, 0, (LPARAM)&request);
  assert(loadCalls == 1 && playCalls == 0 && !g_closeAfterPlay);
  assert(g_cfg.speed == previous.speed && g_cfg.speedCustom == previous.speedCustom);
  acceptLoad = true;
  MainProc(nullptr, WM_COPYDATA, 0, (LPARAM)&request);
  assert(loadCalls == 2 && playCalls == 1 && g_closeAfterPlay && g_cfg.speed == 2);
  std::wstring invalidSpeed = L"AutoOnty.exe --file valid.onty --play --speed nan";
  auto invalidRequest = request;
  invalidRequest.lpData = invalidSpeed.data();
  invalidRequest.cbData = DWORD((invalidSpeed.size() + 1) * sizeof(wchar_t));
  MainProc(nullptr, WM_COPYDATA, 0, (LPARAM)&invalidRequest);
  assert(loadCalls == 2 && playCalls == 1 && g_cfg.speed == 2);
  request.dwData = 1;
  MainProc(nullptr, WM_COPYDATA, 0, (LPARAM)&request);
  assert(loadCalls == 3);
  MainProc(nullptr, WM_DROPFILES, 1, 0);
  assert(loadCalls == 4 && loadedPath == L"drop.onty" && finishedDrops == 1);
  g_closeAfterPlay = false;

  MAc key; key.type = A_KD; key.vk = 'A';
  g_events = {key}; g_dirty = true;
  acceptDiscard = false;
  DoCmd(IDM_REC);
  DoCmd(IDM_PASTE_CLIPBOARD);
  assert(g_state == ST_IDLE && g_events.size() == 1 && g_dirty);

  g_acReleaseHook = (HHOOK)1;
  InputProc(nullptr, WM_DESTROY, 0, 0);
  AboutProc(nullptr, WM_DESTROY, 0, 0);
  assert(g_acReleaseHook == (HHOOK)1 && unhookCalls == 0);
  g_acReleaseHook = nullptr;

  HWND host = CreateWindowW(L"STATIC", L"input test", WS_POPUP, 0, 0, 1, 1,
                             nullptr, nullptr, g_hInst, nullptr);
  assert(host);
  InDlg input;
  input.edit = CreateWindowW(L"EDIT", L"", WS_CHILD, 0, 0, 1, 1, host,
                             nullptr, g_hInst, nullptr);
  assert(input.edit);
  double value = 1;
  input.isFloat = true; input.minv = .01; input.maxv = 1000;
  input.outDouble = &value;
  g_in = &input;
  for (const wchar_t *invalid : {L"nan", L"inf", L"", L"2oops"}) {
    SetWindowTextW(input.edit, invalid);
    InputProc(host, WM_IN_OK, 0, 0);
    assert(!input.ok && value == 1 && IsWindow(host));
  }
  SetWindowTextW(input.edit, L"2,5");
  InputProc(host, WM_IN_OK, 0, 0);
  assert(input.ok && value == 2.5 && !IsWindow(host));
  g_in = nullptr;
  g_cfg = previous;
  g_events.clear(); g_dirty = false;
}

int main() {
  _set_error_mode(_OUT_TO_STDERR);
  g_hInst = GetModuleHandleW(nullptr);
  g_cfg.lang = 1; g_cfg.theme = 0; g_cfg.statusbarShow = 0;
  g_cfg.acVk = 'A'; g_cfg.acMods = HK_CTRL|HK_SHIFT|HK_ALT;
  LangInit(); GfxInit();
  // Appended image IDs must not shift the existing translation strings.
  assert(wcscmp(L(L_AC_HOLD), L"Toggle Mode") == 0);
  assert(wcscmp(L(L_AC_TARGET), L"Click when mouse is over window") == 0);
  assert(wcscmp(L(L_RELEASE_STOP), L"Release to stop") == 0);
  assert(wcscmp(L(L_IMAGE), L"Image recognition") == 0);
  assert(wcscmp(L(L_IMAGE_NOT_FOUND), L"Not found") == 0);

  assert(!ClosesMenu(IDM_IMAGE_MACRO));
  g_state = ST_PLAY;
  assert(ClosesMenu(IDM_IMAGE_MACRO));
  g_state = ST_IDLE;
  assert(!ClosesMenu(IDM_AC_IMAGE));
  assert(!ClosesMenu(IDM_AC_IMAGE_ENABLE));
  auto imageIcon = GfxImageMacroBitmap(RGB(230, 230, 230));
  assert(imageIcon && imageIcon != GfxWindowMouseBitmap(RGB(230, 230, 230)));
  g_state = ST_PLAY;
  g_imageStage = IS_SEARCHING;
  g_imageScore = 72;
  g_playPaused = false;
  assert(HasText(EngineStatusText(), L(L_IMAGE_SEARCHING)));
  g_playPaused = true;
  assert(HasText(EngineStatusText(), L(L_PAUSED)) &&
         HasText(EngineStatusText(), L(L_IMAGE_SEARCHING)));
  g_state = ST_IDLE;
  g_playPaused = false;
  g_imageStage = IS_IDLE;
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

  // Main actions share one group; settings are adjacent and macro options
  // nested.
  root = CreatePopupMenu();
  BuildMenu(root);
  const UINT actions[] = {IDM_REC,    IDM_PLAY, IDM_AC_TOGGLE,
                          IDM_MACROS, IDM_OPEN, IDM_SAVE};
  for (int i = 0; i < 6; ++i)
    assert(GetMenuItemID(root, i) == actions[i]);
  assert(GetMenuState(root, 6, MF_BYPOSITION) & MF_SEPARATOR);
  HMENU macroMenu = GetSubMenu(root, 7);
  assert(GetSubMenu(root, 8) && macroMenu && GetSubMenu(root, 9));
  assert(GetMenuState(GetSubMenu(root, 8), IDM_AC_IMAGE, MF_BYCOMMAND) != (UINT)-1);
  assert(GetMenuItemID(GetSubMenu(root, 8), 1) == IDM_AC_IMAGE_ENABLE);
  assert(!(GetMenuState(root, IDM_AC_IMAGE_ENABLE, MF_BYCOMMAND) & MF_CHECKED));
  assert(GetMenuState(root, 10, MF_BYPOSITION) & MF_SEPARATOR);
  assert(GetMenuItemID(root, 11) == IDM_TOPMOST);
  GetMenuStringW(root, 7, text, 512, MF_BYPOSITION);
  assert(wcscmp(text, L(L_MACRO_SETTINGS)) == 0);
  assert(GetMenuState(root, IDM_COPY_CLIPBOARD, MF_BYCOMMAND) == (UINT)-1);
  assert(GetMenuState(root, IDM_ASSOC_ONTY, MF_BYCOMMAND) == (UINT)-1);
  assert(GetMenuItemID(macroMenu, 0) == IDM_IMAGE_MACRO);
  assert(GetMenuState(macroMenu, IDM_COUNTDOWN, MF_BYCOMMAND) != (UINT)-1);
  assert(GetMenuState(macroMenu, IDM_REC_MOVE, MF_BYCOMMAND) != (UINT)-1);
  assert(GetMenuState(macroMenu, IDM_ED_KEYS, MF_BYCOMMAND) != (UINT)-1);
  int top = -1, sub = -1;
  FindMenuPath(root, IDM_REC_LEFT, top, sub);
  assert(top == 6 &&
         sub == 5); // separators do not count for keyboard navigation
  FindMenuPath(root, IDM_SP2, top, sub);
  assert(top == 6 && sub == 1);
  FindMenuPath(root, IDM_AC_IMAGE, top, sub);
  assert(top == 7 && sub == -1);
  FindMenuPath(root, IDM_IMAGE_MACRO, top, sub);
  assert(top == 6 && sub == -1);
  FindMenuPath(root, IDM_COUNTDOWN, top, sub);
  assert(top == 6 && sub == -1);
  auto macroBitmap = GfxMacroSettingsBitmap(RGB(230, 230, 230));
  assert(macroBitmap && macroBitmap != imageIcon);
  DestroyMenu(root);

  // Selection is only committed after the entire click is swallowed.
  DoCmd(IDM_AC_IMAGE);
  assert(imageListsOpened == 1);
  DoCmd(IDM_AC_IMAGE_ENABLE);
  assert(imagePickersOpened == 1 && !g_cfg.acImagesEnabled);
  acceptImagePicker = true;
  DoCmd(IDM_AC_IMAGE_ENABLE);
  assert(imagePickersOpened == 2 && g_cfg.acImagesEnabled);
  root = CreatePopupMenu();
  BuildAutoclickMenu(root);
  assert(GetMenuState(root, IDM_AC_IMAGE_ENABLE, MF_BYCOMMAND) & MF_CHECKED);
  DestroyMenu(root);
  DoCmd(IDM_AC_IMAGE_ENABLE);
  assert(imagePickersOpened == 2 && !g_cfg.acImagesEnabled);
  g_acRunning = true;
  root = CreatePopupMenu();
  BuildAutoclickMenu(root);
  assert(GetMenuState(root, IDM_AC_IMAGE, MF_BYCOMMAND) & MF_GRAYED);
  DestroyMenu(root);
  DoCmd(IDM_AC_IMAGE);
  assert(imageListsOpened == 1);
  DoCmd(IDM_AC_IMAGE_ENABLE);
  assert(imagePickersOpened == 2 && !g_cfg.acImagesEnabled);
  g_acRunning = false;
  DoCmd(IDM_REC_X1);
  DoCmd(IDM_REC_X2);
  assert(g_cfg.recX1 && g_cfg.recX2);
  DoCmd(IDM_REC_X1);
  assert(!g_cfg.recX1 && g_cfg.recX2);
  for (auto choice : {std::pair{IDM_AC_BTN_X1, 3}, std::pair{IDM_AC_BTN_X2, 4}}) {
    DoCmd(choice.first);
    assert(g_cfg.acButton == choice.second);
    root = CreatePopupMenu();
    BuildAutoclickMenu(root);
    assert(GetMenuState(root, choice.first, MF_BYCOMMAND) & MF_CHECKED);
    DestroyMenu(root);
  }

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
  // Exit returns to the menu before the queued message destroys the window.
  HWND quitWindow = CreateWindowW(L"STATIC", L"quit regression", WS_POPUP,
                                   0, 0, 1, 1, nullptr, nullptr, g_hInst, nullptr);
  assert(quitWindow);
  g_hwnd = quitWindow;
  g_cfg.systemTray = 0;
  DoCmd(IDM_EXIT);
  assert(IsWindow(quitWindow));
  MSG quit{};
  assert(PeekMessageW(&quit, quitWindow, WM_APP_QUIT, WM_APP_QUIT, PM_REMOVE));
  MainProc(quitWindow, quit.message, quit.wParam, quit.lParam);
  assert(!IsWindow(quitWindow));
  g_hwnd = nullptr;
  AdditionalMainTests();
  GfxShutdown();
  puts("Deferred exit and image-list routing checks passed.");
  puts("Main UI tests passed: checked app menus, picker release/cancel, picker command guard, hold repeat latch, toolbar mode and snapshot status.");
}
