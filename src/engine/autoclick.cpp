#include "engine/autoclick.h"
#include "core/config.h"
#include "engine/engine.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

std::atomic<bool> g_acRunning{false};
std::atomic<unsigned> g_acClicks{0};
DWORD g_acStartTick = 0;

static std::atomic<bool> g_acStop{false};
static HANDLE g_acThread = nullptr;

// Settings are snapshotted when clicking starts, so changing a menu option
// while it runs can't change behaviour half-way through a run.
struct AcRun {
  int button;
  bool dbl;
  bool fixed;
  POINT pos;
  double intervalMs;
  bool untilStop;
  unsigned count;
};
static AcRun g_run;

DWORD Autoclick_IntervalMs() {
  unsigned long long ms =
      (((unsigned long long)g_cfg.acHours * 60ULL + g_cfg.acMins) * 60ULL +
       g_cfg.acSecs) *
          1000ULL +
      g_cfg.acMs;
  if (ms < 1)
    ms = 1;
  if (ms > 0xFFFFFFFFULL)
    ms = 0xFFFFFFFFULL;
  return (DWORD)ms;
}

static double NowMs() {
  static LARGE_INTEGER freq = []() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
  }();
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (double)c.QuadPart * 1000.0 / (double)freq.QuadPart;
}

// Same emergency keys as macro playback (Menu -> Hotkeys -> Panic stop key).
static bool PanicPressed() {
  return HotkeyDown(g_cfg.panicMods, g_cfg.panicVk);
}

// True if the window under `pt` belongs to AutoOnty itself (main window, menus,
// dialogs, HUD). We never click on our own UI: starting from the toolbar button
// leaves the cursor on it, and clicking it would just toggle autoclick off.
static bool OwnWindowAt(POINT pt) {
  HWND w = WindowFromPoint(pt);
  if (!w)
    return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(w, &pid);
  return pid == GetCurrentProcessId();
}

static void SendClick(int button, bool dbl) {
  DWORD dn = MOUSEEVENTF_LEFTDOWN, up = MOUSEEVENTF_LEFTUP;
  if (button == 1) {
    dn = MOUSEEVENTF_RIGHTDOWN;
    up = MOUSEEVENTF_RIGHTUP;
  } else if (button == 2) {
    dn = MOUSEEVENTF_MIDDLEDOWN;
    up = MOUSEEVENTF_MIDDLEUP;
  }
  INPUT in[4] = {};
  UINT n = 0;
  for (int i = 0; i < (dbl ? 2 : 1); i++) {
    in[n].type = INPUT_MOUSE;
    in[n].mi.dwFlags = dn;
    n++;
    in[n].type = INPUT_MOUSE;
    in[n].mi.dwFlags = up;
    n++;
  }
  SendInput(n, in, sizeof(INPUT));
}

// Sleeps until `target` (NowMs() timeline). Returns false if a stop was
// requested meanwhile.
static bool WaitUntil(double target) {
  for (;;) {
    if (g_acStop.load() || PanicPressed())
      return false;
    double rem = target - NowMs();
    if (rem <= 0)
      return true;
    if (rem > 20)
      Sleep(10);
    else if (rem > 2)
      Sleep(1);
    else
      Sleep(0);
  }
}

static DWORD WINAPI AcProc(LPVOID) {
  const AcRun r = g_run;
  timeBeginPeriod(1); // 1 ms timer resolution so short intervals stay accurate
  double next = NowMs();
  unsigned done = 0;
  while (!g_acStop.load() && !PanicPressed()) {
    POINT pt = r.pos;
    bool have = r.fixed || GetCursorPos(&pt);
    if (have && !OwnWindowAt(pt)) {
      if (r.fixed)
        SetCursorPos(pt.x, pt.y);
      SendClick(r.button, r.dbl);
      g_acClicks++;
      done++;
      if (!r.untilStop && done >= r.count)
        break;
    }
    next += r.intervalMs;
    double now = NowMs();
    if (next < now)
      next = now; // fell behind: don't fire a burst to catch up
    if (!WaitUntil(next))
      break;
  }
  timeEndPeriod(1);
  g_acRunning = false;
  if (g_hwnd)
    PostMessageW(g_hwnd, WM_APP_AUTOCLICKDONE, 0, 0);
  return 0;
}

bool Autoclick_Start() {
  if (g_acRunning.load())
    return false;
  if (g_acThread) {
    WaitForSingleObject(g_acThread, 3000);
    CloseHandle(g_acThread);
    g_acThread = nullptr;
  }
  g_run.button = g_cfg.acButton;
  g_run.dbl = g_cfg.acDouble != 0;
  g_run.fixed = g_cfg.acMode == 1;
  g_run.pos.x = g_cfg.acX;
  g_run.pos.y = g_cfg.acY;
  g_run.intervalMs = (double)Autoclick_IntervalMs();
  g_run.untilStop = g_cfg.acRepeatUntilStop != 0;
  g_run.count = (unsigned)(g_cfg.acRepeatCount < 1 ? 1 : g_cfg.acRepeatCount);
  g_acStop = false;
  g_acClicks = 0;
  g_acStartTick = GetTickCount();
  g_acRunning = true;
  g_acThread = CreateThread(nullptr, 0, AcProc, nullptr, 0, nullptr);
  if (!g_acThread) {
    g_acRunning = false;
    return false;
  }
  return true;
}

void Autoclick_Stop() { g_acStop = true; }

void Autoclick_StopAndWait(DWORD ms) {
  g_acStop = true;
  if (g_acThread) {
    if (WaitForSingleObject(g_acThread, ms) == WAIT_OBJECT_0) {
      CloseHandle(g_acThread);
      g_acThread = nullptr;
    }
  }
}

bool Autoclick_SavePosition() {
  POINT p;
  if (!GetCursorPos(&p))
    return false;
  g_cfg.acX = p.x;
  g_cfg.acY = p.y;
  g_cfg.acHasPos = 1;
  g_cfg.acMode = 1;
  ConfigSave();
  return true;
}
