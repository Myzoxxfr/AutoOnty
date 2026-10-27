#include "engine/engine.h"
#include "core/config.h"
#include "core/json.h"
#include "core/macrometa.h"
#include "lang/lang.h"
#include "ui/statusbar.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

std::vector<MAc> g_events;
std::atomic<int> g_state{ST_IDLE};
std::atomic<int> g_playLoop{0};
std::atomic<bool> g_playStopped{false};
std::atomic<bool> g_mouseMovedDuringPlay{false};
DWORD g_startTick = 0;
FileSettings g_fileSettings;

static std::atomic<bool> g_playStop{false};
static HHOOK g_kbHook = nullptr;
static HHOOK g_msHook = nullptr;
static HHOOK g_playMsHook = nullptr;
static bool g_keysDown[256];
static bool g_recSwallow[256];
static bool g_recPressed[256]; // key-down was recorded, release still pending
static bool g_mbLeft = false, g_mbRight = false, g_mbMid = false,
            g_mbX1 = false, g_mbX2 = false;
static HANDLE g_playThread = nullptr;
static bool g_rawOk = false;
static HWND g_targetHwnd = nullptr;

static const int MAX_EVENTS = 100000;

static std::mutex g_mvMtx;
static std::vector<std::pair<WORD, WORD>> g_pendingRel;
static std::vector<WORD> g_pendingRelD;
static bool g_hasRelative = false;
static LONG g_rawAccX = 0, g_rawAccY = 0;
static double g_lastMs = 0; // single recording clock: time the timeline is accounted up to
static std::atomic<bool> g_stopPending{false};
static std::thread g_deltaTh;

static bool ModOk(int mods, int bit, int vk) {
  bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
  return (mods & bit) ? down : !down;
}

static bool ModOk2(int mods, int bit, int vkA, int vkB) {
  bool down =
      (GetAsyncKeyState(vkA) & 0x8000) || (GetAsyncKeyState(vkB) & 0x8000);
  return (mods & bit) ? down : !down;
}

bool HotkeyDown(int mods, int vk) {
  if (!vk)
    return false;
  if (!(GetAsyncKeyState(vk) & 0x8000))
    return false;
  if (!ModOk(mods, HK_CTRL, VK_CONTROL))
    return false;
  if (!ModOk(mods, HK_SHIFT, VK_SHIFT))
    return false;
  if (!ModOk(mods, HK_ALT, VK_MENU))
    return false;
  if (!ModOk2(mods, HK_WIN, VK_LWIN, VK_RWIN))
    return false;
  return true;
}

static std::wstring VkName(int vk) {
  if (vk == VK_SNAPSHOT)
    return L"PrtScn";
  if (vk == VK_PAUSE)
    return L"Pause";
  if (vk == VK_SCROLL)
    return L"Scroll";
  if (vk == VK_ESCAPE)
    return L"Esc";
  if (vk == VK_CAPITAL)
    return L"Caps";
  if (vk == VK_NUMLOCK)
    return L"NumLock";
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
    return L"Num " + std::to_wstring(vk - VK_NUMPAD0);
  if (vk == VK_MULTIPLY)
    return L"Num *";
  if (vk == VK_ADD)
    return L"Num +";
  if (vk == VK_SUBTRACT)
    return L"Num -";
  if (vk == VK_DECIMAL)
    return L"Num .";
  if (vk == VK_DIVIDE)
    return L"Num /";
  if (vk >= 'A' && vk <= 'Z')
    return std::wstring(1, (wchar_t)vk);
  if (vk >= '0' && vk <= '9')
    return std::wstring(1, (wchar_t)vk);
  if (vk >= VK_F1 && vk <= VK_F24)
    return L"F" + std::to_wstring(vk - VK_F1 + 1);

  wchar_t buf[64] = L"";
  int map = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
  LONG lp = (LONG)(map << 16);
  if (vk >= VK_F13 && vk <= VK_F24)
    lp |= 0x100;
  if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
      vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT || vk == VK_RIGHT ||
      vk == VK_UP || vk == VK_DOWN)
    lp |= 0x100;
  if (GetKeyNameTextW(lp, buf, 64) > 0)
    return buf;
  wchar_t b[16];
  swprintf_s(b, 16, L"0x%02X", vk);
  return b;
}

std::wstring FmtHk(int mods, int vk) {
  std::wstring s;
  if (mods & HK_CTRL)
    s += L"Ctrl+";
  if (mods & HK_ALT)
    s += L"Alt+";
  if (mods & HK_SHIFT)
    s += L"Shift+";
  if (mods & HK_WIN)
    s += L"Win+";
  s += vk ? VkName(vk) : L"...";
  return s;
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

static void PreciseSleepMs(double ms) {
  double target = NowMs() + ms;
  for (;;) {
    double r = target - NowMs();
    if (r <= 0)
      return;
    if (r > 20)
      Sleep(10);
    else if (r > 2)
      Sleep(1);
    else
      Sleep(0);
  }
}

void Engine_InitRawInput() {
  RAWINPUTDEVICE rid = {};
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = RIDEV_INPUTSINK;
  rid.hwndTarget = g_hwnd;
  g_rawOk = RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
}

void Engine_RawInput(LPARAM l) {
  if (!g_rawOk || g_state.load() != ST_REC || g_stopPending.load() ||
      !g_cfg.recMouseMove)
    return;
  UINT sz = 0;
  GetRawInputData((HRAWINPUT)l, RID_INPUT, nullptr, &sz,
                  sizeof(RAWINPUTHEADER));
  if (!sz || sz > 1024)
    return;
  BYTE buf[1024];
  if (GetRawInputData((HRAWINPUT)l, RID_INPUT, buf, &sz,
                      sizeof(RAWINPUTHEADER)) != sz)
    return;
  RAWINPUT *raw = (RAWINPUT *)buf;
  if (raw->header.dwType != RIM_TYPEMOUSE)
    return;
  LONG dx = raw->data.mouse.lLastX, dy = raw->data.mouse.lLastY;
  if (!dx && !dy)
    return;
  if (dx > 32767)
    dx = 32767;
  if (dx < -32767)
    dx = -32767;
  if (dy > 32767)
    dy = 32767;
  if (dy < -32767)
    dy = -32767;
  {
    std::lock_guard<std::mutex> lock(g_mvMtx);
    g_rawAccX += dx;
    g_rawAccY += dy;
  }
}

static bool PushAct(const MAc &a) {
  if (g_events.size() >= MAX_EVENTS) {
    Engine_RecordStop();
    StatusShow(L(L_ST_MAX), 3000);
    return false;
  }
  g_events.push_back(a);
  return true;
}

static LONG g_pendingStartX = 0, g_pendingStartY = 0;

static void FlushRawDeltas() {
  if (g_state.load() != ST_REC || g_stopPending.load())
    return;
  std::lock_guard<std::mutex> lk(g_mvMtx);
  int dx = (int)g_rawAccX;
  int dy = (int)g_rawAccY;
  if (dx == 0 && dy == 0)
    return;
  g_rawAccX = 0;
  g_rawAccY = 0;
  double now = NowMs();
  DWORD moveDelay = (g_lastMs > 0) ? (DWORD)(now - g_lastMs) : 0;
  if (moveDelay < 1)
    moveDelay = 1;
  g_lastMs = now;
  g_pendingRel.push_back({(WORD)(SHORT)dx, (WORD)(SHORT)dy});
  g_pendingRelD.push_back((WORD)(std::min)((DWORD)65535, moveDelay));
  g_hasRelative = true;
}

void Engine_FlushMoves() {
  if (g_state.load() == ST_REC && (g_mbLeft || g_mbRight)) {
    FlushRawDeltas();
  }
}

static bool TakePendingMoves(MAc &mv) {
  std::lock_guard<std::mutex> lk(g_mvMtx);
  if (g_pendingRel.empty())
    return false;
  mv.path = std::move(g_pendingRel);
  mv.pdelays = std::move(g_pendingRelD);
  mv.relative = g_hasRelative;
  mv.x = g_pendingStartX;
  mv.y = g_pendingStartY;
  g_hasRelative = true;
  g_pendingRel.clear();
  g_pendingRelD.clear();
  POINT cp = {};
  GetCursorPos(&cp);
  g_pendingStartX = cp.x;
  g_pendingStartY = cp.y;
  return true;
}

static void ClearPendingMoves() {
  std::lock_guard<std::mutex> lk(g_mvMtx);
  g_pendingRel.clear();
  g_pendingRelD.clear();
  g_hasRelative = true;
  g_rawAccX = 0;
  g_rawAccY = 0;
  POINT cp = {};
  GetCursorPos(&cp);
  g_pendingStartX = cp.x;
  g_pendingStartY = cp.y;
}

static bool OwnWindow(HWND hwnd) {
  if (!hwnd)
    return false;
  if (hwnd == g_hwnd)
    return true;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  return pid == GetCurrentProcessId();
}

static bool OwnFg() {
  HWND fg = GetForegroundWindow();
  return OwnWindow(fg);
}

static bool IsModVk(BYTE vk) {
  return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
         vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
         vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN ||
         vk == VK_RWIN;
}

static bool IsActiveRecModVk(BYTE vk) {
  if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)
    return (g_cfg.recordMods & HK_CTRL) != 0;
  if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
    return (g_cfg.recordMods & HK_SHIFT) != 0;
  if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)
    return (g_cfg.recordMods & HK_ALT) != 0;
  if (vk == VK_LWIN || vk == VK_RWIN)
    return (g_cfg.recordMods & HK_WIN) != 0;
  return false;
}

static void TrimTail() {
  if (g_events.empty())
    return;

  if (g_events.back().type == A_KD || g_events.back().type == A_KU) {
    BYTE lastVk = g_events.back().vk;
    if ((g_cfg.panicVk && lastVk == (BYTE)g_cfg.panicVk) ||
        (g_cfg.playVk && lastVk == (BYTE)g_cfg.playVk)) {
      while (!g_events.empty() &&
             (g_events.back().type == A_KD || g_events.back().type == A_KU) &&
             g_events.back().vk == lastVk) {
        g_events.pop_back();
      }
      return;
    }
  }

  bool hasCtrl = (g_cfg.recordMods & HK_CTRL) != 0;
  bool hasShift = (g_cfg.recordMods & HK_SHIFT) != 0;
  bool hasAlt = (g_cfg.recordMods & HK_ALT) != 0;
  bool hasWin = (g_cfg.recordMods & HK_WIN) != 0;

  bool seenRecVk = false;
  while (!g_events.empty()) {
    const MAc &a = g_events.back();
    if (a.type == A_KD || a.type == A_KU) {
      if (a.vk == (BYTE)g_cfg.recordVk) {
        seenRecVk = true;
        g_events.pop_back();
        continue;
      }
      if (seenRecVk) {
        if (hasCtrl && (a.vk == VK_CONTROL || a.vk == VK_LCONTROL ||
                        a.vk == VK_RCONTROL)) {
          hasCtrl = false;
          g_events.pop_back();
          continue;
        }
        if (hasShift &&
            (a.vk == VK_SHIFT || a.vk == VK_LSHIFT || a.vk == VK_RSHIFT)) {
          hasShift = false;
          g_events.pop_back();
          continue;
        }
        if (hasAlt &&
            (a.vk == VK_MENU || a.vk == VK_LMENU || a.vk == VK_RMENU)) {
          hasAlt = false;
          g_events.pop_back();
          continue;
        }
        if (hasWin && (a.vk == VK_LWIN || a.vk == VK_RWIN)) {
          hasWin = false;
          g_events.pop_back();
          continue;
        }
      }
    }
    break;
  }
}

static LRESULT CALLBACK RecKbProc(int code, WPARAM w, LPARAM l) {
  if (code >= 0 && g_state.load() == ST_REC && !g_stopPending.load()) {
    KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)l;
    bool up = (p->flags & LLKHF_UP) != 0;
    if (p->flags & LLKHF_INJECTED)
      return CallNextHookEx(nullptr, code, w, l);
    BYTE vk = (BYTE)(p->vkCode & 0xFF);

    if (up)
      g_keysDown[vk] = false;
    else {
      if (g_keysDown[vk])
        return CallNextHookEx(nullptr, code, w, l);
      g_keysDown[vk] = true;
    }

    // Never record the app's own hotkeys (record / play / autoclick), including
    // the modifiers of a combo like Ctrl+Shift+Y. Checked on modifiers only:
    // GetAsyncKeyState is not updated yet inside a LL hook, so checking the
    // key's own state let the press slip into the macro.
    auto hkMatch = [&](int hkVk, int hkMods) -> bool {
      return hkVk && vk == (BYTE)hkVk && ModOk(hkMods, HK_CTRL, VK_CONTROL) &&
             ModOk(hkMods, HK_SHIFT, VK_SHIFT) &&
             ModOk(hkMods, HK_ALT, VK_MENU) &&
             ModOk2(hkMods, HK_WIN, VK_LWIN, VK_RWIN);
    };
    if (up) {
      if (g_recSwallow[vk]) { // release of a swallowed hotkey key / modifier
        g_recSwallow[vk] = false;
        return CallNextHookEx(nullptr, code, w, l);
      }
      if (!g_recPressed[vk]) // release of a key whose press we never recorded
        return CallNextHookEx(nullptr, code, w, l);
    } else {
      int hkMods = -1;
      if (hkMatch(g_cfg.recordVk, g_cfg.recordMods))
        hkMods = g_cfg.recordMods;
      else if (hkMatch(g_cfg.playVk, g_cfg.playMods))
        hkMods = g_cfg.playMods;
      else if (hkMatch(g_cfg.acVk, g_cfg.acMods))
        hkMods = g_cfg.acMods;
      else if (hkMatch(g_cfg.acPosVk, g_cfg.acPosMods))
        hkMods = g_cfg.acPosMods;
      if (hkMods >= 0) {
        g_recSwallow[vk] = true;
        // The modifiers of the combo (Ctrl/Shift/Alt/Win) were pressed just
        // before this key and are already recorded: take them back out and
        // swallow their releases too. Their delays go back on the clock.
        double giveBack = 0;
        while (!g_events.empty() && g_events.back().type == A_KD) {
          BYTE mv = g_events.back().vk;
          bool inCombo =
              ((mv == VK_CONTROL || mv == VK_LCONTROL || mv == VK_RCONTROL) &&
               (hkMods & HK_CTRL)) ||
              ((mv == VK_SHIFT || mv == VK_LSHIFT || mv == VK_RSHIFT) &&
               (hkMods & HK_SHIFT)) ||
              ((mv == VK_MENU || mv == VK_LMENU || mv == VK_RMENU) &&
               (hkMods & HK_ALT)) ||
              ((mv == VK_LWIN || mv == VK_RWIN) && (hkMods & HK_WIN));
          if (!inCombo)
            break;
          g_recSwallow[mv] = true;
          g_recPressed[mv] = false;
          giveBack += g_events.back().delay;
          g_events.pop_back();
        }
        g_lastMs -= giveBack;
        return CallNextHookEx(nullptr, code, w, l);
      }
    }

    double now = NowMs();
    DWORD delay = g_lastMs > 0 ? (DWORD)(now - g_lastMs) : 0;

    {
      MAc moveAction;
      moveAction.type = A_MOVE;
      moveAction.delay = 0;
      moveAction.mDown = g_mbLeft || g_mbRight;
      moveAction.mUp = false;
      moveAction.btn = g_mbRight ? 1 : 0;
      if (TakePendingMoves(moveAction)) {
        PushAct(moveAction);
        g_lastMs = now;
      }
    }

    MAc a;
    a.type = up ? A_KU : A_KD;
    a.vk = vk;
    g_recPressed[vk] = !up;
    a.flags = (p->flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
    a.delay = delay > 65535 ? 65535 : delay;
    PushAct(a);
    g_lastMs = now;
  }
  return CallNextHookEx(nullptr, code, w, l);
}

static bool IsOnRecButton(POINT pt) {
  if (!IsWindow(g_hwnd) || !IsWindowVisible(g_hwnd))
    return false;
  RECT rw = {};
  GetWindowRect(g_hwnd, &rw);
  if (!PtInRect(&rw, pt))
    return false;
  POINT cpt = pt;
  ScreenToClient(g_hwnd, &cpt);
  RECT recTile = {2 * 50, 30, 3 * 50, 30 + 45};
  return PtInRect(&recTile, cpt);
}

static LRESULT CALLBACK RecMsProc(int code, WPARAM w, LPARAM l) {
  if (code >= 0 && g_state.load() == ST_REC && !g_stopPending.load()) {
    MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)l;
    if (p->flags & LLMHF_INJECTED)
      return CallNextHookEx(nullptr, code, w, l);
    UINT msg = (UINT)w;

    // Settings -> Record: skip inputs the user switched off.
    if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) && !g_cfg.recLeft)
      return CallNextHookEx(nullptr, code, w, l);
    if ((msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) && !g_cfg.recRight)
      return CallNextHookEx(nullptr, code, w, l);
    if ((msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP) && !g_cfg.recMiddle)
      return CallNextHookEx(nullptr, code, w, l);
    if (msg == WM_MOUSEWHEEL) {
      SHORT wd = (SHORT)HIWORD(p->mouseData);
      if ((wd > 0 && !g_cfg.recWheelUp) || (wd < 0 && !g_cfg.recWheelDown))
        return CallNextHookEx(nullptr, code, w, l);
    }

    if (msg == WM_MOUSEMOVE) {
      FlushRawDeltas();
    } else if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
      if (msg == WM_LBUTTONDOWN && IsOnRecButton(p->pt))
        return CallNextHookEx(nullptr, code, w, l);

      bool isRight = (msg == WM_RBUTTONDOWN);
      bool wasDown = isRight ? g_mbRight : g_mbLeft;
      if (isRight)
        g_mbRight = true;
      else
        g_mbLeft = true;

      FlushRawDeltas();

      double now = NowMs();
      DWORD delay = g_lastMs > 0 ? (DWORD)(now - g_lastMs) : 0;

      {
        MAc moveAction;
        moveAction.type = A_MOVE;
        moveAction.delay = 0;
        moveAction.mDown = wasDown;
        moveAction.mUp = false;
        moveAction.btn = isRight ? 1 : 0;
        if (TakePendingMoves(moveAction)) {
          PushAct(moveAction);
          g_lastMs = now;
        }
      }

      MAc action;
      action.type = A_MD;
      action.x = p->pt.x;
      action.y = p->pt.y;
      action.delay = delay > 65535 ? 65535 : delay;
      action.btn = isRight ? 1 : 0;
      PushAct(action);
      g_lastMs = now;
    } else if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) {
      if (msg == WM_LBUTTONUP && IsOnRecButton(p->pt) && !g_mbLeft)
        return CallNextHookEx(nullptr, code, w, l);

      bool isRight = (msg == WM_RBUTTONUP);
      FlushRawDeltas();
      if (isRight)
        g_mbRight = false;
      else
        g_mbLeft = false;

      double now = NowMs();
      DWORD delay = g_lastMs > 0 ? (DWORD)(now - g_lastMs) : 0;

      {
        MAc moveAction;
        moveAction.type = A_MOVE;
        moveAction.delay = 0;
        moveAction.mDown = true;
        moveAction.mUp = false;
        moveAction.btn = isRight ? 1 : 0;
        if (TakePendingMoves(moveAction)) {
          PushAct(moveAction);
          g_lastMs = now;
        }
      }

      MAc action;
      action.type = A_MU;
      action.x = p->pt.x;
      action.y = p->pt.y;
      action.delay = delay > 65535 ? 65535 : delay;
      action.btn = isRight ? 1 : 0;
      PushAct(action);
      g_lastMs = now;
    } else if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
      SHORT delta = (SHORT)HIWORD(p->mouseData);
      FlushRawDeltas();
      double now = NowMs();
      DWORD delay = g_lastMs > 0 ? (DWORD)(now - g_lastMs) : 0;

      {
        MAc moveAction;
        moveAction.type = A_MOVE;
        moveAction.delay = 0;
        moveAction.mDown = g_mbLeft || g_mbRight;
        moveAction.mUp = false;
        moveAction.btn = g_mbRight ? 1 : 0;
        if (TakePendingMoves(moveAction)) {
          PushAct(moveAction);
          g_lastMs = now;
        }
      }

      MAc action;
      action.type = A_WH;
      action.x = p->pt.x;
      action.y = p->pt.y;
      action.delay = delay > 65535 ? 65535 : delay;
      action.wheel = delta;
      action.btn = (delta < 0) ? 1 : 0;
      PushAct(action);
      g_lastMs = now;
    } else if (msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN ||
               msg == WM_MBUTTONUP || msg == WM_XBUTTONUP) {
      bool isDown = (msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN);
      int btn = 2;
      if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP) {
        btn = 2 + (int)HIWORD(p->mouseData);
        if (btn < 3 || btn > 4)
          return CallNextHookEx(nullptr, code, w, l);
      }
      bool *downFlag = (btn == 2) ? &g_mbMid : (btn == 3 ? &g_mbX1 : &g_mbX2);

      FlushRawDeltas();
      double now = NowMs();
      DWORD delay = g_lastMs > 0 ? (DWORD)(now - g_lastMs) : 0;

      if (isDown) {
        bool wasDown = *downFlag;
        *downFlag = true;

        {
          MAc moveAction;
          moveAction.type = A_MOVE;
          moveAction.delay = 0;
          moveAction.mDown = wasDown;
          moveAction.mUp = false;
          moveAction.btn = (BYTE)btn;
          if (TakePendingMoves(moveAction)) {
            PushAct(moveAction);
            g_lastMs = now;
          }
        }

        MAc action;
        action.type = A_MD;
        action.x = p->pt.x;
        action.y = p->pt.y;
        action.delay = delay > 65535 ? 65535 : delay;
        action.btn = (BYTE)btn;
        PushAct(action);
        g_lastMs = now;
      } else {
        *downFlag = false;

        {
          MAc moveAction;
          moveAction.type = A_MOVE;
          moveAction.delay = 0;
          moveAction.mDown = true;
          moveAction.mUp = false;
          moveAction.btn = (BYTE)btn;
          if (TakePendingMoves(moveAction)) {
            PushAct(moveAction);
            g_lastMs = now;
          }
        }

        MAc action;
        action.type = A_MU;
        action.x = p->pt.x;
        action.y = p->pt.y;
        action.delay = delay > 65535 ? 65535 : delay;
        action.btn = (BYTE)btn;
        PushAct(action);
        g_lastMs = now;
      }
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}

static void DeltaThread() {
  while (g_state.load() == ST_REC && !g_stopPending.load()) {
    FlushRawDeltas();
    Sleep(15);
  }
}

bool Engine_Record() {
  if (g_state.load() != ST_IDLE)
    return false;
  g_events.clear();
  Engine_InitRawInput();
  HWND fg = GetForegroundWindow();
  if (fg && fg != g_hwnd)
    g_targetHwnd = fg;
  memset(g_keysDown, 0, sizeof(g_keysDown));
  memset(g_recSwallow, 0, sizeof(g_recSwallow));
  memset(g_recPressed, 0, sizeof(g_recPressed));
  g_mbLeft = g_mbRight = g_mbMid = g_mbX1 = g_mbX2 = false;
  ClearPendingMoves();
  g_lastMs = NowMs();
  g_lastMs = NowMs();
  g_startTick = GetTickCount();
  g_stopPending = false;
  g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, RecKbProc, g_hInst, 0);
  g_msHook = SetWindowsHookExW(WH_MOUSE_LL, RecMsProc, g_hInst, 0);
  if (!g_kbHook && !g_msHook)
    return false;
  g_state = ST_REC;
  if (g_deltaTh.joinable())
    g_deltaTh.join();
  g_deltaTh = std::thread(DeltaThread);
  InvalidateRect(g_hwnd, nullptr, FALSE);
  return true;
}

bool Engine_RecordStop() {
  if (g_state.load() != ST_REC)
    return false;
  g_stopPending = true;
  Sleep(50);
  if (g_kbHook) {
    UnhookWindowsHookEx(g_kbHook);
    g_kbHook = nullptr;
  }
  if (g_msHook) {
    UnhookWindowsHookEx(g_msHook);
    g_msHook = nullptr;
  }
  if (g_deltaTh.joinable())
    g_deltaTh.join();
  FlushRawDeltas();
  {
    MAc moveAction;
    moveAction.type = A_MOVE;
    moveAction.delay = 0;
    moveAction.relative = true;
    if (TakePendingMoves(moveAction)) {
      PushAct(moveAction);
    }
  }
  TrimTail();
  g_state = ST_IDLE;
  g_mbLeft = g_mbRight = g_mbMid = g_mbX1 = g_mbX2 = false;
  StatusShow(L(L_ST_RECSTOP), 2500);
  InvalidateRect(g_hwnd, nullptr, FALSE);
  return true;
}

static double DelayFactor() {
  int s = Eff_Speed();
  if (s == -1)
    return 4.0;
  if (s == 0)
    return 2.0;
  if (s == -2)
    return 1.0 / 0.75;
  if (s == 1)
    return 1.0;
  if (s == -3)
    return 1.0 / 1.25;
  if (s == -4)
    return 1.0 / 1.5;
  if (s == -5)
    return 0.0;
  if (s == 100)
    return 0.0;
  if (s >= 2 && s <= 100)
    return 1.0 / (double)s;
  if (s == 999)
    return Eff_SpeedCustom() > 0.0001 ? 1.0 / Eff_SpeedCustom() : 1.0;
  return 1.0;
}

static bool g_playBtnDown[5] = {false, false, false, false, false};
static bool g_playModDown[3] = {false, false, false};
static BYTE g_playKeyFlags[256] = {0}; // 0 = up, else 1 | (ext ? 2 : 0)
static bool g_playSkipKey[256] = {false};

// True when a stored key event is the app's own record/play hotkey (old macros
// can contain them). Replaying one would fire the hotkey and stop playback.
static bool IsOwnHotkeyEvent(const MAc &a) {
  if (a.type != A_KD && a.type != A_KU)
    return false;
  if (a.type == A_KU && g_playSkipKey[a.vk]) {
    g_playSkipKey[a.vk] = false;
    return true;
  }
  if (a.type == A_KU)
    return false;
  auto match = [&](int hkVk, int hkMods) -> bool {
    if (!hkVk || a.vk != (BYTE)hkVk)
      return false;
    if ((hkMods & HK_CTRL) && !g_playModDown[0])
      return false;
    if ((hkMods & HK_SHIFT) && !g_playModDown[1])
      return false;
    if ((hkMods & HK_ALT) && !g_playModDown[2])
      return false;
    return true;
  };
  if (match(g_cfg.recordVk, g_cfg.recordMods) ||
      match(g_cfg.playVk, g_cfg.playMods)) {
    g_playSkipKey[a.vk] = true;
    return true;
  }
  return false;
}

static bool BtnIsDown(int btn) {
  if (btn >= 0 && btn < 5)
    return g_playBtnDown[btn];
  return false;
}

static void BtnDown(int btn) {
  if (btn >= 0 && btn < 5)
    g_playBtnDown[btn] = true;
  switch (btn) {
  case 0:
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    break;
  case 1:
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    break;
  case 2:
    mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
    break;
  case 3:
    mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON1, 0);
    break;
  default:
    mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON2, 0);
    break;
  }
}

static void BtnUp(int btn) {
  if (btn >= 0 && btn < 5)
    g_playBtnDown[btn] = false;
  switch (btn) {
  case 0:
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    break;
  case 1:
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
    break;
  case 2:
    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
    break;
  case 3:
    mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON1, 0);
    break;
  default:
    mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON2, 0);
    break;
  }
}

static void SendKey(BYTE vk, bool down, DWORD flags) {
  if (vk == VK_CONTROL)
    g_playModDown[0] = down;
  else if (vk == VK_SHIFT)
    g_playModDown[1] = down;
  else if (vk == VK_MENU)
    g_playModDown[2] = down;
  g_playKeyFlags[vk] =
      down ? (BYTE)(1 | ((flags & KEYEVENTF_EXTENDEDKEY) ? 2 : 0)) : 0;
  BYTE scan = (BYTE)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
  keybd_event(vk, scan,
              (flags & KEYEVENTF_EXTENDEDKEY) | (down ? 0 : KEYEVENTF_KEYUP),
              0);
}

static void ReleaseAll() {
  for (int i = 0; i < 5; i++) {
    if (g_playBtnDown[i])
      BtnUp(i);
  }
  // release every key the macro left held (not just Ctrl/Shift/Alt), so a
  // macro that ends on a key-down can't leave it stuck between loops/after stop
  for (int vk = 1; vk < 256; vk++) {
    if (g_playKeyFlags[vk])
      SendKey((BYTE)vk, false,
              (g_playKeyFlags[vk] & 2) ? KEYEVENTF_EXTENDEDKEY : 0);
  }
  if (g_playModDown[0])
    SendKey(VK_CONTROL, false, 0);
  if (g_playModDown[1])
    SendKey(VK_SHIFT, false, 0);
  if (g_playModDown[2])
    SendKey(VK_MENU, false, 0);
}

static LRESULT CALLBACK PlayMsProc(int code, WPARAM w, LPARAM l) {
  if (code >= 0 && g_state.load() == ST_PLAY) {
    MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)l;
    if (!(p->flags & LLMHF_INJECTED)) {
      g_mouseMovedDuringPlay = true;
      // Real mouse moves arrive up to 1000x/s; each one used to queue a full
      // cursor-tip re-render on the UI thread. At most ~8 per second now.
      static DWORD lastPost = 0;
      DWORD nowT = GetTickCount();
      if (nowT - lastPost >= 120) {
        lastPost = nowT;
        PostMessageW(g_hwnd, WM_APP + 5, 0, 0);
      }
      if (g_cfg.panicMouseMove) {
        g_playStop = true;
      }
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}

static bool CheckPanic() {
  if (g_playStop.load())
    return true;
  return HotkeyDown(g_cfg.panicMods, g_cfg.panicVk);
}

static DWORD WINAPI PlayProc(LPVOID) {
  int total = Eff_Continuous() ? INT_MAX : (Eff_Loops() < 1 ? 1 : Eff_Loops());
  double f = DelayFactor();
  for (int lp = 0; lp < total; lp++) {
    g_playLoop = lp;
    double base = NowMs();
    double due = 0.0;
    // Sleeps in short slices and re-checks the stop key, so a long gap in the
    // macro can't make the panic hotkey feel dead. Returns false = stop.
    auto waitDue = [&]() -> bool {
      double target = base + due * f;
      if (f > 0.0) {
        for (;;) {
          if (CheckPanic())
            return false;
          double r = target - NowMs();
          if (r <= 0.0)
            break;
          PreciseSleepMs(r > 20.0 ? 20.0 : r);
        }
      }
      return true;
    };
    for (const MAc &a : g_events) {
      if (a.x != 0 || a.y != 0) {
        SetCursorPos(a.x, a.y);
        break;
      }
    }
    for (const MAc &a : g_events) {
      if (CheckPanic())
        goto done;
      if (IsOwnHotkeyEvent(a)) {
        due += a.delay; // keep the timing of everything after it
        continue;
      }
      switch (a.type) {
      case A_MOVE: {
        if (a.path.empty())
          break;
        if (a.delay > 0) {
          due += a.delay;
          if (!waitDue())
            goto done;
        }
        if (a.x != 0 || a.y != 0) {
          SetCursorPos(a.x, a.y);
        }
        if (a.mDown && !BtnIsDown(a.btn)) {
          BtnDown(a.btn);
        }
        for (size_t i = 0; i < a.path.size(); i++) {
          if (CheckPanic())
            goto done;
          WORD sd = i < a.pdelays.size() ? a.pdelays[i] : 15;
          due += sd;
          if (!waitDue())
            goto done;
          SHORT dx = (SHORT)a.path[i].first;
          SHORT dy = (SHORT)a.path[i].second;
          mouse_event(MOUSEEVENTF_MOVE, (DWORD)(int16_t)dx, (DWORD)(int16_t)dy,
                      0, 0);
        }
        if (a.mUp && BtnIsDown(a.btn)) {
          due += 10;
          if (!waitDue())
            goto done;
          BtnUp(a.btn);
        }
        break;
      }
      case A_MD:
        due += a.delay;
        if (!waitDue())
          goto done;
        if (a.x != 0 || a.y != 0)
          SetCursorPos(a.x, a.y);
        BtnDown(a.btn);
        break;
      case A_MU:
        due += a.delay;
        if (!waitDue())
          goto done;
        if (a.x != 0 || a.y != 0)
          SetCursorPos(a.x, a.y);
        BtnUp(a.btn);
        break;
      case A_KD:
        due += a.delay;
        if (!waitDue())
          goto done;
        SendKey(a.vk, true, a.flags);
        break;
      case A_KU:
        due += a.delay;
        if (!waitDue())
          goto done;
        SendKey(a.vk, false, a.flags);
        break;
      case A_WH:
        due += a.delay;
        if (!waitDue())
          goto done;
        if (a.x != 0 || a.y != 0)
          SetCursorPos(a.x, a.y);
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(int)a.wheel, 0);
        break;
      }
    }
    ReleaseAll(); // end of this pass: let go of anything still held
  }
done:
  ReleaseAll();
  g_playStopped = g_playStop.load() || CheckPanic();
  if (g_playMsHook) {
    UnhookWindowsHookEx(g_playMsHook);
    g_playMsHook = nullptr;
  }
  g_state = ST_IDLE;
  PostMessageW(g_hwnd, WM_APP + 3, 0, 0);
  return 0;
}

bool Engine_Play() {
  if (g_state.load() != ST_IDLE)
    return false;
  if (g_events.empty()) {
    StatusShow(L(L_ST_NOTHING), 2500);
    return false;
  }
  if (g_playThread) {
    WaitForSingleObject(g_playThread, 3000);
    CloseHandle(g_playThread);
    g_playThread = nullptr;
  }
  if (g_targetHwnd && IsWindow(g_targetHwnd) && g_targetHwnd != g_hwnd)
    SetForegroundWindow(g_targetHwnd);
  memset(g_playSkipKey, 0, sizeof(g_playSkipKey));
  g_playStop = false;
  g_playStopped = false;
  g_mouseMovedDuringPlay = false;
  g_playLoop = 0;
  g_state = ST_PLAY;
  g_startTick = GetTickCount();
  g_playMsHook = SetWindowsHookExW(WH_MOUSE_LL, PlayMsProc, g_hInst, 0);
  g_playThread = CreateThread(nullptr, 0, PlayProc, nullptr, 0, nullptr);
  if (!g_playThread) {
    if (g_playMsHook) {
      UnhookWindowsHookEx(g_playMsHook);
      g_playMsHook = nullptr;
    }
    g_state = ST_IDLE;
    return false;
  }
  InvalidateRect(g_hwnd, nullptr, FALSE);
  return true;
}

void Engine_PlayStop() {
  if (g_state.load() == ST_PLAY) {
    g_playStop = true;
    if (g_playMsHook) {
      UnhookWindowsHookEx(g_playMsHook);
      g_playMsHook = nullptr;
    }
  }
}

void Engine_Shutdown() {
  if (g_state.load() == ST_REC)
    Engine_RecordStop();
  if (g_state.load() == ST_PLAY) {
    Engine_PlayStop();
    if (g_playThread) {
      WaitForSingleObject(g_playThread, 3000);
      CloseHandle(g_playThread);
      g_playThread = nullptr;
    }
    if (g_playMsHook) {
      UnhookWindowsHookEx(g_playMsHook);
      g_playMsHook = nullptr;
    }
    g_state = ST_IDLE;
  }
}

static const char *AName(int t) {
  switch (t) {
  case A_MD:
    return "md";
  case A_MU:
    return "mu";
  case A_KD:
    return "kd";
  case A_KU:
    return "ku";
  case A_MOVE:
    return "mv";
  default:
    return "wh";
  }
}

bool Engine_ToJson(std::string &outJson) {
  if (g_events.empty())
    return false;
  // The file records how this macro plays: its own settings if it has any,
  // otherwise the general ones.
  const int effSpeed = Eff_Speed();
  double spv = 1.0;
  if (effSpeed == -1)
    spv = 0.25;
  else if (effSpeed == 0)
    spv = 0.5;
  else if (effSpeed == -2)
    spv = 0.75;
  else if (effSpeed == 1)
    spv = 1.0;
  else if (effSpeed == -3)
    spv = 1.25;
  else if (effSpeed == -4)
    spv = 1.5;
  else if (effSpeed == -5)
    spv = 0.0;
  else if (effSpeed == 100)
    spv = 100.0;
  else if (effSpeed >= 2 && effSpeed <= 50)
    spv = (double)effSpeed;
  else if (effSpeed == 999)
    spv = Eff_SpeedCustom();
  char num[32];
  std::string s = "{\"onty\":1,\"speed\":";
  if (spv == (double)(int)spv) {
    sprintf_s(num, "%d", (int)spv);
    s += num;
  } else {
    sprintf_s(num, "%g", spv);
    s += num;
  }
  sprintf_s(num, "%d", Eff_Loops());
  s += ",\"loops\":" + std::string(num) +
       ",\"continuous\":" + (Eff_Continuous() ? "true" : "false") +
       ",\"events\":[";
  char b[192];
  for (size_t i = 0; i < g_events.size(); i++) {
    const MAc &a = g_events[i];
    std::string tail = i + 1 < g_events.size() ? "," : "";
    switch (a.type) {
    case A_KD:
    case A_KU:
      sprintf_s(b, "{\"t\":%lu,\"e\":\"%s\",\"vk\":%u,\"ext\":%u}%s", a.delay,
                AName(a.type), a.vk, a.flags ? 1 : 0, tail.c_str());
      s += b;
      break;
    case A_MD:
    case A_MU:
      sprintf_s(b, "{\"t\":%lu,\"e\":\"%s\",\"b\":%u,\"x\":%ld,\"y\":%ld}%s",
                a.delay, AName(a.type), a.btn, a.x, a.y, tail.c_str());
      s += b;
      break;
    case A_WH:
      sprintf_s(b, "{\"t\":%lu,\"e\":\"wh\",\"d\":%d}%s", a.delay, (int)a.wheel,
                tail.c_str());
      s += b;
      break;
    default: {
      sprintf_s(b, "{\"t\":%lu,\"e\":\"mv\",\"rel\":%u,\"p\":[", a.delay,
                a.relative ? 1u : 0u);
      s += b;
      for (size_t k = 0; k < a.path.size(); k++) {
        sprintf_s(b, "[%u,%u,%u]%s", a.path[k].first, a.path[k].second,
                  k < a.pdelays.size() ? a.pdelays[k] : 15,
                  k + 1 < a.path.size() ? "," : "");
        s += b;
      }
      s += "]}";
      s += tail;
      break;
    }
    }
  }
  s += "]}";
  outJson = std::move(s);
  return true;
}

bool Engine_Save(const wchar_t *path) {
  std::string s;
  if (!Engine_ToJson(s))
    return false;
  return json::WriteTextFile(path, s);
}

bool Engine_FromJson(const std::string &bytes) {
  std::wstring src = json::ToWide(bytes);
  json::Js j{src.c_str(), src.c_str() + src.size()};
  if (!json::Eat(j, L'{'))
    return false;
  std::vector<MAc> tmp;
  int newSpeed = -10, newLoops = -1, newCont = -1;
  double newCustom = g_cfg.speedCustom;
  int onty = 0;
  for (;;) {
    json::Ws(j);
    if (json::Eat(j, L'}'))
      break;
    std::wstring key;
    if (!json::Str(j, key) || !json::Eat(j, L':'))
      return false;
    if (key == L"onty") {
      double v;
      if (!json::Num(j, v))
        return false;
      onty = (int)v;
    } else if (key == L"speed") {
      double v;
      if (!json::Num(j, v))
        return false;
      if (v == 0.25)
        newSpeed = -1;
      else if (v == 0.5)
        newSpeed = 0;
      else if (v == 0.75)
        newSpeed = -2;
      else if (v == 1.25)
        newSpeed = -3;
      else if (v == 1.5)
        newSpeed = -4;
      else if (v == 0.0)
        newSpeed = -5;
      else if (v == 100.0)
        newSpeed = 100;
      else if (v == 1 || v == 2 || v == 3 || v == 4 || v == 5 || v == 8 ||
               v == 10 || v == 50)
        newSpeed = (int)v;
      else {
        newSpeed = 999;
        newCustom = v < 0.01 ? 0.01 : (v > 1000.0 ? 1000.0 : v);
      }
    } else if (key == L"loops") {
      double v;
      if (!json::Num(j, v))
        return false;
      int n = (int)(v + 0.5);
      newLoops = n < 1 ? 1 : (n > 99999 ? 99999 : n);
    } else if (key == L"continuous") {
      bool b;
      if (!json::Bool(j, b))
        return false;
      newCont = b ? 1 : 0;
    } else if (key == L"events") {
      json::Ws(j);
      if (!json::Eat(j, L'['))
        return false;
      for (;;) {
        json::Ws(j);
        if (json::Eat(j, L']'))
          break;
        json::Ws(j);
        if (!json::Eat(j, L'{'))
          return false;
        MAc a;
        bool isRel = false, hasP = false;
        std::vector<std::pair<WORD, WORD>> pts;
        std::vector<WORD> dls;
        for (;;) {
          json::Ws(j);
          if (json::Eat(j, L'}'))
            break;
          std::wstring ek;
          if (!json::Str(j, ek) || !json::Eat(j, L':'))
            return false;
          if (ek == L"t") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.delay = v < 0 ? 0 : (DWORD)v;
          } else if (ek == L"e") {
            std::wstring v;
            if (!json::Str(j, v))
              return false;
            if (v == L"kd")
              a.type = A_KD;
            else if (v == L"ku")
              a.type = A_KU;
            else if (v == L"md")
              a.type = A_MD;
            else if (v == L"mu")
              a.type = A_MU;
            else if (v == L"mv")
              a.type = A_MOVE;
            else if (v == L"wh")
              a.type = A_WH;
            else if (v == L"mm") {
              a.type = A_MOVE;
              isRel = false;
              hasP = true;
            } else if (v == L"mr") {
              a.type = A_MOVE;
              isRel = true;
              hasP = true;
            } else
              return false;
          } else if (ek == L"vk") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.vk = (BYTE)(int)v;
          } else if (ek == L"ext" || ek == L"sc" || ek == L"flags") {
            double v;
            if (!json::Num(j, v))
              return false;
            if (ek == L"ext" || ek == L"flags")
              a.flags = (int)v ? KEYEVENTF_EXTENDEDKEY : 0;
          } else if (ek == L"b" || ek == L"btn") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.btn = (BYTE)(int)v;
          } else if (ek == L"d" || ek == L"wheel") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.wheel = (SHORT)(int)v;
          } else if (ek == L"x") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.x = (LONG)v;
          } else if (ek == L"y") {
            double v;
            if (!json::Num(j, v))
              return false;
            a.y = (LONG)v;
          } else if (ek == L"rel") {
            bool b = false;
            double v = 0;
            if (json::Bool(j, b))
              isRel = b;
            else if (json::Num(j, v))
              isRel = ((int)v != 0);
            else
              return false;
            hasP = true;
          } else if (ek == L"p" || ek == L"path") {
            hasP = true;
            json::Ws(j);
            if (!json::Eat(j, L'['))
              return false;
            for (;;) {
              json::Ws(j);
              if (json::Eat(j, L']'))
                break;
              json::Ws(j);
              if (!json::Eat(j, L'['))
                return false;
              double a0 = 0, a1 = 0, a2 = 15;
              if (!json::Num(j, a0) || !json::Eat(j, L','))
                return false;
              if (!json::Num(j, a1))
                return false;
              if (json::Eat(j, L',')) {
                json::Ws(j);
                if (j.p < j.e &&
                    (*j.p == L'-' || (*j.p >= L'0' && *j.p <= L'9'))) {
                  if (!json::Num(j, a2))
                    return false;
                }
              }
              json::Ws(j);
              if (!json::Eat(j, L']'))
                return false;
              pts.push_back({(WORD)(int)a0, (WORD)(int)a1});
              dls.push_back((WORD)(int)a2);
              if (!json::Eat(j, L',')) {
                json::Ws(j);
                if (!json::Eat(j, L']'))
                  return false;
                break;
              }
            }
          } else if (ek == L"delays") {
            json::Ws(j);
            if (!json::Eat(j, L'['))
              return false;
            dls.clear();
            for (;;) {
              json::Ws(j);
              if (json::Eat(j, L']'))
                break;
              double d = 15;
              if (!json::Num(j, d))
                return false;
              dls.push_back((WORD)(int)d);
              if (!json::Eat(j, L',')) {
                json::Ws(j);
                if (!json::Eat(j, L']'))
                  return false;
                break;
              }
            }
          } else {
            if (!json::SkipVal(j))
              return false;
          }
          if (!json::Eat(j, L',')) {
            json::Ws(j);
            if (!json::Eat(j, L'}'))
              return false;
            break;
          }
        }
        if (a.type == A_MOVE && hasP && pts.empty()) {
          if (a.x || a.y || isRel) {
            pts.push_back({(WORD)(SHORT)a.x, (WORD)(SHORT)a.y});
            dls.push_back((WORD)(std::max)(a.delay, (DWORD)1));
            a.delay = 0;
            a.x = 0;
            a.y = 0;
          }
        }
        if (a.type == A_MOVE) {
          a.relative = isRel;
          while (dls.size() < pts.size()) {
            dls.push_back(15);
          }
          a.path.swap(pts);
          a.pdelays.swap(dls);
          if (a.path.empty())
            continue;
        }
        if (a.type == A_KD || a.type == A_KU) {
          if (!a.vk)
            return false;
        }
        if (tmp.size() < MAX_EVENTS)
          tmp.push_back(a);
        if (!json::Eat(j, L',')) {
          json::Ws(j);
          if (!json::Eat(j, L']'))
            return false;
          break;
        }
      }
    } else {
      if (!json::SkipVal(j))
        return false;
    }
    if (!json::Eat(j, L',')) {
      json::Ws(j);
      if (!json::Eat(j, L'}'))
        return false;
      break;
    }
  }
  if (onty != 1 && onty != 2 && tmp.empty())
    return false;
  // What the file says is only remembered, not applied: the general Speed /
  // Loops settings stay the default for every macro (a macro can override them
  // from the macro library). Command-line runs apply it, see
  // Engine_ApplyFileSettings.
  g_fileSettings.speed = newSpeed >= -5 ? newSpeed : -10;
  g_fileSettings.speedCustom = newCustom;
  g_fileSettings.loops = newLoops;
  g_fileSettings.continuous = newCont;
  g_events.swap(tmp);
  return true;
}

void Engine_ApplyFileSettings() {
  if (g_fileSettings.speed >= -5) {
    g_cfg.speed = g_fileSettings.speed;
    g_cfg.speedCustom = g_fileSettings.speedCustom;
  }
  if (g_fileSettings.loops >= 0)
    g_cfg.loops = g_fileSettings.loops;
  if (g_fileSettings.continuous >= 0)
    g_cfg.continuous = g_fileSettings.continuous;
}

bool Engine_Load(const wchar_t *path) {
  std::string bytes;
  if (!json::ReadTextFile(path, bytes))
    return false;
  return Engine_FromJson(bytes);
}
