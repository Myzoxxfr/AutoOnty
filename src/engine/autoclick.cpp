#include "engine/autoclick.h"
#include "core/config.h"
#include "engine/engine.h"
#include <cstdlib>
#include "core/targeting.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

std::atomic<bool> g_acRunning{false};
std::atomic<unsigned> g_acClicks{0};
std::atomic<DWORD> g_acStartTick{0};
std::atomic<int> g_acCountdown{0};
std::vector<MAc> g_acImages;
bool Autoclick_HasSelectedImages() {
  for (size_t i = 0; i < g_acImages.size() && i < 64; ++i)
    if (g_cfg.acImageMask & (1ULL << i)) return true;
  return false;
}

static bool ValidAutoclickImages(const std::vector<MAc> &images) {
  if (images.size() > 64) return false;
  for (const auto &a : images)
    if (a.type != A_IMAGE || !a.image || !image::Dimensions(a.image->w, a.image->h) ||
        a.image->rgb.size() != size_t(a.image->w) * a.image->h ||
        a.sensitivity < 0 || a.sensitivity > 100 ||
        a.imageOptions.action != IA_AUTOCLICK || !a.imageOptions.interval ||
        a.imageOptions.target < IT_IMAGE || a.imageOptions.target > IT_FIXED || a.imageMacro)
      return false;
  return true;
}
bool Autoclick_LoadImages() {
  auto directory = ConfigDirectory();
  if (directory.empty()) return false;
  auto path = directory + L"ImageDetections\\AutoclickImages.onty";
  DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
    auto legacyDirectory = ExecutableDirectory();
    if (legacyDirectory.empty()) return false;
    auto legacy = legacyDirectory + L"AutoclickImages.onty";
    if (GetFileAttributesW(legacy.c_str()) == INVALID_FILE_ATTRIBUTES) {
      error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
      g_acImages.clear();
      return true;
    }
    std::vector<MAc> images;
    if (!Engine_ReadFile(legacy.c_str(), images) || !ValidAutoclickImages(images)) return false;
    return Autoclick_SaveImages(images);
  }
  std::vector<MAc> images;
  if (!Engine_ReadFile(path.c_str(), images, directory.c_str()) || !ValidAutoclickImages(images))
    return false;
  g_acImages = std::move(images);
  return true;
}
bool Autoclick_SaveImages(const std::vector<MAc> &images) {
  auto directory = ConfigDirectory();
  if (directory.empty() || !ValidAutoclickImages(images)) return false;
  auto folder = directory + L"ImageDetections";
  if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    return false;
  DWORD attributes = GetFileAttributesW(folder.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
  auto path = folder + L"\\AutoclickImages.onty";
  auto temporary = path + L".new";
  // Keep an empty saved list so old portable data is not imported again.
  if (!Engine_WriteFile(temporary.c_str(), images, directory.c_str()) ||
      !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(temporary.c_str());
    return false;
  }
  g_acImages = images;
  return true;
}

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
  bool jitter;
  int countdown;
  std::vector<MAc> images;
};
static AcRun g_run;
bool Autoclick_UsesImages() { return !g_run.images.empty(); }
bool Autoclick_ImageHideApp() {
  for (const auto &a : g_run.images)
    if (a.imageOptions.hideApp) return true;
  return false;
}
std::atomic<bool> g_acHeld{false};
static std::wstring acTargets;
const std::wstring &Autoclick_FocusTargets() { return acTargets; }
bool Autoclick_TargetReady() {
  return TargetHovered(acTargets) &&
         (!g_run.images.empty() || !g_run.fixed || TargetAtPoint(acTargets, g_run.pos));
}
static int positionRadius;
static int holdMods, holdVk;
void Autoclick_KeyReleased(DWORD vk) {
  if (g_acHeld.load() && HotkeyReleaseEnds(holdMods, holdVk, vk))
    g_acStop = true;
}
static bool StopRequested() { return g_acStop.load() || (g_acHeld.load() && !HotkeyHeld(holdMods, holdVk)); }

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

static bool SendClick(int button, bool dbl, const POINT *position = nullptr,
                      const POINT *restore = nullptr) {
  DWORD dn = MOUSEEVENTF_LEFTDOWN, up = MOUSEEVENTF_LEFTUP;
  if (button == 1) {
    dn = MOUSEEVENTF_RIGHTDOWN;
    up = MOUSEEVENTF_RIGHTUP;
  } else if (button == 2) {
    dn = MOUSEEVENTF_MIDDLEDOWN;
    up = MOUSEEVENTF_MIDDLEUP;
  } else if (button == 3 || button == 4) {
    dn = MOUSEEVENTF_XDOWN;
    up = MOUSEEVENTF_XUP;
  }
  INPUT in[6] = {};
  DWORD data = button == 3 ? XBUTTON1 : button == 4 ? XBUTTON2 : 0;
  UINT n = 0;
  RECT bounds = DesktopBounds();
  if (position) in[n++] = AbsoluteMouseMove(*position, bounds);
  for (int i = 0; i < (dbl ? 2 : 1); i++) {
    in[n].type = INPUT_MOUSE;
    in[n].mi.dwFlags = dn;
    in[n].mi.mouseData = data;
    n++;
    in[n].type = INPUT_MOUSE;
    in[n].mi.dwFlags = up;
    in[n].mi.mouseData = data;
    n++;
  }
  if (restore) in[n++] = AbsoluteMouseMove(*restore, bounds);
  UINT sent = SendInput(n, in, sizeof(INPUT));
  if (sent < n && sent) {
    INPUT cleanup[2]{};
    UINT count = 0;
    if (in[sent - 1].mi.dwFlags == dn) {
      cleanup[count].type = INPUT_MOUSE;
      cleanup[count].mi.dwFlags = up;
      cleanup[count++].mi.mouseData = data;
    }
    if (restore) cleanup[count++] = AbsoluteMouseMove(*restore, bounds);
    if (count) SendInput(count, cleanup, sizeof(INPUT));
  }
  return sent == n;
}

// Sleeps until `target` (NowMs() timeline). Returns false if a stop was
// requested meanwhile.
static bool WaitUntil(double target) {
  for (;;) {
    if (StopRequested() || PanicPressed())
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

static void RunImageAutoclick(const AcRun &r) {
  struct State {
    POINT point{};
    bool visible = false, finished = false, expired = false;
    double began = 0, next = 0;
    double waitBegan = NowMs();
    unsigned clicks = 0;
  };
  std::vector<State> states(r.images.size());
  double scanAt = 0;
  unsigned done = 0;
  g_imageStage = IS_SEARCHING;
  while (!StopRequested() && !PanicPressed()) {
    if (!Autoclick_TargetReady()) { Sleep(5); scanAt = 0; continue; }
    bool allExpired = true, unlimited = false;
    DWORD remaining = MAXDWORD;
    for (size_t i = 0; i < states.size(); ++i) {
      auto &s = states[i];
      DWORD timeout = r.images[i].imageTimeout;
      if (!s.visible && timeout && NowMs() - s.waitBegan >= timeout) s.expired = true;
      if (!s.expired) {
        allExpired = false;
        if (!s.visible) {
          if (!timeout) unlimited = true;
          else remaining = (std::min)(remaining, (DWORD)(std::max)(0.0, timeout - (NowMs() - s.waitBegan)));
        }
      }
    }
    g_imageRemaining = unlimited || remaining == MAXDWORD ? 0 : remaining;
    if (allExpired) { g_imageStage = IS_TIMED_OUT; return; }
    if (NowMs() >= scanAt) {
      int ox = 0, oy = 0;
      auto screen = image::Screen(ox, oy);
      if (!screen) {
        g_imageStage = IS_CAPTURE_FAILED;
        if (!WaitUntil(NowMs() + 150)) return;
        continue;
      }
      for (size_t i = 0; i < r.images.size(); ++i) {
        const auto &a = r.images[i];
        auto &s = states[i];
        if (s.expired) continue;
        auto checkpoint = [&] {
          if (!s.visible && a.imageTimeout && NowMs() - s.waitBegan >= a.imageTimeout)
            s.expired = true;
          return !s.expired && !StopRequested() && !PanicPressed() && Autoclick_TargetReady();
        };
        image::Match match;
        // Verify visible images in place before repeating a full desktop search.
        if (s.visible && !a.imageOptions.differentSizes) {
          int x = s.point.x - a.image->w / 2 - ox;
          int y = s.point.y - a.image->h / 2 - oy;
          if (x >= 0 && y >= 0 && x + a.image->w <= screen->w && y + a.image->h <= screen->h) {
            image::Comparison comparison;
            for (int py = 0; py < a.image->h; ++py) {
              if ((py & 15) == 0 && !checkpoint()) { match.cancelled = true; break; }
              for (int px = 0; px < a.image->w; ++px)
                comparison.Add(a.image->rgb[size_t(py) * a.image->w + px],
                               screen->rgb[size_t(y + py) * screen->w + x + px]);
            }
            match.score = comparison.Score() * 100;
            match.found = !match.cancelled && match.score + 1e-8 >= a.sensitivity;
            match.x = x; match.y = y;
          }
        }
        if (!match.found && !match.cancelled)
          match = image::Scan(*screen, *a.image, a.sensitivity,
                              a.imageOptions.differentSizes, checkpoint);
        if (match.cancelled) {
          if (s.expired) continue;
          scanAt = 0; break;
        }
        if (match.failed) { g_imageStage = IS_MATCH_FAILED; return; }
        g_imageScore = (int)std::floor(match.score + 1e-8);
        if (match.found) {
          s.point = {ox + match.x + (match.w ? match.w : a.image->w) / 2,
                     oy + match.y + (match.h ? match.h : a.image->h) / 2};
          if (!s.visible) {
            s.finished = false; s.began = 0; s.next = NowMs(); s.clicks = 0;
          }
        }
        if (s.visible && !match.found) s.waitBegan = NowMs();
        s.visible = match.found;
      }
      scanAt = NowMs() + 150;
    }
    bool clicking = false, limited = false;
    for (size_t i = 0; i < r.images.size(); ++i) {
      auto &s = states[i];
      const auto &o = r.images[i].imageOptions;
      if (!s.visible || s.expired) continue;
      if ((o.clicks && s.clicks >= o.clicks) ||
          (o.autoDuration && s.began && NowMs() - s.began >= o.autoDuration))
        s.finished = true;
      if (s.finished) { limited = true; continue; }
      clicking = true;
      if (NowMs() < s.next) continue;
      if (!Autoclick_TargetReady() || StopRequested() || PanicPressed()) break;
      POINT point = s.point;
      if (o.target == IT_CURSOR && !GetCursorPos(&point)) continue;
      if (o.target == IT_FIXED) point = {o.targetX, o.targetY};
      POINT center = point;
      if (positionRadius)
        point = JitterPoint(center, positionRadius, double(rand()) / RAND_MAX,
                            double(rand()) / RAND_MAX, DesktopBounds());
      if (OwnWindowAt(point) || OwnWindowAt(center) ||
          !TargetAtPoint(acTargets, point) || !TargetAtPoint(acTargets, center)) continue;
      if (!Autoclick_TargetReady() || StopRequested() || PanicPressed()) break;
      if (!SendClick(r.button, r.dbl, &point,
                     o.target == IT_CURSOR && positionRadius ? &center : nullptr)) {
        g_imageStage = IS_CLICK_FAILED;
        return;
      }
      if (!s.clicks) s.began = NowMs();
      ++s.clicks; ++g_acClicks;
      if (!r.untilStop && ++done >= r.count) return;
      double interval = o.interval;
      if (r.jitter) interval *= 0.8 + 0.4 * double(rand()) / RAND_MAX;
      s.next = NowMs() + (std::max)(1.0, interval);
    }
    if (StopRequested() || PanicPressed()) return;
    g_imageStage = clicking ? IS_CLICKING : limited ? IS_WAIT_CLEAR : IS_SEARCHING;
    Sleep(1);
  }
}

static DWORD WINAPI AcProc(LPVOID) {
  const AcRun r = g_run;
  srand(GetTickCount());
  timeBeginPeriod(1); // 1 ms timer resolution so short intervals stay accurate
  double countdownEnd = NowMs() + r.countdown * 1000.0;
  while (!StopRequested() && !PanicPressed() && g_acCountdown.load() > 0) {
    double left = countdownEnd - NowMs();
    g_acCountdown = left > 0 ? (int)((left + 999.0) / 1000.0) : 0;
    if (left > 0)
      Sleep(5);
  }
  g_acStartTick = GetTickCount();
  if (!StopRequested() && !PanicPressed() && !r.images.empty()) {
    RunImageAutoclick(r);
    timeEndPeriod(1);
    g_acCountdown = 0;
    g_acRunning = false;
    if (g_hwnd) PostMessageW(g_hwnd, WM_APP_AUTOCLICKDONE, 0, 0);
    return 0;
  }
  double next = NowMs();
  unsigned done = 0;
  while (!StopRequested() && !PanicPressed()) {
    if (!Autoclick_TargetReady()) { Sleep(5); next = NowMs(); continue; }
    POINT pt = r.pos;
    bool have = r.fixed || GetCursorPos(&pt);
    POINT center = pt;
    if (have && positionRadius) {
      RECT bounds = DesktopBounds();
      pt = JitterPoint(center, positionRadius, double(rand())/RAND_MAX, double(rand())/RAND_MAX, bounds);
    }
    // Hover may change during lookup. Target skips never consume a repeat.
    if (!Autoclick_TargetReady()) { next = NowMs(); continue; }
    if (StopRequested() || PanicPressed()) break;
    if (have && !OwnWindowAt(pt) && !OwnWindowAt(center)) {
      if (!Autoclick_TargetReady()) { next = NowMs(); continue; }
      // Jitter must not cross an application boundary and click another app.
      if (!TargetAtPoint(acTargets, pt)) { Sleep(5); next = NowMs(); continue; }
      if (SendClick(r.button, r.dbl, r.fixed || positionRadius ? &pt : nullptr,
                    !r.fixed && positionRadius ? &center : nullptr))
        g_acClicks++;
    }
    // Skipped targets consume finite-repeat attempts, not click statistics.
    if (!r.untilStop && ++done >= r.count)
      break;
    double step = r.intervalMs;
    if (r.jitter) {
      // +-20 % so the rhythm isn't perfectly regular (never below 1 ms)
      double k = 0.8 + 0.4 * ((double)rand() / (double)RAND_MAX);
      step = r.intervalMs * k;
      if (step < 1.0)
        step = 1.0;
    }
    next += step;
    double now = NowMs();
    if (next < now)
      next = now; // fell behind: don't fire a burst to catch up
    if (!WaitUntil(next))
      break;
  }
  timeEndPeriod(1);
  g_acCountdown = 0;
  g_acRunning = false;
  if (g_hwnd)
    PostMessageW(g_hwnd, WM_APP_AUTOCLICKDONE, 0, 0);
  return 0;
}

bool Autoclick_Start(bool held) {
  if (g_acRunning.load())
    return false;
  if (g_cfg.acImagesEnabled && (!Autoclick_HasSelectedImages() || !ValidAutoclickImages(g_acImages)))
    return false;
  if (g_acThread) {
    if (WaitForSingleObject(g_acThread, 3000) != WAIT_OBJECT_0)
      return false;
    CloseHandle(g_acThread);
    g_acThread = nullptr;
  }
  g_acHeld = held;
  holdMods = g_cfg.acMods; holdVk = g_cfg.acVk;
  acTargets = g_cfg.acTargets;
  positionRadius = g_cfg.acPositionJitter ? g_cfg.acRadius : 0;
  g_run.countdown =
      (g_cfg.acCountdown < 0 || g_cfg.acCountdown > 60) ? 0 : g_cfg.acCountdown;
  g_acCountdown = g_run.countdown;
  g_run.button = g_cfg.acButton;
  g_run.dbl = g_cfg.acDouble != 0;
  g_run.fixed = g_cfg.acMode == 1;
  g_run.pos.x = g_cfg.acX;
  g_run.pos.y = g_cfg.acY;
  g_run.intervalMs = (double)Autoclick_IntervalMs();
  g_run.untilStop = g_cfg.acRepeatUntilStop != 0;
  g_run.jitter = g_cfg.acJitter != 0;
  g_run.count = (unsigned)(g_cfg.acRepeatCount < 1 ? 1 : g_cfg.acRepeatCount);
  g_run.images.clear();
  if (g_cfg.acImagesEnabled)
    for (size_t i = 0; i < g_acImages.size(); ++i)
      if (g_cfg.acImageMask & (1ULL << i)) g_run.images.push_back(g_acImages[i]);
  g_imageStage = g_run.images.empty() ? IS_IDLE : IS_SEARCHING;
  g_imageMatches = g_imageScore = 0;
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
