#include "../src/core/targeting.h"
#include "../src/core/update.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <random>
#include <windows.h>
static std::atomic<int> focusOverride{-1};
static std::atomic<bool> focusRace{false};
static std::atomic<int> focusCalls{0};
static bool TestTargetFocused(const std::wstring &targets) {
  if (targets.empty()) return true;
  ++focusCalls;
  if (focusRace.exchange(false)) { focusOverride = 0; return true; }
  int value = focusOverride.load();
  return value < 0 ? TargetFocused(targets) : value != 0;
}
static std::atomic<int> hoverOverride{-1};
static std::atomic<bool> hoverRace{false}, pointAllowed{true};
static std::atomic<int> hoverCalls{0};
static std::atomic<bool> observeMissingScan{false}, missingScanArmed{false};
static std::atomic<bool> missingScanBlocked{false};
static std::atomic<int> missingScanChecks{0}, missingScanStage{-1};
static void ObserveAutoclickCheck();
static bool TestTargetHovered(const std::wstring &targets) {
  if (targets.empty()) return true;
  ObserveAutoclickCheck();
  ++hoverCalls;
  if (hoverRace.exchange(false)) { hoverOverride = 0; return true; }
  int value = hoverOverride.load();
  if (!value && missingScanStage.load() != -1) missingScanBlocked = true;
  return value < 0 ? TargetHovered(targets) : value != 0;
}
static bool TestTargetAtPoint(const std::wstring &targets, POINT) {
  return targets.empty() || pointAllowed.load();
}
static std::atomic<bool> mockPhysicalKeys{false};
static std::atomic<SHORT> physicalKeys[256];
static SHORT WINAPI TestKeyState(int vk) {
  return mockPhysicalKeys.load() ? physicalKeys[vk].load() : GetAsyncKeyState(vk);
}
#define TargetFocused TestTargetFocused
#define GetAsyncKeyState TestKeyState
static std::atomic<bool> heldKeys[256];
static std::atomic<bool> heldMouse[5];
static std::atomic<unsigned> downEvents{0};
static std::atomic<unsigned> cursorCountBeforeT{0}, leftButtonDowns{0};
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
  if (flags & MOUSEEVENTF_LEFTDOWN) {
    ++leftButtonDowns;
    heldMouse[0] = true;
  }
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
static std::atomic<int> lastCursorX{0}, lastCursorY{0};
static std::atomic<unsigned> screenCaptures{0};
static std::atomic<bool> twoImageLocations{false};
static std::atomic<bool> pauseImageCapture{false}, screenMatches{false},
    blurImageCapture{false};
static std::atomic<bool> failCursorMove{false}, stopOnCursorMove{false},
    blurOnCursorMove{false}, clampCursorMove{false}, failCursorRead{false};
static std::atomic<bool> failScreenSelection{false};
static std::atomic<unsigned> imageBlits{0};
static HGDIOBJ WINAPI ImageTestSelect(HDC dc, HGDIOBJ object) {
  return failScreenSelection ? nullptr : SelectObject(dc, object);
}
static void OnImageCapture();
static std::vector<uint32_t> scaledImageScreen;
static void OnImageCursorMove();
static int WINAPI ImageTestMetric(int metric) {
  switch (metric) {
  case SM_XVIRTUALSCREEN:
    return -200;
  case SM_YVIRTUALSCREEN:
    return -100;
  case SM_CXVIRTUALSCREEN:
    return 80;
  case SM_CYVIRTUALSCREEN:
    return 60;
  default:
    return GetSystemMetrics(metric);
  }
}
static BOOL WINAPI ImageTestBlit(HDC dc, int, int, int, int, HDC, int, int,
                                 DWORD) {
  ++imageBlits;
  DIBSECTION dib{};
  assert(GetObjectW(GetCurrentObject(dc, OBJ_BITMAP), sizeof(dib), &dib));
  if (!dib.dsBm.bmBits)
    return FALSE;
  auto pixels = (uint32_t *)dib.dsBm.bmBits;
  for (int i = 0; i < 80 * 60; ++i)
    pixels[i] = !twoImageLocations && screenMatches ?
        (scaledImageScreen.empty() ? 0x123456 : scaledImageScreen[i]) : 0xffffff;
  if (twoImageLocations) {
    pixels[5 * 80 + 5] = 0x123456;
    pixels[20 * 80 + 30] = 0x123456;
  }
  ++screenCaptures;
  OnImageCapture();
  return TRUE;
}
static BOOL WINAPI TestCursor(int x, int y) {
  lastCursorX = x + (clampCursorMove ? 1 : 0);
  lastCursorY = y;
  ++cursorPositions;
  OnImageCursorMove();
  return !failCursorMove.load();
}
static BOOL WINAPI ImageTestCursorPosition(LPPOINT point) {
  point->x = lastCursorX;
  point->y = lastCursorY;
  return !failCursorRead;
}
#define GetCursorPos ImageTestCursorPosition
#define SelectObject ImageTestSelect
#define GetSystemMetrics ImageTestMetric
#define BitBlt ImageTestBlit
#define keybd_event TestKeyboard
#define mouse_event TestMouse
#define SetCursorPos TestCursor
#include "../src/engine/engine.cpp"
#undef GetCursorPos
#undef SelectObject
#undef GetSystemMetrics
#undef BitBlt
#undef keybd_event
#undef mouse_event
#undef SetCursorPos
#undef TargetFocused
#undef GetAsyncKeyState
#include "../src/ui/editor_model.h"
static void OnImageCursorMove() {
  if (stopOnCursorMove.exchange(false))
    g_playStop = true;
  if (blurOnCursorMove.exchange(false))
    focusOverride = 0;
}
static void OnImageCapture() {
  if (observeMissingScan && !screenMatches) {
    missingScanChecks = 0;
    missingScanArmed = true;
  }
  if (blurImageCapture.exchange(false))
    focusOverride = 0;
  if (pauseImageCapture.exchange(false))
    g_pauseRequested = true;
}

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
#include "storage_paths.h"
#define GetModuleFileNameW TestModuleFileName
#define SHGetFolderPathW TestDocumentsFolder
#include "../src/core/config.cpp"
#undef GetModuleFileNameW
#undef SHGetFolderPathW

static std::wstring preferencesDir;
std::wstring TestPreferencesDir() { return preferencesDir.empty() ? preferencesDir : preferencesDir + L"\\"; }
#define IniDirectory TestPreferencesDir
#include "../src/core/editorprefs.cpp"
#undef IniDirectory

int Eff_Loops() { return g_cfg.loops; }
int Eff_Continuous() { return g_cfg.continuous; }
int Eff_Speed() { return g_cfg.speed; }
double Eff_SpeedCustom() { return g_cfg.speedCustom; }
void StatusShow(const wchar_t *, UINT) {}
const wchar_t *L(int) { return L""; }

// Autoclick tests never inject input or depend on desktop/window placement.
static std::atomic<unsigned> sentClicks{0};
static std::atomic<bool> externalClickTarget{false};
static std::atomic<LONG> cursorX{400}, cursorY{400};
static std::mutex batchMutex;
static std::vector<std::vector<INPUT>> inputBatches;
static std::atomic<int> sendLimit{-1};
static BOOL WINAPI AcTestCursor(LPPOINT point) {
  point->x = cursorX.load(); point->y = cursorY.load(); return TRUE;
}
static std::atomic<unsigned> targetChecks{0};
static HWND WINAPI TestWindowFromPoint(POINT) {
  ++targetChecks;
  return (HWND)1;
}
static DWORD WINAPI TestWindowProcess(HWND, LPDWORD pid) {
  *pid = externalClickTarget.load() ? GetCurrentProcessId()+1 : GetCurrentProcessId();
  return GetCurrentThreadId();
}
static UINT WINAPI TestSendInput(UINT n, LPINPUT inputs, int) {
  std::lock_guard<std::mutex> lock(batchMutex);
  inputBatches.emplace_back(inputs, inputs+n);
  sentClicks += n;
  int limit = sendLimit.exchange(-1);
  return limit < 0 ? n : (std::min)(n, (UINT)limit);
}
static std::atomic<bool> slowMissingImageSearch{false}, slowSearchEntered{false};
namespace image {
static Match AcTestScan(const Pixels &screen, const Pixels &snippet, int sensitivity,
                        bool sizes, const std::function<bool()> &checkpoint,
                        const std::vector<RECT> &excluded = {}, bool candidatesOnly = false) {
  if (slowMissingImageSearch && !candidatesOnly && screen.rgb[0] == 0xffffff) {
    slowSearchEntered = true;
    DWORD began = GetTickCount();
    while (GetTickCount() - began < 3000) {
      if (!checkpoint()) return {false, true};
      Sleep(1);
    }
  }
  return Scan(screen, snippet, sensitivity, sizes, checkpoint, excluded, candidatesOnly);
}
}
#define Scan AcTestScan
#define TargetFocused TestTargetFocused
#define GetAsyncKeyState TestKeyState
#define TargetHovered TestTargetHovered
#define TargetAtPoint TestTargetAtPoint
#define GetCursorPos AcTestCursor
#define NowMs AcNowMs
#define WindowFromPoint TestWindowFromPoint
#define GetWindowThreadProcessId TestWindowProcess
#define SendInput TestSendInput
#include "../src/engine/autoclick.cpp"
#undef Scan
#undef TargetFocused
#undef GetAsyncKeyState
#undef TargetHovered
#undef TargetAtPoint
#undef GetCursorPos
#undef NowMs
#undef WindowFromPoint
#undef GetWindowThreadProcessId
#undef SendInput

static void ObserveAutoclickCheck() {
  if (missingScanArmed && missingScanChecks.fetch_add(1) == 1) {
    hoverOverride = 0;
    missingScanStage = g_imageStage.load();
    missingScanArmed = false;
  }
}

static DWORD WINAPI SlowWorker(LPVOID duration) {
  Sleep((DWORD)(ULONG_PTR)duration);
  return 0;
}
static bool Event(const std::string &fields) {
  return Engine_FromJson("{\"onty\":1,\"events\":[{" + fields + "}]}");
}
static image::Match ReferenceMatch(const image::Pixels &screen,
                                   const image::Pixels &snippet,
                                   int sensitivity) {
  image::Match best;
  double bestScore = -1;
  for (int y = 0; y <= screen.h - snippet.h; ++y)
    for (int x = 0; x <= screen.w - snippet.w; ++x) {
      double tMean[3]{}, sMean[3]{};
      size_t n = snippet.rgb.size();
      for (int py = 0; py < snippet.h; ++py)
        for (int px = 0; px < snippet.w; ++px)
          for (int c = 0; c < 3; ++c) {
            tMean[c] +=
                (snippet.rgb[size_t(py) * snippet.w + px] >> (c * 8)) & 255;
            sMean[c] +=
                (screen.rgb[size_t(y + py) * screen.w + x + px] >> (c * 8)) &
                255;
          }
      for (int c = 0; c < 3; ++c) {
        tMean[c] /= n;
        sMean[c] /= n;
      }
      double tv = 0, sv = 0, cov = 0, error = 0;
      for (int py = 0; py < snippet.h; ++py)
        for (int px = 0; px < snippet.w; ++px)
          for (int c = 0; c < 3; ++c) {
            double t = (snippet.rgb[size_t(py) * snippet.w + px] >> (c * 8)) &
                       255,
                   s = (screen.rgb[size_t(y + py) * screen.w + x + px] >>
                        (c * 8)) &
                       255;
            tv += (t - tMean[c]) * (t - tMean[c]);
            sv += (s - sMean[c]) * (s - sMean[c]);
            cov += (t - tMean[c]) * (s - sMean[c]);
            error += abs(t - s);
          }
      double color = error / (765 * n),
             score = tv < .001 ? 1 - color
                     : sv < .001
                         ? 0
                         : (std::clamp)(cov / std::sqrt(tv * sv), 0.0, 1.0) *
                               (1 - .15 * color);
      score *= 100;
      if (score > bestScore) {
        bestScore = score;
        best = {score + 1e-8 >= sensitivity, false, x, y, score};
      }
    }
  return best;
}
static void MatcherDifferentialTests() {
  std::mt19937 random(17411);
  for (int iteration = 0; iteration < 400; ++iteration) {
    int sw = 8 + random() % 6, sh = 8 + random() % 5, nw = 1 + random() % 10,
        nh = 1 + random() % 10;
    image::Pixels screen{sw, sh, std::vector<uint32_t>(sw * sh)},
        snippet{nw, nh, std::vector<uint32_t>(nw * nh)};
    for (auto &pixel : screen.rgb)
      pixel = random();
    for (auto &pixel : snippet.rgb)
      pixel = random();
    // Include exact and noisy planted matches; several snippets exceed the 40
    // samples.
    if (nw <= sw && nh <= sh && iteration % 2 == 0) {
      int x = random() % (sw - nw + 1), y = random() % (sh - nh + 1);
      for (int sy = 0; sy < nh; ++sy)
        for (int sx = 0; sx < nw; ++sx)
          screen.rgb[(y + sy) * sw + x + sx] =
              snippet.rgb[sy * nw + sx] ^ (iteration % 4 ? 0x010203 : 0);
    }
    int sensitivities[] = {0, 70, 85, 95, 100};
    int sensitivity = sensitivities[iteration % 5];
    auto expected = ReferenceMatch(screen, snippet, sensitivity),
         actual = image::Find(screen, snippet, sensitivity);
    assert(actual.found == expected.found);
    if (actual.found && (actual.x != expected.x || actual.y != expected.y ||
                         abs(actual.score - expected.score) >= 1e-9))
      fprintf(stderr, "Matcher case %d (%dx%d): expected %d,%d %.12f; actual %d,%d %.12f\n",
              iteration, nw, nh, expected.x, expected.y, expected.score,
              actual.x, actual.y, actual.score);
    if (actual.found)
      assert(actual.x == expected.x && actual.y == expected.y &&
             abs(actual.score - expected.score) < 1e-9);
  }
}
static void ImagePlaybackTests() {
  MAc imageStep;
  imageStep.type = A_IMAGE;
  imageStep.image = std::make_shared<image::Pixels>(
      image::Pixels{2, 2, std::vector<uint32_t>(4, 0x123456)});
  imageStep.sensitivity = 100;
  imageStep.imageTimeout = 1000;
  MAc after;
  after.type = A_KD;
  after.vk = 'T';
  screenMatches = false;
  screenCaptures = 0;
  pauseImageCapture = true;
  auto positions = cursorPositions.load();
  assert(Engine_PlaySelection({imageStep, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  screenMatches = true;
  Engine_TogglePause();
  assert(Eventually([] { return g_state == ST_IDLE; }));
  assert(screenCaptures >= 2 && cursorPositions == positions + 1 &&
         lastCursorX == -199 && lastCursorY == -99);
  Engine_Shutdown();
  // A timeout skips the image and executes the next action, without a click.
  screenMatches = false;
  screenCaptures = 0;
  imageStep.imageTimeout = 40;
  positions = cursorPositions;
  auto keys = downEvents.load();
  assert(Engine_PlaySelection({imageStep, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  assert(cursorPositions == positions && downEvents == keys + 1 &&
         screenCaptures == 1);
  Engine_Shutdown();
  // Stop while an image scan is paused does not restore or click stale pixels.
  screenMatches = false;
  pauseImageCapture = true;
  imageStep.imageTimeout = 10000;
  positions = cursorPositions;
  keys = downEvents;
  assert(Engine_PlaySelection({imageStep, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  Engine_PlayStop();
  assert(Eventually([] { return g_state == ST_IDLE; }, 250));
  assert(cursorPositions == positions && downEvents == keys);
  Engine_Shutdown();
  // Stop during retry ends the run without executing its next action.
  imageStep.imageTimeout = 10000;
  screenCaptures = 0;
  keys = downEvents;
  assert(Engine_PlaySelection({imageStep, after}, 0));
  assert(Eventually([] { return screenCaptures.load() > 0; }));
  DWORD stop = GetTickCount();
  Engine_PlayStop();
  assert(Eventually([] { return g_state == ST_IDLE; }, 250));
  assert(GetTickCount() - stop < 250 && downEvents == keys);
  Engine_Shutdown();
}
static void ImageFocusTests() {
  MAc step;
  step.type = A_IMAGE;
  step.image = std::make_shared<image::Pixels>(
      image::Pixels{2, 2, std::vector<uint32_t>(4, 0x123456)});
  step.sensitivity = 100;
  step.imageTimeout = 80;
  MAc after;
  after.type = A_KD;
  after.vk = 'T';
  g_cfg.playTargets = L"game.exe;other.exe";
  mockPhysicalKeys = true;
  // A focus wait longer than the image timeout consumes no scan/timeout budget.
  focusOverride = 0;
  screenMatches = true;
  screenCaptures = 0;
  auto positions = cursorPositions.load();
  auto keys = downEvents.load();
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  Sleep(150);
  assert(screenCaptures == 0 && cursorPositions == positions &&
         downEvents == keys);
  focusOverride = 1;
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(screenCaptures == 1 && cursorPositions == positions + 1 &&
         downEvents == keys + 1);
  // Losing focus during a scan releases playback, preserves its target
  // snapshot, and starts with fresh pixels after BOTH focus and manual pause
  // are cleared.
  step.imageTimeout = 120;
  screenMatches = false;
  screenCaptures = 0;
  blurImageCapture = true;
  positions = cursorPositions;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  g_cfg.playTargets.clear();
  assert(Engine_FocusTargets() == L"game.exe;other.exe");
  Engine_TogglePause();
  focusOverride = 1;
  screenMatches = true;
  Sleep(200);
  assert(g_playPaused && screenCaptures == 1 && cursorPositions == positions &&
         downEvents == keys);
  Engine_TogglePause();
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(screenCaptures == 2 && cursorPositions == positions + 1 &&
         downEvents == keys + 1);
  assert(lastCursorX == -199 && lastCursorY == -99);
  // Stop/panic during image focus waits must never click or play the next key.
  g_cfg.playTargets = L"game.exe";
  focusOverride = 0;
  screenCaptures = 0;
  positions = cursorPositions;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  Engine_PlayStop();
  assert(Eventually([] { return g_state == ST_IDLE; }, 250));
  Engine_Shutdown();
  assert(screenCaptures == 0 && cursorPositions == positions &&
         downEvents == keys);
  auto panicVk = g_cfg.panicVk, panicMods = g_cfg.panicMods;
  g_cfg.panicVk = VK_PAUSE;
  g_cfg.panicMods = 0;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  physicalKeys[VK_PAUSE] = SHORT(0x8000);
  assert(Eventually([] { return g_state == ST_IDLE; }, 250));
  Engine_Shutdown();
  assert(screenCaptures == 0 && cursorPositions == positions &&
         downEvents == keys);
  physicalKeys[VK_PAUSE] = 0;
  mockPhysicalKeys = false;
  g_cfg.panicVk = panicVk;
  g_cfg.panicMods = panicMods;
  focusOverride = -1;
  g_cfg.playTargets.clear();
}
static void ImageClickGuardTests() {
  MAc step;
  step.type = A_IMAGE;
  step.sensitivity = 100;
  step.imageTimeout = 1000;
  step.image = std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  MAc after;
  after.type = A_KD;
  after.vk = 'T';
  screenMatches = true;
  g_cfg.playTargets.clear();
  // A failed SetCursorPos must never turn into a click at the old position.
  failCursorMove = true;
  auto clicks = leftButtonDowns.load(), keys = downEvents.load();
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks && downEvents == keys + 1);
  failCursorMove = false;
  // Windows can report a successful move but clamp it to a clipping rectangle.
  for (bool clamp : {true, false}) {
    clampCursorMove = clamp;
    failCursorRead = !clamp;
    clicks = leftButtonDowns;
    keys = downEvents;
    assert(Engine_PlaySelection({step, after}, 0));
    assert(Eventually([] { return g_state == ST_IDLE; }));
    Engine_Shutdown();
    assert(leftButtonDowns == clicks && downEvents == keys + 1);
  }
  clampCursorMove = false;
  failCursorRead = false;
  // Stop can arrive during the cursor move, after the last scan checkpoint.
  stopOnCursorMove = true;
  keys = downEvents;
  clicks = leftButtonDowns;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks && downEvents == keys);
  // The move itself can change focus: do not click until focus is allowed
  // again.
  g_cfg.playTargets = L"game.exe";
  focusOverride = 1;
  blurOnCursorMove = true;
  screenCaptures = 0;
  clicks = leftButtonDowns;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  assert(leftButtonDowns == clicks && downEvents == keys);
  focusOverride = 1;
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(screenCaptures == 2 && leftButtonDowns == clicks + 1 &&
         downEvents == keys + 1);
  g_cfg.playTargets.clear();
  focusOverride = -1;
}
static void WideDesktopTests() {
  image::Pixels screen{19200, 1, std::vector<uint32_t>(19200, 0xffffff)};
  image::Pixels snippet{1, 1, {0x123456}};
  screen.rgb.back() = snippet.rgb[0];
  auto result = image::Find(screen, snippet, 100);
  assert(result.found && result.x == 19199 && result.y == 0);
  assert(
      image::ScreenDimensions(7680, 4320)); // 8K exceeds the old 32M pixel cap.
  assert(image::ScreenDimensions(19200, 2160));
  assert(!image::ScreenDimensions(65536, 65536));
  int x, y;
  auto blits = imageBlits.load();
  failScreenSelection = true;
  assert(!image::Screen(x, y) && imageBlits == blits);
  failScreenSelection = false;
}
static void ImageAutomationTests() {
  MAc step;
  step.type = A_IMAGE;
  step.image = std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  step.sensitivity = 100;
  step.imageTimeout = 0;
  MAc after;
  after.type = A_KD;
  after.vk = 'T';
  screenMatches = true;
  step.imageOptions.action = IA_WAIT;
  auto clicks = leftButtonDowns.load(), keys = downEvents.load();
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks && downEvents == keys + 1);
  // No timeout really waits rather than finishing after a hidden default limit.
  screenMatches = false;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  Sleep(180);
  assert(g_state == ST_PLAY && downEvents == keys);
  Engine_PlayStop();
  Engine_Shutdown();
  // Both found and timed-out outcomes support manual pause and stop.
  screenMatches = true;
  step.imageOptions.onFound = IO_PAUSE;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  assert(g_imageStage == IS_PAUSE_FOUND);
  Engine_TogglePause();
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  step.imageOptions.onFound = IO_STOP;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(downEvents == keys);
  step.imageOptions.onFound = IO_CONTINUE;
  step.imageTimeout = 20;
  screenMatches = false;
  step.imageOptions.onTimeout = IO_PAUSE;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_playPaused.load(); }));
  assert(g_imageStage == IS_PAUSE_TIMEOUT);
  Engine_TogglePause();
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  step.imageOptions.onTimeout = IO_STOP;
  keys = downEvents;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(downEvents == keys);
  step.imageTimeout = 0;
  step.imageOptions.onTimeout = IO_CONTINUE;
  screenMatches = true;
  // Watchers own their finite repetition; the global continuous preference is
  // untouched.
  step.imageOptions.action = IA_CLICK;
  step.imageOptions.repeat = IR_REPEAT;
  step.imageOptions.maxMatches = 2;
  step.imageOptions.cooldown = 0;
  step.imageOptions.standalone = true;
  g_cfg.continuous = 1;
  clicks = leftButtonDowns;
  g_events = {step};
  assert(Engine_Play());
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks + 2 && g_playTotal == 1 &&
         g_cfg.continuous == 1);
  g_cfg.continuous = 0;
  // No repeated clicking while a reappearance watcher is waiting for
  // disappearance.
  step.imageOptions.repeat = IR_REAPPEAR;
  clicks = leftButtonDowns;
  assert(Engine_PlaySelection({step}, 0));
  assert(Eventually([] { return g_imageStage == IS_WAIT_CLEAR; }));
  Sleep(180);
  assert(leftButtonDowns == clicks + 1);
  screenMatches = false;
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  screenMatches = true;
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks + 2);
  // Excluding a seen location lets an identical image elsewhere be found.
  twoImageLocations = true;
  step.imageOptions.repeat = IR_LOCATIONS;
  clicks = leftButtonDowns;
  assert(Engine_PlaySelection({step}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks + 2 && lastCursorX == -170 &&
         lastCursorY == -80);
  twoImageLocations = false;
  // Auto clicking is an action of the image step; it does not start the
  // separate autoclick worker.
  step.imageOptions.repeat = IR_ONCE;
  step.imageOptions.action = IA_AUTOCLICK;
  step.imageOptions.clicks = 3;
  step.imageOptions.interval = 5;
  clicks = leftButtonDowns;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(leftButtonDowns == clicks + 3 && !g_acRunning);
  step.imageOptions.clicks = 0;
  clicks = leftButtonDowns;
  assert(Engine_PlaySelection({step, after}, 0));
  assert(Eventually([&] { return leftButtonDowns >= clicks + 2; }));
  screenMatches = false;
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  // A long cooldown cannot overrun a short total watch duration.
  screenMatches = true;
  step.imageOptions.action = IA_WAIT;
  step.imageOptions.repeat = IR_REPEAT;
  step.imageOptions.maxMatches = 0;
  step.imageOptions.duration = 50;
  step.imageOptions.cooldown = 9999999;
  assert(Engine_PlaySelection({step}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }, 300));
  Engine_Shutdown();
  // Nested macros execute once on the same worker and release/restore parent
  // holds.
  step.imageOptions = ImageOptions{};
  step.imageOptions.action = IA_RUN_MACRO;
  MAc child;
  child.type = A_KD;
  child.vk = 'A';
  step.imageMacro =
      std::make_shared<const std::vector<MAc>>(std::vector<MAc>{child});
  MAc down;
  down.type = A_KD;
  down.vk = 'Q';
  MAc up = down;
  up.type = A_KU;
  keys = downEvents;
  assert(Engine_PlaySelection({down, step, up}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(downEvents == keys + 3 && !heldKeys['A'] && !heldKeys['Q']);
  // Preview exclusions filter copied inputs, rather than skipping a wait-only
  // condition.
  PreviewExclusions excluded;
  excluded.keys['A'] = true;
  excluded.movement = true;
  std::vector<MAc> hidden;
  auto rows = macroedit::FromEvents({step}, hidden, EV_ALL);
  std::vector<int> selected;
  for (size_t i = 0; i < rows.size(); ++i)
    selected.push_back((int)i);
  auto filtered = macroedit::Selection(rows, selected, excluded);
  assert(filtered.size() == 1 && filtered[0].imageMacro &&
         filtered[0].imageMacro->empty());
  MAc wait = step;
  wait.imageOptions.action = IA_WAIT;
  assert(!excluded.Blocked(wait));
  // Editing and portable save/load retain all choices, including large/zero
  // timeouts and nested snapshots.
  step.imageFile = image::Name(*step.image);
  step.imageTimeout = 9999999;
  step.imageOptions.repeat = IR_LOCATIONS;
  step.imageOptions.target = IT_FIXED;
  step.imageOptions.targetX = -500;
  step.imageOptions.targetY = 200;
  step.imageMacroName = L"Quoted \"macro\" 日本語";
  g_events = {step};
  std::string encoded;
  assert(Engine_ToJson(encoded));
  assert(Engine_FromJson(encoded));
  assert(g_events[0].imageTimeout == 9999999 &&
         g_events[0].imageMacroName == step.imageMacroName &&
         g_events[0].imageMacro->at(0).vk == 'A' &&
         g_events[0].imageOptions.targetX == -500);
  for (auto fields :
       {"\"action\":4", "\"target\":3", "\"repeat\":4", "\"after_found\":3",
        "\"interval\":0", "\"watch_time\":4294967296"})
    assert(
        !Event(std::string("\"e\":\"image\",\"image\":\"x.bmp\",") + fields));
  g_events.clear();
}
static void ImageTests() {
  ImageAutomationTests();
  ImageClickGuardTests();
  WideDesktopTests();
  MatcherDifferentialTests();
  ImagePlaybackTests();
  ImageFocusTests();
  // Clipboard DIB: bottom-up 24-bit rows, padding, and alpha-independent
  // 32-bit.
  std::vector<BYTE> dib(48);
  auto &header = *(BITMAPINFOHEADER *)dib.data();
  header = {40, 1, 2, 1, 24, BI_RGB};
  dib[40] = 3;
  dib[41] = 2;
  dib[42] = 1;
  dib[44] = 6;
  dib[45] = 5;
  dib[46] = 4;
  auto decoded = image::Dib(dib.data(), dib.size());
  assert(decoded && decoded->rgb[0] == 0x040506 && decoded->rgb[1] == 0x010203);
  assert(!image::Dib(nullptr, 48));
  assert(!image::Dib(dib.data(), 47));
  header.biHeight = LONG_MIN;
  assert(!image::Dib(dib.data(), 48));
  assert(!image::SafeName(L"CON.bmp") && !image::SafeName(L"C:\\x.bmp"));
  image::Pixels alphaScreen{1, 1, {0xff123456}},
      alphaSnippet{1, 1, {0x00123456}};
  assert(image::Find(alphaScreen, alphaSnippet, 100).score == 100);
  image::Pixels screen{4,
                       2,
                       {0x111111, 0x121212, 0xffffff, 0xffffff, 0x111111,
                        0x111111, 0xffffff, 0xffffff}};
  image::Pixels snippet{2, 1, {0x111111, 0x111111}};
  auto best = image::Find(screen, snippet, 85);
  assert(best.found && best.x == 0 && best.y == 1 && best.score == 100);
  screen.rgb[4] = 0x121212;
  best = image::Find(screen, snippet, 85);
  assert(best.found && best.score < 100);
  assert(!image::Find(screen, snippet, 100).found);
  image::Pixels large{5, 1, std::vector<uint32_t>(5)};
  assert(!image::Find(screen, large, 85).found);
  assert(image::Find(screen, snippet, 0).found);
  MAc a;
  a.type = A_IMAGE;
  a.image = std::make_shared<image::Pixels>(snippet);
  a.imageFile = image::Name(snippet);
  a.sensitivity = 70;
  a.imageTimeout = 1234;
  g_events = {a};
  std::string saved;
  assert(Engine_ToJson(saved));
  assert(Engine_FromJson(saved));
  assert(g_events.size() == 1 && g_events[0].type == A_IMAGE &&
         g_events[0].sensitivity == 70 && g_events[0].imageTimeout == 1234);
  for (
      auto bad :
      {R"({"onty":1,"events":[{"e":"image","image":"../x.bmp"}]})",
       R"({"onty":1,"events":[{"e":"image","image":"x.bmp","sensitivity":101}]})",
       R"({"onty":1,"events":[{"e":"image","image":"x.bmp","timeout":4294967296}]})",
       R"({"onty":1,"events":[{"e":"image"}]})"})
    assert(!Engine_FromJson(bad));
  wchar_t temp[MAX_PATH];
  GetTempPathW(MAX_PATH, temp);
  std::wstring path = std::wstring(temp) + L"AutoOnty-image-test.onty";
  g_events = {a};
  assert(Engine_Save(path.c_str()));
  g_events.clear();
  assert(Engine_Load(path.c_str()));
  assert(g_events[0].image->rgb == snippet.rgb);
  assert(a.imageFile.starts_with(L"ImageDetections/"));
  assert(image::SafeName(a.imageFile));
  assert(!image::SafeName(L"ImageDetections/../x.bmp"));
  assert(!image::SafeName(L"ImageDetections/sub/x.bmp"));
  assert(!image::SafeName(L"OtherFolder/x.bmp"));
  // Existing macros with BMP files beside the .onty continue to load.
  const auto legacyName = a.imageFile.substr(16);
  const auto legacyPath = image::Directory(path.c_str()) + legacyName;
  assert(CopyFileW((image::Directory(path.c_str()) + a.imageFile).c_str(),
                   legacyPath.c_str(), FALSE));
  std::string legacyJson;
  assert(Engine_ToJson(legacyJson));
  auto prefix = legacyJson.find("ImageDetections/");
  assert(prefix != std::string::npos);
  legacyJson.erase(prefix, 16);
  assert(Engine_FromJson(legacyJson, image::Directory(path.c_str()).c_str()));
  assert(g_events[0].image->rgb == snippet.rgb);
  DeleteFileW(legacyPath.c_str());
  // A standard 40-byte BI_BITFIELDS header stores masks before the pixels.
  std::vector<BYTE> bitmap(70);
  BITMAPFILEHEADER file{};
  file.bfType = 0x4d42;
  file.bfOffBits = 66;
  file.bfSize = 70;
  BITMAPINFOHEADER info{40, 1, -1, 1, 32, BI_BITFIELDS};
  DWORD masks[] = {0xff0000, 0xff00, 0xff}, color = 0x123456;
  memcpy(bitmap.data(), &file, 14);
  memcpy(bitmap.data() + 14, &info, 40);
  memcpy(bitmap.data() + 54, masks, 12);
  memcpy(bitmap.data() + 66, &color, 4);
  auto bitfieldPath = path + L".bmp";
  HANDLE binary = CreateFileW(bitfieldPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, 0, nullptr);
  assert(binary != INVALID_HANDLE_VALUE);
  DWORD wrote;
  assert(
      WriteFile(binary, bitmap.data(), DWORD(bitmap.size()), &wrote, nullptr) &&
      wrote == bitmap.size());
  CloseHandle(binary);
  auto decodedBitmap = image::Load(bitfieldPath);
  assert(decodedBitmap && decodedBitmap->rgb[0] == color);
  DeleteFileW(bitfieldPath.c_str());
  auto bmp = image::Directory(path.c_str()) + a.imageFile;
  image::Pixels invalid{2, 2, {1}};
  assert(!image::Save(bmp, invalid));
  assert(image::Load(bmp)->rgb == snippet.rgb);
  // Force publication to fail: a reader denies delete/replace sharing.
  HANDLE reader = CreateFileW(bmp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
  assert(reader != INVALID_HANDLE_VALUE);
  assert(!image::Save(bmp, snippet));
  CloseHandle(reader);
  assert(image::Load(bmp)->rgb == snippet.rgb);
  g_events.assign(20, a);
  assert(Engine_Save(path.c_str()));
  assert(Engine_Load(path.c_str()));
  for (auto &event : g_events)
    assert(event.image == g_events[0].image);

  MAc trigger = a;
  trigger.imageOptions.action = IA_RUN_MACRO;
  trigger.imageMacroName = L"Nested action";
  trigger.imageMacro =
      std::make_shared<const std::vector<MAc>>(std::vector<MAc>{a});
  g_events = {trigger};
  assert(Engine_Save(path.c_str()));
  assert(Engine_Load(path.c_str()) && g_events[0].imageMacro &&
         g_events[0].imageMacro->at(0).image == g_events[0].image);
  auto currentSettings = g_fileSettings;
  std::vector<MAc> imported;
  assert(Engine_ReadFile(path.c_str(), imported) && imported.size() == 1 &&
         g_events.size() == 1 &&
         g_events[0].imageMacroName == L"Nested action");
  assert(g_fileSettings.speed == currentSettings.speed &&
         g_fileSettings.loops == currentSettings.loops);
  DeleteFileW((image::Directory(path.c_str()) + a.imageFile).c_str());
  assert(!Engine_Load(path.c_str()));
  assert(g_events[0].image);
  DeleteFileW(path.c_str());
  g_events.clear();
  image::Pixels huge{100, 100, std::vector<uint32_t>(10000, 0)};
  int checks = 0;
  auto cancelled = image::Find(huge, snippet, 100, [&] {
    ++checks;
    return false;
  });
  assert(cancelled.cancelled && checks);
}
static void AutoclickImageTests() {
  Config previous = g_cfg;
  g_cfg.acImagesEnabled = 1;
  g_cfg.acImageMask = 3;
  g_acImages.clear();
  assert(!Autoclick_Start());
  MAc a;
  a.type = A_IMAGE;
  a.image = std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  a.imageOptions.action = IA_AUTOCLICK;
  a.imageOptions.interval = 5;
  a.imageOptions.clicks = 2;
  MAc b = a;
  b.imageOptions.interval = 20;
  b.imageOptions.clicks = 3;
  g_acImages = {a, b};
  g_cfg.acRepeatUntilStop = 1;
  g_cfg.acCountdown = 0;
  g_cfg.acButton = 3;
  g_cfg.acDouble = g_cfg.acPositionJitter = 0;
  g_cfg.acTargets.clear();
  externalClickTarget = true;
  screenMatches = false;
  auto loadedMacro = g_events;
  inputBatches.clear();
  assert(Autoclick_Start());
  assert(Autoclick_UsesImages());
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  Sleep(30); assert(g_acClicks == 0);
  screenMatches = true;
  assert(Eventually([] { return g_acClicks == 5 && g_imageStage == IS_WAIT_CLEAR; }));
  Sleep(200); assert(g_acClicks == 5);
  g_acImages.clear(); // The run owns a snapshot of both image rules.
  g_cfg.acImageMask = 0;
  g_cfg.acButton = 0;
  screenMatches = false;
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  screenMatches = true;
  assert(Eventually([] { return g_acClicks == 10; }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_events.size() == loadedMacro.size());
  for (const auto &batch : inputBatches) {
    assert(batch.size() == 3 && batch[1].mi.dwFlags == MOUSEEVENTF_XDOWN &&
           batch[1].mi.mouseData == XBUTTON1 && batch[2].mi.dwFlags == MOUSEEVENTF_XUP);
  }
  // Duration is per appearance and still works with an unlimited click count.
  g_cfg.acImageMask = 1;
  a.imageOptions.clicks = 0;
  a.imageOptions.autoDuration = 30;
  a.imageOptions.interval = 100;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return g_imageStage == IS_WAIT_CLEAR; }));
  assert(g_acClicks == 1);
  Sleep(180); assert(g_acClicks == 1);
  Autoclick_StopAndWait(INFINITE);
  // The global repeat limit applies across the image list.
  g_cfg.acRepeatUntilStop = 0; g_cfg.acRepeatCount = 3;
  a.imageOptions.autoDuration = 0;
  a.imageOptions.interval = 5;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 3);
  // Only picked rules run; an empty choice never starts ordinary clicking.
  a.imageOptions.clicks = 1;
  b.imageOptions.clicks = 3;
  g_acImages = {a, b};
  g_cfg.acRepeatUntilStop = 1;
  g_cfg.acImageMask = 2;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acClicks == 3 && g_imageStage == IS_WAIT_CLEAR; }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_run.images.size() == 1 && g_run.images[0].imageOptions.clicks == 3);
  g_cfg.acImageMask = 0;
  assert(!Autoclick_Start());
  // The 64th image uses the high bit without shifting outside the mask.
  g_acImages.assign(64, a);
  g_cfg.acImageMask = 1ULL << 63;
  g_cfg.acRepeatUntilStop = 0; g_cfg.acRepeatCount = 1;
  assert(Autoclick_Start());
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 1 && g_run.images.size() == 1);
  // The same hold/release path and double/X2 settings apply in image mode.
  g_acImages = {a}; g_cfg.acImageMask = 1;
  g_cfg.acRepeatUntilStop = 1;
  g_cfg.acButton = 4; g_cfg.acDouble = 1;
  g_cfg.acMods = HK_CTRL; g_cfg.acVk = 'A';
  mockPhysicalKeys = true;
  physicalKeys['A'] = physicalKeys[VK_CONTROL] = physicalKeys[VK_LCONTROL] = SHORT(0x8000);
  inputBatches.clear();
  assert(Autoclick_Start(true));
  assert(Eventually([] { return g_acClicks == 1; }));
  Autoclick_KeyReleased('A');
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(inputBatches.size() == 1 && inputBatches[0].size() == 5);
  assert(inputBatches[0][1].mi.dwFlags == MOUSEEVENTF_XDOWN &&
         inputBatches[0][1].mi.mouseData == XBUTTON2 &&
         inputBatches[0][3].mi.dwFlags == MOUSEEVENTF_XDOWN);
  physicalKeys['A'] = physicalKeys[VK_CONTROL] = physicalKeys[VK_LCONTROL] = 0;
  mockPhysicalKeys = false;
  a.imageOptions.clicks = 0;
  g_acImages = {a}; g_cfg.acDouble = 0;
  // Focus/hover gating and panic remain live during image searching.
  g_cfg.acRepeatUntilStop = 1;
  g_cfg.acTargets = L"game.exe";
  hoverOverride = 0;
  assert(Autoclick_Start());
  Sleep(30); assert(g_acClicks == 0);
  hoverOverride = 1;
  assert(Eventually([] { return g_acClicks > 0; }));
  Autoclick_StopAndWait(INFINITE);
  mockPhysicalKeys = true;
  physicalKeys[VK_PAUSE] = 0;
  screenMatches = false;
  g_cfg.panicMods = 0; g_cfg.panicVk = VK_PAUSE;
  assert(Autoclick_Start());
  physicalKeys[VK_PAUSE] = SHORT(0x8000);
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 0);
  physicalKeys[VK_PAUSE] = 0; mockPhysicalKeys = false;
  g_cfg.acTargets.clear();
  a.imageTimeout = 25;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 0);
  // A missing rule can expire while another rule keeps waiting indefinitely.
  b.imageTimeout = 0;
  g_acImages = {a, b}; g_cfg.acImageMask = 3;
  assert(Autoclick_Start());
  Sleep(100);
  assert(g_acRunning && g_acClicks == 0);
  externalClickTarget = true;
  screenMatches = true;
  assert(Eventually([] { return g_acClicks == 3; }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 3);
  screenMatches = false;
  // After-match actions run after the requested clicks, and resume searches again.
  a.imageTimeout = 0;
  a.imageOptions.clicks = 3;
  a.imageOptions.interval = 5;
  a.imageOptions.onFound = IO_PAUSE;
  g_cfg.acImageMask = 1;
  g_acImages = {a};
  screenMatches = true;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acPaused.load(); }));
  assert(g_acRunning && g_acClicks == 3 && g_imageMatches == 1);
  Sleep(80);
  assert(g_acClicks == 3);
  screenMatches = false;
  Autoclick_TogglePause();
  Sleep(180);
  assert(g_acRunning && !g_acPaused && g_acClicks == 3);
  screenMatches = true;
  assert(Eventually([] { return g_acPaused.load() && g_acClicks == 6; }));
  Autoclick_TogglePause();
  assert(Eventually([] { return g_acPaused.load() && g_acClicks == 9; }));
  Autoclick_StopAndWait(INFINITE);
  assert(!g_acRunning && !g_acPaused);
  a.imageOptions.onFound = IO_STOP;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 3);
  // Unlimited clicking finishes its match when the image disappears.
  a.imageOptions.clicks = 0;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acClicks > 0; }));
  screenMatches = false;
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  screenMatches = false;
  a.imageOptions.onFound = IO_CONTINUE;
  a.imageOptions.clicks = 1;
  a.imageOptions.maxMatches = 0;
  g_cfg.acTargets = L"game.exe";
  hoverOverride = 1;
  g_acImages = {a};
  screenMatches = true;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_imageStage == IS_WAIT_CLEAR; }));
  missingScanStage = -1;
  missingScanBlocked = false;
  observeMissingScan = true;
  screenMatches = false;
  assert(Eventually([] { return missingScanBlocked.load(); }));
  assert(missingScanStage == IS_SEARCHING && g_imageStage == IS_SEARCHING && g_imageScore == 0);
  observeMissingScan = false;
  screenMatches = true;
  hoverOverride = 1;
  assert(Eventually([] { return g_acClicks == 2 && g_imageStage == IS_WAIT_CLEAR; }));
  for (int cycle = 3; cycle <= 5; ++cycle) {
    screenMatches = false;
    assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
    screenMatches = true;
    assert(Eventually([&] { return g_acClicks == cycle && g_imageStage == IS_WAIT_CLEAR; }));
  }
  Autoclick_StopAndWait(INFINITE);
  g_cfg.acTargets.clear();
  // A slow exhaustive search of an absent frame must see a reappearance before that search completes.
  a.imageOptions.clicks = 0;
  g_acImages = {a};
  screenMatches = true;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acClicks > 0; }));
  slowSearchEntered = false;
  slowMissingImageSearch = true;
  screenMatches = false;
  assert(Eventually([] { return slowSearchEntered.load(); }));
  auto clicksBeforeReturn = g_acClicks.load();
  auto framesBeforeReturn = screenCaptures.load();
  screenMatches = true;
  assert(Eventually([&] { return g_acClicks > clicksBeforeReturn; }, 1000));
  assert(screenCaptures > framesBeforeReturn);
  Autoclick_StopAndWait(INFINITE);
  slowMissingImageSearch = false;
  a.imageOptions.clicks = 1;
  // Re-arm using the matched size, rather than verifying a scaled image with the original pixels.
  a.imageOptions.differentSizes = true;
  auto originalImage = a.image;
  a.image = std::make_shared<image::Pixels>(image::Pixels{4, 4, {
      0x152739, 0xab3d1f, 0x2288cc, 0x5522aa, 0x983313, 0x017465, 0xee3322, 0x66aabb,
      0x992277, 0x448822, 0x112244, 0xddcc99, 0xff8811, 0x2277bb, 0x779933, 0x6633aa}});
  auto enlarged = image::ResizeTo(*a.image, 8, 8);
  scaledImageScreen.assign(80 * 60, 0xffffff);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) scaledImageScreen[(y + 10) * 80 + x + 20] = enlarged.rgb[y * 8 + x];
  a.sensitivity = 100;
  g_acImages = {a};
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acClicks == 1 && g_imageStage == IS_WAIT_CLEAR; }));
  Sleep(700);
  assert(g_acClicks == 1);
  screenMatches = false;
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  screenMatches = true;
  assert(Eventually([] { return g_acClicks == 2 && g_imageStage == IS_WAIT_CLEAR; }));
  Autoclick_StopAndWait(INFINITE);
  scaledImageScreen.clear();
  a.image = originalImage;
  a.sensitivity = 85;
  a.imageOptions.differentSizes = false;
  a.imageOptions.onFound = IO_CONTINUE;
  a.imageOptions.clicks = 1;
  a.imageOptions.maxMatches = 2;
  a.imageOptions.cooldown = 600;
  g_acImages = {a};
  screenMatches = true;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acClicks == 1; }));
  DWORD firstAppearance = GetTickCount();
  screenMatches = false;
  assert(Eventually([] { return g_imageStage == IS_SEARCHING; }));
  screenMatches = true;
  assert(Eventually([] { return !g_acRunning.load(); }, 1800));
  assert(g_acClicks == 2 && GetTickCount() - firstAppearance >= 500);
  Autoclick_StopAndWait(INFINITE);
  screenMatches = false;
  hoverOverride = -1; externalClickTarget = false;
  g_acImages.clear(); g_imageStage = IS_IDLE;
  g_cfg = previous;
  inputBatches.clear(); sentClicks = 0;
}
static void AdditionalRegressionTests() {
  image::Pixels snippet{64, 64, {}};
  std::mt19937 random(123);
  for (int i = 0; i < 4096; ++i) snippet.rgb.push_back(random() & 0xffffff);
  auto changed = snippet;
  for (size_t i : {898, 1132, 677, 3075, 3863, 2748, 3705, 1547})
    changed.rgb[i] ^= 0xffffff;
  for (int threshold : {0, 70, 85, 95, 99, 100}) {
    auto expected = ReferenceMatch(changed, snippet, threshold);
    auto actual = image::Find(changed, snippet, threshold);
    assert(actual.found == expected.found);
    if (actual.found) assert(abs(actual.score - expected.score) < 1e-9);
  }
  assert(image::Find(changed, snippet, 85).score > 99);

  assert(!image::Find(changed, snippet, 100).found);
  auto below = image::Find(changed, snippet, 0);
  assert(below.score > 99);
  image::Pixels desktop{137, 91, std::vector<uint32_t>(137 * 91)};
  for (auto &pixel : desktop.rgb) pixel = random() & 0xffffff;
  for (int y = 0; y < changed.h; ++y)
    for (int x = 0; x < changed.w; ++x)
      desktop.rgb[(y + 12) * desktop.w + x + 51] = changed.rgb[y * changed.w + x];
  assert(!image::Find(desktop, snippet, 100).found);
  below = image::Find(desktop, snippet, 0);
  assert(below.score > 99);
  auto detected = image::Find(desktop, snippet, 85);
  assert(detected.found && detected.x == 51 && detected.y == 12);
  // Scalar row tails and SIMD batches must agree with the full reference.
  for (int width : {7, 9, 15, 17, 31}) {
    image::Pixels pattern{width, 9, std::vector<uint32_t>(width * 9)};
    image::Pixels screen{width + 8, 15, std::vector<uint32_t>((width + 8) * 15)};
    for (auto &pixel : pattern.rgb) pixel = random() & 0xffffff;
    for (auto &pixel : screen.rgb) pixel = random() & 0xffffff;
    for (int y = 0; y < pattern.h; ++y)
      for (int x = 0; x < pattern.w; ++x)
        screen.rgb[(y + 3) * screen.w + x + 5] = pattern.rgb[y * pattern.w + x] ^ 0x010203;
    auto expected = ReferenceMatch(screen, pattern, 85), actual = image::Find(screen, pattern, 85);
    assert(actual.found && actual.x == expected.x && actual.y == expected.y &&
           std::abs(actual.score - expected.score) < 1e-9);
  }
  MAc watcher;
  watcher.type = A_IMAGE;
  watcher.image = std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  watcher.imageTimeout = 20;
  watcher.sensitivity = 100;
  watcher.imageOptions.action = IA_AUTOCLICK;
  watcher.imageOptions.clicks = 1;
  screenMatches = false; twoImageLocations = false;
  g_cfg.playTargets.clear(); g_cfg.panicVk = 0;
  assert(Engine_PlaySelection({watcher}, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  watcher.imageTimeout = 0;
  assert(Engine_PlaySelection({watcher}, 0));
  Sleep(100);
  assert(g_state == ST_PLAY && g_imageRemaining == 0);
  screenMatches = true;
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();

  auto cancelled = image::Find(changed, snippet, 85, [] { return false; });
  assert(cancelled.cancelled && !cancelled.found);

  const auto ini = IniPath();
  assert(json::WriteTextFile(ini.c_str(), "[AutoOnty]\r\nspeed=5\r\n"));
  g_cfg.speed = 3;
  HANDLE lock = CreateFileW(ini.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  assert(lock != INVALID_HANDLE_VALUE);
  assert(!ConfigLoad() && g_cfg.speed == 3);
  CloseHandle(lock);
  assert(ConfigLoad() && g_cfg.speed == 5);

  g_cfg.recordVk = g_cfg.playVk = g_cfg.acVk = g_cfg.acPosVk = g_cfg.pauseVk = 0;
  g_cfg.panicVk = 0;
  g_cfg.recLeft = g_cfg.recRight = g_cfg.recMiddle = g_cfg.recX1 = g_cfg.recX2 = 1;
  g_cfg.recWheelUp = g_cfg.recWheelDown = 1;
  mockPhysicalKeys = true;
  g_state = ST_REC; g_stopPending = false;
  ClearPendingMoves();
  g_events.clear();
  memset(g_keysDown, 0, sizeof(g_keysDown));
  memset(g_recPressed, 0, sizeof(g_recPressed));
  g_lastMs = NowMs() - 120000;
  KBDLLHOOKSTRUCT key{}; key.vkCode = 'Q';
  RecKbProc(HC_ACTION, WM_KEYDOWN, (LPARAM)&key);
  assert(g_events.size() == 1 && g_events[0].delay >= 120000);
  for (UINT message : {WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN,
                       WM_RBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP,
                       WM_XBUTTONDOWN, WM_XBUTTONUP, WM_MOUSEWHEEL}) {
    g_events.clear();
    g_lastMs = NowMs() - 120000;
    MSLLHOOKSTRUCT mouse{};
    mouse.pt = {-100, -100};
    mouse.mouseData = DWORD(message == WM_MOUSEWHEEL ? 120 : XBUTTON1) << 16;
    RecMsProc(HC_ACTION, message, (LPARAM)&mouse);
    assert(g_events.size() == 1 && g_events[0].delay >= 120000);
  }
  g_pendingRel = {{1, 2}, {3, 4}};
  g_pendingRelD = {120000, 200000};
  MAc movement; movement.type = A_MOVE;
  assert(TakePendingMoves(movement));
  unsigned long long duration = 0;
  for (WORD delay : movement.pdelays) duration += delay;
  assert(duration == 320000 && movement.path.size() == 6);
  assert(movement.path[0] == std::make_pair(WORD(0), WORD(0)));
  assert(movement.path.back() == std::make_pair(WORD(3), WORD(4)));
  g_state = ST_IDLE; mockPhysicalKeys = false;
  g_cfg.speed = 1; g_cfg.loops = 1; g_cfg.continuous = 0;
  g_events = {movement};
  std::string saved;
  assert(Engine_ToJson(saved) && Engine_FromJson(saved));
  assert(g_events[0].pdelays == movement.pdelays);

  const auto file = ConfigDirectory() + L"speed-validation.onty";
  assert(Engine_Save(file.c_str()));
  std::string original;
  assert(json::ReadTextFile(file.c_str(), original));
  g_cfg.speed = 999;
  for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(), -1.0, .001, 1001.0}) {
    g_cfg.speedCustom = invalid;
    assert(!Engine_Save(file.c_str()));
    std::string intact;
    assert(json::ReadTextFile(file.c_str(), intact) && intact == original);
    assert(Engine_Load(file.c_str()));
  }
  g_cfg.speed = 1; g_cfg.speedCustom = 1;
}

static void TestMousePersistence() {
  g_cfg.speed = 1; g_cfg.speedCustom = 1; g_cfg.loops = 1;
  g_cfg.continuous = 0; g_cfg.playTargets.clear(); g_cfg.panicVk = 0;
  MAc wheel; wheel.type = A_WH; wheel.wheel = 120;
  wheel.x = 450; wheel.y = 300; wheel.horizontal = true;
  MAc move; move.type = A_MOVE; move.hasPosition = true;
  move.btn = 1; move.mDown = move.mUp = true;
  move.path = {{1, 2}, {65535, 0}}; move.pdelays = {15, 20};
  MAc click; click.type = A_MD; click.hasPosition = true;
  MAc cursorClick; cursorClick.type = A_MD;
  g_events = {wheel, move, click, cursorClick};
  std::string saved;
  assert(Engine_ToJson(saved) && Engine_FromJson(saved));
  assert(g_events.size() == 4 && g_events[0].x == 450 && g_events[0].y == 300);
  assert(g_events[0].horizontal && g_events[0].HasPosition());
  assert(g_events[1].HasPosition() && g_events[1].x == 0 && g_events[1].y == 0);
  assert(g_events[1].mDown && g_events[1].mUp && g_events[1].btn == 1);
  assert(g_events[1].path == move.path && g_events[1].pdelays == move.pdelays);
  assert(g_events[2].HasPosition() && !g_events[3].HasPosition());
  for (int type : {A_MD, A_MU, A_WH, A_MOVE}) {
    MAc event = click; event.type = type; event.wheel = 120;
    if (type == A_MOVE) { event.path = {{0, 0}}; event.pdelays = {1}; }
    lastCursorX = 450; lastCursorY = 300; cursorPositions = 0;
    assert(Engine_PlaySelection({event}, 0));
    assert(Eventually([] { return g_state == ST_IDLE; }));
    Engine_Shutdown();
    assert(cursorPositions == 1 && lastCursorX == 0 && lastCursorY == 0);
  }
  assert(Engine_FromJson("{\"onty\":1,\"events\":[{\"e\":\"md\",\"b\":0}]}"));
  assert(!g_events[0].HasPosition());
  cursorPositions = 0;
  assert(Engine_PlaySelection(g_events, 0));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(cursorPositions == 0);
  macroedit::Item item;
  item.a = cursorClick;
  assert(macroedit::Apply(item, macroedit::Coordinates, 0, 0));
  assert(item.a.HasPosition());
  PreviewExclusions excluded; excluded.movement = true;
  auto preview = macroedit::Selection({item}, {0}, excluded);
  assert(preview.size() == 1 && !preview[0].HasPosition());
  assert(!Engine_FromJson("{\"onty\":1,\"events\":[{\"e\":\"mv\",\"p\":[[1,2]],\"down\":1}]}"));
}

static void IniMigrationTests() {
  auto previous = documentsPathOverride;
  auto base = ConfigDirectory() + L"ini-migration-test";
  assert(CreateDirectoryW(base.c_str(), nullptr));
  documentsPathOverride = base;
  auto root = ConfigDirectory();
  for (const wchar_t *name : {L"AutoOnty.ini", L"macros.ini", L"editor-settings.ini"})
    assert(json::WriteTextFile((root + name).c_str(), "[Settings]\r\nvalue=7\r\n"));
  auto ini = IniDirectory();
  assert(ini == root + L"Ini\\");
  for (const wchar_t *name : {L"AutoOnty.ini", L"macros.ini", L"editor-settings.ini"}) {
    assert(GetFileAttributesW((root + name).c_str()) == INVALID_FILE_ATTRIBUTES);
    assert(GetPrivateProfileIntW(L"Settings", L"value", 0, (ini + name).c_str()) == 7);
  }
  assert(json::WriteTextFile((root + L"AutoOnty.ini").c_str(), "[Settings]\r\nvalue=9\r\n"));
  assert(IniDirectory() == ini);
  assert(GetPrivateProfileIntW(L"Settings", L"value", 0, (ini + L"AutoOnty.ini").c_str()) == 7);
  assert(DeleteFileW((ini + L"AutoOnty.ini").c_str()));
  HANDLE locked = CreateFileW((root + L"AutoOnty.ini").c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
  assert(locked != INVALID_HANDLE_VALUE && IniDirectory().empty());
  CloseHandle(locked);
  assert(IniDirectory() == ini);
  assert(GetPrivateProfileIntW(L"Settings", L"value", 0, (ini + L"AutoOnty.ini").c_str()) == 9);
  assert(DeleteFileW((ini + L"macros.ini").c_str()));
  assert(CreateDirectoryW((ini + L"macros.ini").c_str(), nullptr));
  assert(IniDirectory().empty() && !ConfigLoad());
  documentsPathOverride = previous;
}

int main() {
  IniMigrationTests();
  int publishedVersion = -1;
  assert(Update_ParseVersion("1006003\n", publishedVersion) && publishedVersion == 1006003);
  assert(Update_ParseVersion("\xEF\xBB\xBF" "1006003\r\n", publishedVersion) && publishedVersion == 1006003);
  assert(Update_ParseVersion(" \t1006003\r\n", publishedVersion) && publishedVersion == 1006003);
  assert(Update_ParseVersion("2147483647", publishedVersion) && publishedVersion == 2147483647);
  for (const char *invalid : {"", "0", "-1", "1006003garbage", "100 6003",
                              "2147483648", "999999999999999999", "404: Not Found",
                              "<html>error</html>", "\xEF\xBB\xBF"}) {
    publishedVersion = -1;
    assert(!Update_ParseVersion(invalid, publishedVersion) && publishedVersion == -1);
  }
  assert(ExeName(LR"(C:\Games\game.exe)") == L"game.exe");
  assert(ExeName(L"C:/Games/game.exe") == L"game.exe");
  assert(ExeName(L"game.exe") == L"game.exe");
  assert(ExeName(L"").empty());
  assert(TargetMatches(L"game.exe;other.exe", L"GAME.EXE"));
  assert(!TargetMatches(L"game.exe;other.exe", L"notgame.exe"));
  assert(!TargetMatches(L"game.exe", L""));
  assert(TargetMatches(L"", L""));
  std::wstring targets;
  ToggleTarget(targets, L"game.exe"); ToggleTarget(targets, L"other.exe");
  assert(targets == L"game.exe;other.exe");
  ToggleTarget(targets, L"GAME.EXE"); assert(targets == L"other.exe");
  ToggleTarget(targets, L"third.exe"); ToggleTarget(targets, L"last.exe");
  RemoveTarget(targets, 1); assert(targets == L"other.exe;last.exe");
  RemoveTarget(targets, 99); assert(targets == L"other.exe;last.exe");
  RemoveTarget(targets, 1); assert(targets == L"other.exe");
  RemoveTarget(targets, 0); assert(targets.empty());
  RECT bounds{-1920, -100, 1920, 1080};
  for (int radius = 1; radius <= 50; ++radius)
    for (int i = 0; i <= 1000; ++i) {
      POINT center{100, 100};
      POINT p = JitterPoint(center, radius, i/1000.0, (i%101)/100.0, bounds);
      long dx = p.x-center.x, dy = p.y-center.y;
      assert(dx*dx+dy*dy <= radius*radius);
      for (POINT edge : {POINT{-1920,-100}, POINT{1919,1079}}) {
        auto q = JitterPoint(edge, radius, i/1000.0, 1, bounds);
        assert(q.x >= bounds.left && q.x < bounds.right && q.y >= bounds.top && q.y < bounds.bottom);
      }
    }

  g_cfg.speed = 1;
  g_cfg.speedCustom = 1;
  g_cfg.loops = 1;
  ImageTests();
  AutoclickImageTests();
  // X buttons use XDOWN/XUP and the matching data on every single/double pair.
  for (int button = 0; button < 5; ++button)
    for (bool dbl : {false, true}) {
      inputBatches.clear();
      assert(SendClick(button, dbl));
      const auto &batch = inputBatches.back();
      assert(batch.size() == (dbl ? 4 : 2));
      const DWORD downs[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN,
                             MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_XDOWN,
                             MOUSEEVENTF_XDOWN};
      const DWORD ups[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP,
                           MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_XUP,
                           MOUSEEVENTF_XUP};
      for (size_t i = 0; i < batch.size(); ++i) {
        assert(batch[i].type == INPUT_MOUSE);
        assert(batch[i].mi.dwFlags == (i % 2 ? ups[button] : downs[button]));
        assert(batch[i].mi.mouseData ==
               (button == 3 ? XBUTTON1 : button == 4 ? XBUTTON2 : 0));
      }
    }
  inputBatches.clear();
  sendLimit = 1;
  assert(!SendClick(4, true));
  assert(inputBatches.size() == 2 && inputBatches.back().size() == 1);
  assert(inputBatches.back()[0].mi.dwFlags == MOUSEEVENTF_XUP &&
         inputBatches.back()[0].mi.mouseData == XBUTTON2);
  inputBatches.clear(); sentClicks = 0;
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
  assert(!ini.empty() && ini.ends_with(L"\\Ini\\AutoOnty.ini"));
  modulePathOverride = L"C:\\" + std::wstring(1000, L'a') + L"\\AutoOnty.exe";
  assert(ExecutableDirectory() == modulePathOverride.substr(
                          0, modulePathOverride.find_last_of(L'\\') + 1));
  assert(IniPath() == ini);
  modulePathFailure = true;
  assert(ExecutableDirectory().empty() && IniPath() == ini);
  modulePathFailure = false;
  modulePathOverride.clear();
  documentsPathFailure = true;
  assert(ConfigDirectory().empty() && !ConfigLoad());
  documentsPathFailure = false;
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
  // Hover wait keeps finite repeat attempts intact; stop/panic remains live.
  g_cfg.acCountdown = 0; g_cfg.acMs = 1; g_cfg.acSecs = g_cfg.acMins = g_cfg.acHours = 0;
  g_cfg.acMode = 0; g_cfg.acPositionJitter = 0;
  g_cfg.acRepeatUntilStop = 0; g_cfg.acRepeatCount = 3;
  g_cfg.acTargets = L"game.exe;other.exe";
  focusOverride = 1; // Selected app focused, but pointer over a different app: wait.
  hoverOverride = 0; hoverCalls = 0; externalClickTarget = true;
  sentClicks = 0;
  assert(Autoclick_Start());
  assert(Eventually([] { return hoverCalls.load() > 2; }));
  Sleep(20);
  assert(g_acRunning && g_acClicks == 0 && sentClicks == 0);
  // Edits are next-run only, and the HUD can retrieve the original list.
  g_cfg.acTargets = L"changed.exe";
  assert(Autoclick_FocusTargets() == L"game.exe;other.exe");
  focusOverride = 0; // Selected app NOT focused, but pointer over it: click.
  hoverOverride = 1;
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 3 && sentClicks == 6);

  // Leaving the hovered app between the loop's two checks cannot burn an attempt.
  hoverRace = true; hoverOverride = 1;
  assert(Autoclick_Start());
  assert(Eventually([] { return hoverOverride.load() == 0; }));
  assert(g_acRunning && g_acClicks == 0);
  hoverOverride = 1;
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 3);

  // Hovering the selected app cannot authorize clicks at a different fixed destination.
  pointAllowed = false; hoverOverride = 1; g_cfg.acMode = 1;
  assert(Autoclick_Start());
  Sleep(30); assert(g_acRunning && g_acClicks == 0);
  pointAllowed = true;
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE); assert(g_acClicks == 3);
  // Follow/jitter mode must reject a landing point outside the selected app.
  g_cfg.acMode = 0; g_cfg.acPositionJitter = 1; g_cfg.acRadius = 3;
  pointAllowed = false;
  assert(Autoclick_Start());
  Sleep(30); assert(g_acRunning && g_acClicks == 0);
  pointAllowed = true;
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE); assert(g_acClicks == 3);
  g_cfg.acPositionJitter = 0;

  mockPhysicalKeys = true;
  g_cfg.panicMods = 0; g_cfg.panicVk = VK_PAUSE;
  hoverOverride = 0;
  assert(Autoclick_Start());
  assert(Eventually([] { return g_acRunning.load(); }));
  physicalKeys[VK_PAUSE] = SHORT(0x8000);
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  assert(g_acClicks == 0);
  physicalKeys[VK_PAUSE] = 0;

  // Jitter movement, double click and center restore are a single ordered batch.
  g_cfg.acTargets.clear(); g_cfg.acPositionJitter = 1; g_cfg.acRadius = 20;
  g_cfg.acDouble = 1; g_cfg.acRepeatCount = 12;
  { std::lock_guard<std::mutex> lock(batchMutex); inputBatches.clear(); }
  assert(Autoclick_Start());
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  {
    std::lock_guard<std::mutex> lock(batchMutex);
    assert(inputBatches.size() == 12);
    INPUT center = AbsoluteMouseMove({400,400}, DesktopBounds());
    for (const auto &batch : inputBatches) {
      assert(batch.size() == 6);
      assert(batch.front().mi.dwFlags == center.mi.dwFlags);
      assert(batch[1].mi.dwFlags == MOUSEEVENTF_LEFTDOWN);
      assert(batch[2].mi.dwFlags == MOUSEEVENTF_LEFTUP);
      assert(batch[3].mi.dwFlags == MOUSEEVENTF_LEFTDOWN);
      assert(batch[4].mi.dwFlags == MOUSEEVENTF_LEFTUP);
      assert(batch.back().mi.dx == center.mi.dx && batch.back().mi.dy == center.mi.dy);
    }
  }
  g_cfg.acDouble = 0; g_cfg.acPositionJitter = 0;

  // Hold detection: extra modifiers are allowed; any required release stops.
  g_cfg.acRepeatUntilStop = 1; g_cfg.acMods = HK_CTRL|HK_SHIFT|HK_ALT; g_cfg.acVk = 'A';
  physicalKeys['A'] = physicalKeys[VK_CONTROL] = physicalKeys[VK_SHIFT] = physicalKeys[VK_MENU] = SHORT(0x8000);
  physicalKeys[VK_LWIN] = SHORT(0x8000);
  assert(HotkeyHeld(g_cfg.acMods, g_cfg.acVk));
  assert(Autoclick_Start(true));
  assert(Eventually([] { return g_acClicks.load() > 0; }));
  Autoclick_KeyReleased('B');
  Sleep(10); assert(g_acRunning);
  Autoclick_KeyReleased(VK_LSHIFT);
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  // A toolbar-style start does not stop when the configured keys are released.
  assert(Autoclick_Start(false));
  for (auto &key : physicalKeys) key = 0;
  Autoclick_KeyReleased('A');
  Sleep(20); assert(g_acRunning);
  Autoclick_StopAndWait(INFINITE);
  // Holding the other side of a modifier keeps the combo held.
  physicalKeys['A'] = physicalKeys[VK_CONTROL] = physicalKeys[VK_SHIFT] = physicalKeys[VK_MENU] = SHORT(0x8000);
  physicalKeys[VK_RCONTROL] = SHORT(0x8000);
  assert(Autoclick_Start(true));
  Autoclick_KeyReleased(VK_LCONTROL);
  Sleep(10); assert(g_acRunning);
  physicalKeys['A'] = 0; // Polling fallback when no key-up event arrives.
  assert(Eventually([] { return !g_acRunning.load(); }));
  Autoclick_StopAndWait(INFINITE);
  for (auto &key : physicalKeys) key = 0;
  mockPhysicalKeys = false; externalClickTarget = false; hoverOverride = -1;
  g_cfg.acTargets.clear();

  // Real playback worker: focus pause releases and restores held keys/buttons,
  // manual pause is independent, timing freezes, and stop never restores input.
  g_cfg.playTargets = L"game.exe;other.exe";
  focusOverride = 0; downEvents = 0;
  assert(Engine_PlaySelection(run));
  assert(Eventually([] { return g_playPaused.load(); }));
  assert(downEvents == 0 && !heldKeys['Q'] && !heldMouse[1]);
  g_cfg.playTargets.clear();
  assert(Engine_FocusTargets() == L"game.exe;other.exe");
  focusOverride = 1;
  assert(Eventually([] { return heldKeys['Q'] && heldMouse[1] && !g_playPaused; }));
  focusOverride = 0;
  assert(Eventually([] { return g_playPaused && !heldKeys['Q'] && !heldMouse[1]; }));
  assert(!heldKeys['Q'] && !heldMouse[1]);
  DWORD frozenElapsed = g_playElapsed.load();
  Sleep(30); assert(g_playElapsed == frozenElapsed);
  Engine_TogglePause(); // Manual pause while focus-paused.
  focusOverride = 1;
  Sleep(30); assert(g_playPaused && !heldKeys['Q'] && !heldMouse[1]);
  Engine_TogglePause();
  assert(Eventually([] { return heldKeys['Q'] && heldMouse[1] && !g_playPaused; }));
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  if (downEvents != 2 || heldKeys['Q'] || heldMouse[1])
    fprintf(stderr, "Focus pause: key downs=%u, Q held=%d, mouse held=%d\n", downEvents.load(), heldKeys['Q'].load(), heldMouse[1].load());
  assert(downEvents == 2 && !heldKeys['Q'] && !heldMouse[1]);

  g_cfg.playTargets = L"game.exe"; focusOverride = 1; downEvents = 0;
  assert(Engine_PlaySelection(run));
  assert(Eventually([] { return heldKeys['Q'] && heldMouse[1]; }));
  focusOverride = 0;
  assert(Eventually([] { return g_playPaused.load(); }));
  Engine_Shutdown();
  assert(downEvents == 1 && !heldKeys['Q'] && !heldMouse[1]);
  // Panic during the initial focus wait also exits without injecting anything.
  mockPhysicalKeys = true; downEvents = 0;
  assert(Engine_PlaySelection(run));
  assert(Eventually([] { return g_playPaused.load(); }));
  physicalKeys[VK_PAUSE] = SHORT(0x8000);
  assert(Eventually([] { return g_state == ST_IDLE; }));
  Engine_Shutdown();
  assert(downEvents == 0);
  physicalKeys[VK_PAUSE] = 0; mockPhysicalKeys = false;
  g_cfg.playTargets.clear(); focusOverride = -1;

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
  documentsPathOverride = sandbox + L"\\documents-" + std::to_wstring(GetCurrentProcessId());
  assert(json::WriteTextFile((sandbox + L"\\AutoOnty.ini").c_str(),
                             "[AutoOnty]\r\nsound_cue=1\r\n[Autoclick]\r\nimages_enabled=1\r\n"));
  HANDLE oldIniReader = CreateFileW((sandbox + L"\\AutoOnty.ini").c_str(), GENERIC_READ,
                                     0, nullptr, OPEN_EXISTING, 0, nullptr);
  assert(oldIniReader != INVALID_HANDLE_VALUE);
  assert(!ConfigLoad());
  assert(GetFileAttributesW(IniPath().c_str()) == INVALID_FILE_ATTRIBUTES);
  CloseHandle(oldIniReader);
  assert(ConfigLoad());
  assert(g_cfg.soundCue == 1 && g_cfg.acImagesEnabled == 0);
  assert(g_cfg.playCountdown == 0 && g_cfg.acCountdown == 0 &&
         g_cfg.pauseVk == 'H');
  assert(g_cfg.recX1 == 1 && g_cfg.recX2 == 1);
  assert(g_cfg.acImageMask == ~0ULL); // Old settings retain all saved images.
  const auto iniFile = IniPath();
  assert(iniFile != sandbox + L"\\AutoOnty.ini");
  WritePrivateProfileStringW(L"AutoOnty", L"sound_cue", L"0", iniFile.c_str());
  g_cfg.acImagesEnabled = 1;
  assert(ConfigLoad() && !g_cfg.acImagesEnabled && !g_cfg.soundCue);
  assert(GetPrivateProfileIntW(L"Autoclick", L"images_enabled", 0,
                                (sandbox + L"\\AutoOnty.ini").c_str()) == 1);
  g_cfg.acImageMask = (1ULL << 63) | 5;
  ConfigSave();
  g_cfg.acImageMask = 0;
  ConfigLoad();
  assert(g_cfg.acImageMask == ((1ULL << 63) | 5));
  for (const wchar_t *invalid : {L"-1", L"18446744073709551616", L"1junk", L"", L"+2"}) {
    WritePrivateProfileStringW(L"Autoclick", L"image_mask", invalid, iniFile.c_str());
    ConfigLoad();
    assert(g_cfg.acImageMask == 0);
  }
  // The saved autoclick list is separate from the user's loaded macro.
  MAc profile;
  profile.type = A_IMAGE;
  profile.image = std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  profile.imageOptions.action = IA_AUTOCLICK;
  profile.imageOptions.interval = 37;
  profile.imageOptions.autoDuration = 250;
  auto beforeProfiles = g_events.size();
  // Import the old list and BMPs without changing or deleting the originals.
  auto legacyProfile = sandbox + L"\\AutoclickImages.onty";
  assert(Engine_WriteFile(legacyProfile.c_str(), {profile}));
  auto blockedImagesFolder = ConfigDirectory() + L"ImageDetections";
  assert(json::WriteTextFile(blockedImagesFolder.c_str(), "not a folder"));
  assert(!Autoclick_LoadImages() && g_acImages.empty());
  std::vector<MAc> intactLegacy;
  assert(Engine_ReadFile(legacyProfile.c_str(), intactLegacy) && intactLegacy.size() == 1);
  assert(DeleteFileW(blockedImagesFolder.c_str()));
  assert(Autoclick_LoadImages());
  auto profilesPath = ConfigDirectory() + L"ImageDetections\\ImageLibrary.onty";
  assert(GetFileAttributesW(profilesPath.c_str()) != INVALID_FILE_ATTRIBUTES);
  assert(GetFileAttributesW(legacyProfile.c_str()) != INVALID_FILE_ATTRIBUTES);
  assert(GetFileAttributesW((ConfigDirectory() + image::Name(*profile.image)).c_str()) != INVALID_FILE_ATTRIBUTES);
  assert(GetFileAttributesW((ConfigDirectory() + L"ImageDetections\\ImageDetections").c_str()) == INVALID_FILE_ATTRIBUTES);
  g_cfg.acImagesEnabled = 1;
  ConfigSave();
  assert(ConfigLoad() && !g_cfg.acImagesEnabled);
  assert(Autoclick_LoadImages() && g_acImages.size() == 1);
  auto shared = g_imageLibrary;
  shared[0].macro.sensitivity = 61;
  shared[0].macro.imageTimeout = 1234;
  assert(ImageLibrary_Save(shared) && ImageLibrary_Load());
  assert(g_imageLibrary[0].macro.sensitivity == 61 &&
         g_imageLibrary[0].macro.imageTimeout == 1234 &&
         g_acImages[0].imageOptions.interval == 37 && g_acImages[0].sensitivity == 85);
  assert(Autoclick_SaveImages({profile}));
  assert(g_imageLibrary[0].macro.sensitivity == 61 && g_imageLibrary[0].macro.imageTimeout == 1234);
  g_acImages.clear();
  assert(Autoclick_LoadImages());
  assert(g_acImages.size() == 1 && g_acImages[0].imageOptions.interval == 37 &&
         g_acImages[0].imageOptions.autoDuration == 250 && g_events.size() == beforeProfiles);
  profile.imageOptions.interval = 99;
  assert(Engine_WriteFile(legacyProfile.c_str(), {profile}));
  assert(Autoclick_LoadImages() && g_acImages[0].imageOptions.interval == 37);
  HANDLE lockedProfiles = CreateFileW(profilesPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
  assert(lockedProfiles != INVALID_HANDLE_VALUE);
  profile.imageOptions.interval = 99;
  assert(!Autoclick_SaveImages({profile}));
  CloseHandle(lockedProfiles);
  assert(Autoclick_LoadImages() && g_acImages[0].imageOptions.interval == 37);
  assert(!Autoclick_SaveImages(std::vector<MAc>(65, profile)));
  assert(json::WriteTextFile(profilesPath.c_str(), "{broken"));
  assert(!Autoclick_LoadImages() && g_acImages[0].imageOptions.interval == 37);
  assert(Autoclick_SaveImages({}));
  assert(Autoclick_LoadImages() && g_acImages.empty()); // Do not resurrect the old list.
  assert(GetFileAttributesW(legacyProfile.c_str()) != INVALID_FILE_ATTRIBUTES);
  // Manual file deletion prunes only that row and shifts the selected-image mask.
  auto missing = ImageLibrary_New(std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x654321}}));
  auto kept = ImageLibrary_New(std::make_shared<image::Pixels>(image::Pixels{1, 1, {0xabcdef}}));
  kept.autoclick.imageOptions.interval = 73;
  kept.macro.sensitivity = 62;
  kept.macro.imageOptions.action = IA_RUN_MACRO;
  kept.macro.imageMacro = std::make_shared<const std::vector<MAc>>(std::vector<MAc>{missing.macro});
  assert(ImageLibrary_Save({missing, kept}));
  g_cfg.acImageMask = 2;
  g_cfg.acImagesEnabled = 1;
  auto missingPath = ConfigDirectory() + image::Name(*missing.macro.image);
  assert(DeleteFileW(missingPath.c_str()));
  std::vector<MAc> strict;
  assert(!Engine_ReadFile(profilesPath.c_str(), strict, ConfigDirectory().c_str()));
  assert(ImageLibrary_Load() && g_imageLibrary.size() == 1 && g_acImages.size() == 1);
  assert(g_cfg.acImageMask == 1 && g_cfg.acImagesEnabled);
  assert(g_acImages[0].imageOptions.interval == 73 && g_imageLibrary[0].macro.sensitivity == 62);
  assert(g_imageLibrary[0].macro.imageMacro && !g_imageLibrary[0].macro.imageMacro->at(0).image);
  assert(GetFileAttributesW(missingPath.c_str()) == INVALID_FILE_ATTRIBUTES);
  assert(ImageLibrary_Load() && g_imageLibrary.size() == 1); // No resurrection on the next open.
  assert(ImageLibrary_Save(g_imageLibrary)); // Missing copied-macro assets do not block autoclick settings.
  auto keptPath = ConfigDirectory() + image::Name(*kept.macro.image);
  assert(json::WriteTextFile(keptPath.c_str(), "broken bitmap"));
  assert(!ImageLibrary_Load() && g_imageLibrary.size() == 1); // Corruption remains an error.
  assert(DeleteFileW(keptPath.c_str()));
  assert(ImageLibrary_Load() && g_imageLibrary.empty() && !g_cfg.acImageMask && !g_cfg.acImagesEnabled);
  assert(ImageLibrary_Load() && g_imageLibrary.empty());
  g_cfg.acImagesEnabled = 1;
  ConfigSave();
  assert(ConfigLoad() && !g_cfg.acImagesEnabled);
  for (int button : {3, 4, 5}) {
    g_cfg.acButton = button;
    g_cfg.recX1 = button == 3;
    g_cfg.recX2 = button == 4;
    ConfigSave();
    g_cfg.acButton = -1;
    g_cfg.recX1 = g_cfg.recX2 = -1;
    ConfigLoad();
    assert(g_cfg.acButton == (button == 5 ? 0 : button));
    assert(g_cfg.recX1 == (button == 3) && g_cfg.recX2 == (button == 4));
  }
  WritePrivateProfileStringW(L"AutoOnty", L"record_mods", L"7",
                             iniFile.c_str());
  WritePrivateProfileStringW(L"AutoOnty", L"record_vk", L"72", iniFile.c_str());
  WritePrivateProfileStringW(L"AutoOnty", L"countdown", L"99", iniFile.c_str());
  WritePrivateProfileStringW(L"Autoclick", L"countdown", L"-1",
                             iniFile.c_str());
  ConfigLoad();
  assert(g_cfg.pauseVk != 'H' && g_cfg.pauseMods == 7);
  assert(g_cfg.playCountdown == 0 && g_cfg.acCountdown == 0);
  assert(g_cfg.acHold == 0 && g_cfg.acPositionJitter == 0 && g_cfg.acRadius == 3);
  g_cfg.acHold = 1; g_cfg.acPositionJitter = 1; g_cfg.acRadius = 50;
  g_cfg.playTargets = L"game.exe;other.exe"; g_cfg.acTargets = L"click.exe";
  g_cfg.playCountdown = 60;
  g_cfg.acCountdown = 17;
  ConfigSave();
  g_cfg.playCountdown = g_cfg.acCountdown = 0;
  g_cfg.acHold = g_cfg.acPositionJitter = g_cfg.acRadius = 0;
  g_cfg.playTargets.clear(); g_cfg.acTargets.clear();
  ConfigLoad();
  assert(g_cfg.playCountdown == 60 && g_cfg.acCountdown == 17);
  assert(g_cfg.acHold == 1 && g_cfg.acPositionJitter == 1 && g_cfg.acRadius == 50);
  assert(g_cfg.playTargets == L"game.exe;other.exe" && g_cfg.acTargets == L"click.exe");
  WritePrivateProfileStringW(L"Autoclick", L"radius", L"99", iniFile.c_str());
  ConfigLoad(); assert(g_cfg.acRadius == 50);
  WritePrivateProfileStringW(L"Autoclick", L"radius", L"0", iniFile.c_str());
  ConfigLoad(); assert(g_cfg.acRadius == 1);
  g_cfg.playTargets.clear(); g_cfg.acTargets.clear();
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
  // Disabling one side button must leave the other recordable, including up.
  for (int enabled = 0; enabled <= 3; ++enabled) {
    g_events.clear();
    g_cfg.recX1 = enabled & 1;
    g_cfg.recX2 = enabled & 2;
    for (int button = 1; button <= 2; ++button) {
      MSLLHOOKSTRUCT side{};
      side.mouseData = DWORD(button) << 16;
      RecMsProc(HC_ACTION, WM_XBUTTONDOWN, (LPARAM)&side);
      RecMsProc(HC_ACTION, WM_XBUTTONUP, (LPARAM)&side);
    }
    assert(g_events.size() == 2 * ((enabled & 1 ? 1 : 0) + (enabled & 2 ? 1 : 0)));
    for (size_t i = 0; i < g_events.size(); i += 2) {
      assert(g_events[i].type == A_MD && g_events[i + 1].type == A_MU);
      assert(g_events[i].btn == g_events[i + 1].btn);
      assert(g_events[i].btn == ((enabled == 2 || i == 2) ? 4 : 3));
    }
  }
  g_events.clear();
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
  TestMousePersistence();
  AdditionalRegressionTests();
  puts("Regression tests passed: mouse persistence/origin, image recognition, previous coverage, preview "
       "filters/timing, "
       "visibility, horizontal wheel, preferences, focus pause/wait/panic, "
       "hold release, jitter input batches and INI round trips.");
}
