#include <atomic>
#include <cassert>
#include <cstdio>
#include <functional>
#include <windows.h>
static std::atomic<bool> heldKeys[256];
static std::atomic<bool> heldMouse[5];
static std::atomic<unsigned> downEvents{0};
static std::atomic<unsigned> cursorCountBeforeT{0};
static std::atomic<unsigned> horizontalWheels{0}, verticalWheels{0},
    movementEvents{0}, cursorPositions{0};
static void WINAPI TestKeyboard(BYTE vk, BYTE, DWORD flags, ULONG_PTR) {
  heldKeys[vk] = !(flags & KEYEVENTF_KEYUP);
  if (!(flags & KEYEVENTF_KEYUP)) {
    ++downEvents;
    if (vk == 'T')
      cursorCountBeforeT = cursorPositions.load();
  }
}
static void WINAPI TestMouse(DWORD flags, DWORD, DWORD, DWORD data, ULONG_PTR) {
  if (flags & MOUSEEVENTF_HWHEEL)
    ++horizontalWheels;
  if (flags & MOUSEEVENTF_WHEEL)
    ++verticalWheels;
  if (flags & MOUSEEVENTF_MOVE)
    ++movementEvents;
  if (flags & MOUSEEVENTF_LEFTDOWN)
    heldMouse[0] = true;
  if (flags & MOUSEEVENTF_LEFTUP)
    heldMouse[0] = false;
  if (flags & MOUSEEVENTF_RIGHTDOWN)
    heldMouse[1] = true;
  if (flags & MOUSEEVENTF_RIGHTUP)
    heldMouse[1] = false;
  if (flags & MOUSEEVENTF_MIDDLEDOWN)
    heldMouse[2] = true;
  if (flags & MOUSEEVENTF_MIDDLEUP)
    heldMouse[2] = false;
  if (flags & MOUSEEVENTF_XDOWN)
    heldMouse[data == XBUTTON1 ? 3 : 4] = true;
  if (flags & MOUSEEVENTF_XUP)
    heldMouse[data == XBUTTON1 ? 3 : 4] = false;
}
static BOOL WINAPI TestCursor(int, int) {
  ++cursorPositions;
  return TRUE;
}
#define keybd_event TestKeyboard
#define mouse_event TestMouse
#define SetCursorPos TestCursor
#include "../src/engine/engine.cpp"
#undef keybd_event
#undef mouse_event
#undef SetCursorPos
#include "../src/ui/editor_model.h"

static bool Eventually(const std::function<bool()> &condition,
                       DWORD timeout = 1500) {
  DWORD start = GetTickCount();
  while (!condition()) {
    if (GetTickCount() - start >= timeout)
      return false;
    Sleep(1);
  }
  return true;
}
static std::wstring modulePathOverride;
static bool modulePathFailure = false;
static DWORD WINAPI TestModuleFileName(HMODULE module, LPWSTR buffer,
                                       DWORD size) {
  if (modulePathFailure)
    return 0;
  if (modulePathOverride.empty())
    return GetModuleFileNameW(module, buffer, size);
  if (modulePathOverride.size() >= size) {
    wmemcpy(buffer, modulePathOverride.data(), size);
    return size;
  }
  wmemcpy(buffer, modulePathOverride.c_str(), modulePathOverride.size() + 1);
  return (DWORD)modulePathOverride.size();
}
#define GetModuleFileNameW TestModuleFileName
#include "../src/core/config.cpp"
#undef GetModuleFileNameW

static std::wstring preferencesDir;
std::wstring TestPreferencesDir() { return preferencesDir; }
#define Macros_Dir TestPreferencesDir
#include "../src/core/editorprefs.cpp"
#undef Macros_Dir

int Eff_Loops() { return g_cfg.loops; }
int Eff_Continuous() { return g_cfg.continuous; }
int Eff_Speed() { return g_cfg.speed; }
double Eff_SpeedCustom() { return g_cfg.speedCustom; }
void StatusShow(const wchar_t *, UINT) {}
const wchar_t *L(int) { return L""; }

// Autoclick tests never inject input or depend on desktop/window placement.
static unsigned sentClicks = 0;
static std::atomic<unsigned> targetChecks{0};
static HWND WINAPI TestWindowFromPoint(POINT) {
  ++targetChecks;
  return (HWND)1;
}
static DWORD WINAPI TestWindowProcess(HWND, LPDWORD pid) {
  *pid = GetCurrentProcessId();
  return GetCurrentThreadId();
}
static UINT WINAPI TestSendInput(UINT n, LPINPUT, int) {
  sentClicks += n;
  return n;
}
#define NowMs AcNowMs
#define WindowFromPoint TestWindowFromPoint
#define GetWindowThreadProcessId TestWindowProcess
#define SendInput TestSendInput
#include "../src/engine/autoclick.cpp"
#undef NowMs
#undef WindowFromPoint
#undef GetWindowThreadProcessId
#undef SendInput

static DWORD WINAPI SlowWorker(LPVOID duration) {
  Sleep((DWORD)(ULONG_PTR)duration);
  return 0;
}
static bool Event(const std::string &fields) {
  return Engine_FromJson("{\"onty\":1,\"events\":[{" + fields + "}]}");
}
int main() {
  g_cfg.speed = 1;
  g_cfg.speedCustom = 1;
  g_cfg.loops = 1;
  for (auto token : {L"NaN", L"Infinity", L"-inf", L"1e999", L"1e-999", L"+1",
                     L"01", L"1.", L".5", L"1e", L"0x10", L"1x"}) {
    json::Js j{token, token + wcslen(token)};
    double value;
    assert(!json::Num(j, value));
  }
  // The numeric parser respects explicit bounds, without requiring a NUL.
  wchar_t bounded[] = {L'1', L'2', L'3'};
  json::Js j{bounded, bounded + 2};
  double value;
  assert(json::Num(j, value) && value == 12 && j.p == bounded + 2);
  assert(Event("\"e\":\"kd\",\"vk\":65,\"t\":4294967295"));
  for (auto fields :
       {"\"e\":\"kd\",\"vk\":256", "\"e\":\"kd\",\"vk\":-1",
        "\"e\":\"kd\",\"vk\":65.5", "\"e\":\"md\",\"b\":5",
        "\"e\":\"md\",\"t\":4294967296", "\"e\":\"md\",\"t\":-1",
        "\"e\":\"md\",\"x\":2147483648", "\"e\":\"md\",\"y\":-2147483649",
        "\"e\":\"wh\",\"d\":32768", "\"e\":\"mv\",\"p\":[[65536,0,1]]",
        "\"e\":\"mv\",\"p\":[[0,0,65536]]", "\"e\":\"mv\",\"p\":[[0,0,]]",
        "\"e\":\"mv\",\"rel\":1e99", "\"e\":\"md\",\"t\":NaN",
        "\"e\":\"md\",\"t\":1e999", "\"e\":\"kd\",\"vk\":65,\"ext\":2",
        "\"t\":1"}) {
    auto previous = g_events.size();
    assert(!Event(fields));
    assert(g_events.size() == previous); // Failed loads leave the macro intact.
  }
  for (auto text :
       {"{\"onty\":1e99}", "{\"onty\":1,\"loops\":1e99}",
        "{\"onty\":1,\"speed\":NaN}", "{\"onty\":1,\"speed\":-1}",
        "{\"onty\":1}garbage", "{\"onty\":1,}", "{\"onty\":1,\"future\":[{]}",
        "{\"onty\":1,\"future\":undefined}",
        "{\"onty\":1,\"future\":\"\\uZZZZ\"}"})
    assert(!Engine_FromJson(text));
  assert(Engine_FromJson(
      R"({"onty":2,"future":[{"a":[true,null,{"b":"} ]"}]}],"events":[{"e":"mv","rel":1,"p":[[-1,-32768,65535],[65535,32767,0]]},{"e":"kd","vk":65},{"e":"ku","vk":65}]})"));
  assert(g_events.size() == 3 && g_events[0].path[0].first == 65535);
  std::string serialized;
  assert(Engine_ToJson(serialized));
  assert(Engine_FromJson(serialized));
  assert(g_events.size() == 3 && g_events[0].pdelays[0] == 65535);
  assert(Engine_FromJson(
      R"({"onty":1,"events":[{"e":"mv","p":[]},{"e":"kd","vk":65}]})"));
  assert(g_events.size() == 1);
  std::string deep = "{\"onty\":1,\"future\":" + std::string(130, '[') + "0" +
                     std::string(130, ']') + "}";
  assert(!Engine_FromJson(deep));
  // Startup must retain an unfinished worker and refuse to replace it.
  g_playThread = CreateThread(nullptr, 0, SlowWorker, (LPVOID)3300, 0, nullptr);
  assert(g_playThread);
  HANDLE previous = g_playThread;
  assert(!Engine_Play() && g_playThread == previous);
  DWORD code;
  assert(GetExitCodeThread(previous, &code) && code == STILL_ACTIVE);
  Engine_Shutdown();
  assert(!g_playThread); // Joins even though state was already ST_IDLE.
  // Exercise real worker lifecycle without emitting any input.
  g_events.assign(1, MAc());
  g_events[0].type = A_MOVE;
  for (int i = 0; i < 30; ++i) {
    assert(Engine_Play());
    Engine_Shutdown();
    assert(!g_playThread && g_state == ST_IDLE);
  }
  g_acThread = CreateThread(nullptr, 0, SlowWorker, (LPVOID)3300, 0, nullptr);
  previous = g_acThread;
  assert(!Autoclick_Start() && g_acThread == previous);
  Autoclick_StopAndWait(INFINITE);
  assert(!g_acThread);
  g_cfg.acMode = 1;
  g_cfg.acMs = 1;
  g_cfg.acRepeatCount = 3;
  g_cfg.acRepeatUntilStop = 0;
  for (int i = 0; i < 30; ++i) {
    assert(Autoclick_Start());
    assert(WaitForSingleObject(g_acThread, 1000) == WAIT_OBJECT_0);
    assert(!g_acRunning && g_acClicks == 0 && sentClicks == 0);
    Autoclick_StopAndWait(INFINITE);
  }
  auto ini = IniPath();
  assert(!ini.empty() && ini.ends_with(L"\\AutoOnty.ini"));
  modulePathOverride = L"C:\\" + std::wstring(1000, L'a') + L"\\AutoOnty.exe";
  assert(IniPath() == modulePathOverride.substr(
                          0, modulePathOverride.find_last_of(L'\\') + 1) +
                          L"AutoOnty.ini");
  modulePathFailure = true;
  assert(IniPath().empty());
  modulePathFailure = false;
  modulePathOverride.clear();
  // Bulk edits affect compatible selected rows, preserving hidden movement.
  using macroedit::Item;
  std::vector<Item> items(7);
  items[0].a.type = A_KD;
  items[0].a.vk = 'A';
  items[1].isDelay = true;
  items[1].ms = 100;
  items[2].a.type = A_MD;
  items[2].a.btn = 1;
  items[3].isDelay = true;
  items[3].ms = 200;
  items[4].a.type = A_WH;
  items[4].a.wheel = 120;
  items[5].isDelay = true;
  items[5].ms = 300;
  items[6].a.type = A_KU;
  items[6].a.vk = 'A';
  MAc hidden;
  hidden.type = A_MOVE;
  hidden.delay = 20;
  hidden.path = {{1, 2}};
  hidden.pdelays = {40};
  items[6].pre = {hidden};
  auto selection = macroedit::Selection(items, {0, 6});
  assert(selection.size() == 3 && selection[0].delay == 0 &&
         selection[1].type == A_MOVE && selection[1].delay == 320 &&
         selection[2].delay == 300);
  assert(macroedit::Selection(items, {1, 3}).empty());
  assert(macroedit::Selection(items, {5, 6})[0].delay == 300);
  for (auto &item : items)
    macroedit::Apply(item, macroedit::Key, 'B', 0, true);
  assert(items[0].a.vk == 'B' && items[6].a.vk == 'B' && items[2].a.btn == 1);
  macroedit::Apply(items[0], macroedit::KeyUp, 0);
  assert(items[0].a.type == A_KU);
  macroedit::Apply(items[6], macroedit::KeyDown, 0);
  assert(items[6].a.type == A_KD);
  assert(!macroedit::Apply(items[2], macroedit::Key, 'C'));
  assert(macroedit::Apply(items[2], macroedit::Button, 4));
  macroedit::Apply(items[2], macroedit::MouseUp, 0);
  assert(items[2].a.btn == 4 && items[2].a.type == A_MU);
  macroedit::Apply(items[2], macroedit::MouseDown, 0);
  macroedit::Apply(items[2], macroedit::Coordinates, -1920, 1080);
  assert(items[2].a.type == A_MD && items[2].a.x == -1920 &&
         items[2].a.y == 1080);
  macroedit::Apply(items[4], macroedit::Wheel, -120);
  assert(items[4].a.wheel == -120);
  assert(macroedit::UniversalDelay(items, {1, 2}, 10) == 1);
  assert(items[1].ms == 10 && items[3].ms == 200);
  assert(macroedit::UniversalDelay(items, {}, 50) == 3);
  assert(items[6].pre[0].delay == 20 && items[6].pre[0].pdelays[0] == 40);

  // Actions selected without Delay rows still update their existing delays.
  assert(macroedit::UniversalDelay(items, {2, 6}, 75) == 2);
  assert(items[1].ms == 75 && items[3].ms == 50 && items[5].ms == 75);
  assert(items[6].pre[0].delay == 20 && items[6].pre[0].pdelays[0] == 40);
  assert(macroedit::UniversalDelay(items, {0}, 90) ==
         0); // Never insert a delay.
  auto multipleDelays = items;
  multipleDelays.insert(multipleDelays.begin() + 1, items[1]);
  assert(macroedit::UniversalDelay(multipleDelays, {3}, 33) == 2);
  assert(multipleDelays[1].ms == 33 && multipleDelays[2].ms == 33);
  multipleDelays.push_back(items[1]);
  assert(macroedit::UniversalDelay(multipleDelays,
                                   {(int)multipleDelays.size() - 1}, 0) == 1);
  assert(multipleDelays.back().ms == 0);

  // Real worker pause/release/restore/stop with all injected input mocked.
  std::vector<MAc> run(4);
  run[0].type = A_KD;
  run[0].vk = 'Q';
  run[1].type = A_MD;
  run[1].btn = 1;
  run[2].type = A_KU;
  run[2].vk = 'Q';
  run[2].delay = 400;
  run[3].type = A_MU;
  run[3].btn = 1;
  const size_t loadedSize = g_events.size();
  g_cfg.loops = 99;
  g_cfg.continuous = 1;
  downEvents = 0;
  assert(Engine_PlaySelection(run));
  assert(g_playTotal == 1);
  assert(Eventually([] { return heldKeys['Q'] && heldMouse[1]; }));
  Engine_TogglePause();
  assert(Eventually([] { return g_playPaused.load(); }));
  assert(!heldKeys['Q'] && !heldMouse[1]);
  DWORD elapsed = g_playElapsed.load();
  Sleep(120);
  assert(g_playElapsed == elapsed && g_state == ST_PLAY);
  Engine_TogglePause();
  assert(Eventually([] { return heldKeys['Q'] && heldMouse[1]; }));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(!heldKeys['Q'] && !heldMouse[1] && downEvents == 2);
  assert(g_events.size() == loadedSize);
  downEvents = 0;
  assert(Engine_PlaySelection(run));
  assert(Eventually([] { return heldMouse[1].load(); }));
  Engine_TogglePause();
  assert(Eventually([] { return g_playPaused.load(); }));
  Engine_Shutdown();
  assert(!heldKeys['Q'] && !heldMouse[1] && downEvents == 1 && !g_playPaused);

  // A per-selection countdown overrides the general setting without mutating
  // it.
  g_cfg.playCountdown = 7;
  downEvents = 0;
  assert(Engine_PlaySelection(run, 1));
  Sleep(100);
  assert(g_playCountdown == 1 && downEvents == 0 && g_cfg.playCountdown == 7);
  Engine_Shutdown();
  assert(g_playCountdown == 0 && downEvents == 0);
  assert(Engine_PlaySelection(run, 0));
  assert(Eventually([] { return downEvents.load() > 0; }));
  assert(g_cfg.playCountdown == 7 && g_playCountdown == 0);
  Engine_Shutdown();

  // Countdown precedes only the first loop and is cancellable before input.
  g_cfg.playCountdown = 1;
  g_cfg.loops = 2;
  g_cfg.continuous = 0;
  g_events = {run[0], run[2]};
  g_events[1].delay = 0;
  downEvents = 0;
  DWORD start = GetTickCount();
  assert(Engine_Play());
  Sleep(100);
  assert(downEvents == 0 && g_playCountdown == 1);
  Engine_TogglePause();
  assert(Eventually([] { return g_playPaused.load(); }));
  Sleep(120);
  assert(downEvents == 0 && g_playCountdown == 1);
  Engine_TogglePause();
  assert(Eventually([] { return g_state == ST_IDLE; }, 1800));
  assert(GetTickCount() - start >= 990 && GetTickCount() - start < 1800);
  Engine_Shutdown();
  assert(downEvents == 2);
  g_cfg.playCountdown = 60;
  downEvents = 0;
  assert(Engine_Play());
  Sleep(20);
  Engine_Shutdown();
  assert(downEvents == 0 && g_playCountdown == 0);
  g_cfg.playCountdown = 0;
  g_cfg.acCountdown = 1;
  targetChecks = 0;
  start = GetTickCount();
  assert(Autoclick_Start());
  Sleep(100);
  assert(targetChecks == 0 && g_acCountdown == 1);
  assert(Eventually([] { return !g_acRunning.load(); }, 1800));
  Autoclick_StopAndWait(INFINITE);
  assert(targetChecks == 3 && GetTickCount() - start >= 990);
  g_cfg.acCountdown = 60;
  targetChecks = 0;
  assert(Autoclick_Start());
  Sleep(20);
  Autoclick_StopAndWait(INFINITE);
  assert(targetChecks == 0 && g_acCountdown == 0);
  // Settings persist without touching the user's INI. Bounds and hotkey
  // conflicts are handled when an old or hand-edited config is loaded.
  wchar_t executable[4096];
  DWORD length = GetModuleFileNameW(nullptr, executable, 4096);
  assert(length && length < 4096);
  std::wstring sandbox(executable, length);
  sandbox.resize(sandbox.find_last_of(L'\\') + 1);
  sandbox += L"config-sandbox";
  assert(CreateDirectoryW(sandbox.c_str(), nullptr) ||
         GetLastError() == ERROR_ALREADY_EXISTS);
  modulePathOverride = sandbox + L"\\AutoOnty.exe";
  assert(json::WriteTextFile((sandbox + L"\\AutoOnty.ini").c_str(),
                             "[AutoOnty]\r\n"));
  ConfigLoad();
  assert(g_cfg.playCountdown == 0 && g_cfg.acCountdown == 0 &&
         g_cfg.pauseVk == 'H');
  const auto iniFile = IniPath();
  WritePrivateProfileStringW(L"AutoOnty", L"record_mods", L"7",
                             iniFile.c_str());
  WritePrivateProfileStringW(L"AutoOnty", L"record_vk", L"72", iniFile.c_str());
  WritePrivateProfileStringW(L"AutoOnty", L"countdown", L"99", iniFile.c_str());
  WritePrivateProfileStringW(L"Autoclick", L"countdown", L"-1",
                             iniFile.c_str());
  ConfigLoad();
  assert(g_cfg.pauseVk != 'H' && g_cfg.pauseMods == 7);
  assert(g_cfg.playCountdown == 0 && g_cfg.acCountdown == 0);
  g_cfg.playCountdown = 60;
  g_cfg.acCountdown = 17;
  ConfigSave();
  g_cfg.playCountdown = g_cfg.acCountdown = 0;
  ConfigLoad();
  assert(g_cfg.playCountdown == 60 && g_cfg.acCountdown == 17);
  modulePathOverride.clear();
  // Hidden movement plays between selected actions; other hidden inputs do not.
  std::vector<Item> previewRows(3);
  previewRows[0].a.type = A_KD;
  previewRows[0].a.vk = 'A';
  previewRows[1].isDelay = true;
  previewRows[1].ms = 50;
  previewRows[2].a.type = A_KU;
  previewRows[2].a.vk = 'A';
  MAc move;
  move.type = A_MOVE;
  move.delay = 20;
  move.path = {{1, 2}};
  move.pdelays = {30};
  MAc hiddenKey;
  hiddenKey.type = A_KD;
  hiddenKey.vk = 'Z';
  hiddenKey.delay = 10;
  MAc hiddenWheel;
  hiddenWheel.type = A_WH;
  hiddenWheel.wheel = -120;
  hiddenWheel.horizontal = true;
  hiddenWheel.delay = 15;
  previewRows[2].pre = {hiddenKey, move, hiddenWheel};
  auto selectedPreview = macroedit::Selection(previewRows, {0, 2});
  assert(selectedPreview.size() == 3 && selectedPreview[1].type == A_MOVE);
  assert(selectedPreview[1].delay == 30 && selectedPreview[2].delay == 65);
  PreviewExclusions exclude;
  exclude.movement = true;
  auto noMoves = macroedit::Selection(previewRows, {0, 2}, exclude);
  assert(noMoves.size() == 2 && noMoves[1].delay == 125);
  Item visibleMove;
  visibleMove.a = move;
  visibleMove.a.delay = 0;
  previewRows.insert(previewRows.begin() + 1, visibleMove);
  selectedPreview = macroedit::Selection(previewRows, {0, 3});
  assert(selectedPreview.size() == 3 && selectedPreview[1].delay == 60);
  previewRows[0].pre = {move};
  assert(macroedit::Selection(previewRows, {0}).size() == 1);
  auto lastOnly = macroedit::Selection(previewRows, {3});
  assert(lastOnly.size() == 1 && lastOnly[0].type == A_KU);
  exclude = {};
  exclude.keys['A'] = true;
  auto noKeys = macroedit::Selection(previewRows, {0, 3}, exclude);
  for (auto &a : noKeys)
    assert(a.type != A_KD && a.type != A_KU);
  for (auto vk : {VK_CONTROL, VK_SHIFT, VK_MENU}) {
    PreviewExclusions e;
    e.keys[vk] = true;
    BYTE left = vk == VK_CONTROL ? VK_LCONTROL
                : vk == VK_SHIFT ? VK_LSHIFT
                                 : VK_LMENU;
    BYTE right = left + 1;
    assert(e.KeyBlocked(left) && e.KeyBlocked(right) && e.KeyBlocked((BYTE)vk));
    e = {};
    e.keys[right] = true;
    assert(!e.KeyBlocked(left) && e.KeyBlocked(right));
    assert(e.KeyBlocked((BYTE)vk, KEYEVENTF_EXTENDEDKEY));
  }
  exclude = {};
  exclude.keys[255] = true;
  assert(exclude.KeyBlocked(VK_LWIN) && exclude.KeyBlocked(VK_RWIN));
  for (int button = 0; button < 5; ++button) {
    for (bool release : {false, true}) {
      PreviewExclusions e;
      e.mouse = 1u << (button * 2 + (release ? 1 : 0));
      MAc action;
      action.type = release ? A_MU : A_MD;
      action.btn = (BYTE)button;
      assert(e.Blocked(action));
      action.type = release ? A_MD : A_MU;
      assert(!e.Blocked(action));
    }
  }
  for (int direction = 0; direction < 4; ++direction) {
    PreviewExclusions e;
    e.wheel = 1u << direction;
    for (int actual = 0; actual < 4; ++actual) {
      MAc action;
      action.type = A_WH;
      action.horizontal = actual >= 2;
      action.wheel = (actual == 0 || actual == 3) ? 120 : -120;
      assert(e.Blocked(action) == (direction == actual));
    }
  }
  std::vector<Item> clickRows(2);
  clickRows[0].a.type = A_MD;
  clickRows[0].a.x = 100;
  clickRows[0].a.y = 200;
  clickRows[1].a.type = A_MU;
  clickRows[1].a.x = 100;
  clickRows[1].a.y = 200;
  exclude = {};
  exclude.movement = true;
  auto clicks = macroedit::Selection(clickRows, {0, 1}, exclude);
  g_cfg.playCountdown = 0;
  g_cfg.continuous = 0;
  g_cfg.loops = 1;
  cursorPositions = movementEvents = 0;
  assert(Engine_PlaySelection(clicks));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(cursorPositions == 0 && movementEvents == 0);
  previewRows[3].pre[1].mDown = previewRows[3].pre[1].mUp = true;
  selectedPreview = macroedit::Selection(previewRows, {0, 3});
  assert(!selectedPreview[1].mDown && !selectedPreview[1].mUp);

  std::vector<MAc> source = {hiddenKey, move, hiddenWheel, clickRows[0].a};
  source[3].delay = 4000000;
  std::vector<MAc> tail;
  auto projection = macroedit::FromEvents(source, tail, EV_KEYS | EV_LEFT);
  auto projectedSelection = std::vector<int>{0};
  macroedit::Reproject(projection, tail, projectedSelection, EV_ALL);
  auto restored = macroedit::ToEvents(projection, tail);
  assert(restored.size() == source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    assert(restored[i].type == source[i].type &&
           restored[i].delay == source[i].delay);
    assert(restored[i].path == source[i].path &&
           restored[i].horizontal == source[i].horizontal);
  }
  for (unsigned mask :
       {0u, (unsigned)EV_ALL, (unsigned)EV_MOVES, (unsigned)EV_WHEELLEFT}) {
    macroedit::Reproject(projection, tail, projectedSelection, mask);
    assert(macroedit::ToEvents(projection, tail).size() == source.size());
  }
  Item trailingDelay;
  trailingDelay.isDelay = true;
  trailingDelay.ms = 75;
  projection.push_back(trailingDelay);
  projectedSelection = {(int)projection.size() - 1};
  macroedit::Reproject(projection, tail, projectedSelection, EV_ALL);
  assert(projection.back().isDelay && projection.back().ms == 75);
  assert((projectedSelection == std::vector<int>{(int)projection.size() - 1}));
  macroedit::Reproject(projection, tail, projectedSelection, 0);
  assert(projection.size() == 1 && projection[0].ms == 75);
  assert(macroedit::ToEvents(projection, tail).size() == source.size());

  g_events.clear();
  g_state = ST_REC;
  g_cfg.recWheelUp = g_cfg.recWheelDown = 1;
  MSLLHOOKSTRUCT wheelEvent{};
  wheelEvent.mouseData = (DWORD)(120u << 16);
  RecMsProc(HC_ACTION, WM_MOUSEHWHEEL, (LPARAM)&wheelEvent);
  assert(g_events.size() == 1 && g_events[0].horizontal &&
         g_events[0].wheel == 120);
  g_state = ST_IDLE;
  g_cfg.loops = 1;
  g_cfg.continuous = 0;
  g_cfg.playCountdown = 0;
  assert(Engine_ToJson(serialized) &&
         serialized.find("\"hw\":true") != std::string::npos);
  assert(Engine_FromJson(serialized) && g_events[0].horizontal);
  horizontalWheels = verticalWheels = 0;
  assert(Engine_Play());
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(horizontalWheels == 1 && verticalWheels == 0);
  assert(Event("\"e\":\"wh\",\"d\":-120"));
  assert(!g_events[0].horizontal);
  assert(Engine_ToJson(serialized) &&
         serialized.find("\"hw\"") == std::string::npos);
  assert(Engine_Play());
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(horizontalWheels == 1 && verticalWheels == 1);
  assert(!Event("\"e\":\"wh\",\"d\":120,\"hw\":0"));

  std::vector<MAc> timedMouse(3);
  timedMouse[0].type = A_KD;
  timedMouse[0].vk = 'T';
  timedMouse[1] = move;
  timedMouse[1].x = 100;
  timedMouse[1].y = 200;
  timedMouse[2].type = A_KU;
  timedMouse[2].vk = 'T';
  cursorPositions = 0;
  assert(Engine_PlaySelection(timedMouse));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(cursorCountBeforeT == 0 && cursorPositions == 1);
  g_events = timedMouse;
  cursorPositions = 0;
  assert(Engine_Play());
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(cursorCountBeforeT == 1 && cursorPositions == 2);

  preferencesDir = sandbox + L"\\preferences";
  assert(CreateDirectoryW(preferencesDir.c_str(), nullptr) ||
         GetLastError() == ERROR_ALREADY_EXISTS);
  entries.clear();
  loaded = true;
  assert(Save());
  EditorPreferences preferences;
  assert(EditorPrefs_Get(L"C:\\outside\\one.onty").Empty());
  preferences.exclude.keys['A'] = true;
  preferences.exclude.mouse = 513;
  preferences.exclude.wheel = 12;
  preferences.exclude.movement = true;
  preferences.customVisibility = true;
  preferences.visibility = EV_KEYS | EV_X2;
  assert(EditorPrefs_Set(L"", preferences));
  assert(EditorPrefs_Get(L"").exclude.keys['A']);
  assert(EditorPrefs_SaveTemporary(L"C:\\outside\\日本語.onty"));
  assert(EditorPrefs_Get(L"").Empty());
  entries.clear();
  loaded = false;
  auto persisted = EditorPrefs_Get(L"c:\\OUTSIDE\\日本語.onty");
  assert(persisted.exclude.keys['A'] && persisted.exclude.mouse == 513 &&
         persisted.exclude.wheel == 12);
  assert(persisted.customVisibility &&
         persisted.visibility == (EV_KEYS | EV_X2));
  assert(EditorPrefs_Get(L"C:\\outside\\other.onty").Empty());
  assert(EditorPrefs_Rename(L"C:\\outside\\日本語.onty",
                            L"C:\\outside\\renamed.onty"));
  assert(EditorPrefs_Get(L"C:\\outside\\日本語.onty").Empty());
  assert(EditorPrefs_Get(L"C:\\outside\\renamed.onty").customVisibility);
  assert(EditorPrefs_Rename(L"C:\\outside\\renamed.onty",
                            L"c:\\OUTSIDE\\RENAMED.onty"));
  assert(EditorPrefs_Get(L"C:\\outside\\renamed.onty").exclude.movement);
  auto validDir = preferencesDir;
  preferencesDir += L"\\missing\\parent";
  assert(!EditorPrefs_Set(L"C:\\not-saved.onty", preferences));
  assert(EditorPrefs_Get(L"C:\\not-saved.onty").Empty());
  preferencesDir = validDir;
  preferences.customVisibility = false;
  assert(EditorPrefs_Set(L"C:\\outside\\renamed.onty", preferences));
  entries.clear();
  loaded = false;
  assert(!EditorPrefs_Get(L"C:\\outside\\renamed.onty").customVisibility);
  assert(EditorPrefs_Remove(L"C:\\outside\\renamed.onty"));
  entries.clear();
  loaded = false;
  assert(EditorPrefs_Get(L"C:\\outside\\renamed.onty").Empty());
  puts("Regression tests passed: previous coverage, preview filters/timing, "
       "visibility, horizontal wheel, preference persistence.");
}
