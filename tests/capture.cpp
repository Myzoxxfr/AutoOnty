// Isolated snipping-flow tests: no real windows, clipboard access, input, or
// sleeps.
#include "engine/engine.h"
#include "lang/lang.h"
#include <cassert>
#include <commctrl.h>
#include <cstdio>
#include <thread>
#include <windows.h>
#include <gdiplus.h>
#include <shlobj.h>
#include "storage_paths.h"
static bool failSnipSave = false;
std::wstring ConfigDirectory() {
  if (failSnipSave) return {};
  wchar_t documents[MAX_PATH];
  if (FAILED(TestDocumentsFolder(nullptr, 0, nullptr, 0, documents))) return {};
  auto directory = std::wstring(documents) + L"\\AutoOnty";
  CreateDirectoryW(directory.c_str(), nullptr);
  return directory + L"\\";
}
static HRESULT WINAPI NoScreenshots(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR *) { return E_FAIL; }
static HWND g_hwnd = (HWND)1;
static HINSTANCE g_hInst = nullptr;
static int modalDepth = 0;
struct ModalGuard {
  ModalGuard() { ++modalDepth; }
  ~ModalGuard() { --modalDepth; }
};
static void PlaceOver(HWND, int, int, int &x, int &y) { x = y = 0; }
const wchar_t *L(int) { return L""; }
bool Macros_RestoreImageCapture();
namespace capturetest {
enum class Scenario {
  OldImage,
  FreshImage,
  DelayedImage,
  InvalidImage,
  TrayCancel,
  TaskbarRestore,
  EarlyCancel,
  QuitEarly,
  QuitAfterTrigger
};
static Scenario scenario;
static DWORD now = 0, triggeredAt = 0, sequence = 100;
static bool triggered = false, delivered = false, request = false,
            editorVisible = true, iconic = false, quitSeen = false;
static unsigned sends = 0, reads = 0, sequenceReads = 0, restores = 0;
static int quitCode = -1;
static HWND foreground = (HWND)3;
static void Reset(Scenario next, bool retainClipboard = false) {
  scenario = next;
  now = triggeredAt = 0;
  triggered = delivered = request = quitSeen = false;
  editorVisible = true;
  iconic = false;
  foreground = (HWND)3;
  sends = reads = sequenceReads = restores = 0;
  quitCode = -1;
  if (!retainClipboard)
    sequence = 100;
}
static DWORD WINAPI Tick() { return now; }
static void WINAPI Wait(DWORD delay) {
  now += delay;
  if (scenario == Scenario::EarlyCancel && !request && now >= 100) {
    request = true;
    assert(Macros_RestoreImageCapture());
  }
  if (triggered && now - triggeredAt >= 900 && !request) {
    if (scenario == Scenario::TrayCancel) {
      request = true;
      assert(Macros_RestoreImageCapture());
    }
    if (scenario == Scenario::TaskbarRestore) {
      request = true;
      iconic = false;
      foreground = g_hwnd;
    }
  }
}
static BOOL WINAPI Peek(LPMSG message, HWND, UINT, UINT, UINT) {
  bool quitting =
      (scenario == Scenario::QuitEarly && now >= 100 && !triggered) ||
      (scenario == Scenario::QuitAfterTrigger && triggered &&
       now - triggeredAt >= 100);
  if (quitting && !quitSeen) {
    quitSeen = true;
    message->message = WM_QUIT;
    message->wParam = 42;
    return TRUE;
  }
  return FALSE;
}
static void WINAPI Quit(int code) { quitCode = code; }
static UINT WINAPI Input(UINT count, LPINPUT, int) {
  assert(count == 6 && sequenceReads > 0 && now >= 350);
  ++sends;
  triggered = true;
  triggeredAt = now;
  return count;
}
static DWORD WINAPI Sequence() {
  ++sequenceReads;
  if (triggered && !delivered && now - triggeredAt >= 100 &&
      (scenario == Scenario::FreshImage || scenario == Scenario::DelayedImage ||
       scenario == Scenario::InvalidImage)) {
    ++sequence;
    delivered = true;
  }
  return sequence;
}
static HWND WINAPI Owner(HWND window, UINT) {
  return window == (HWND)2 ? g_hwnd : nullptr;
}
static BOOL WINAPI Visible(HWND window) {
  return window == (HWND)2 ? editorVisible : TRUE;
}
static BOOL WINAPI Iconic(HWND) { return iconic; }
static BOOL WINAPI Window(HWND window) {
  return window == g_hwnd || window == (HWND)2;
}
static BOOL WINAPI Show(HWND window, int how) {
  if (window == g_hwnd) {
    if (how == SW_MINIMIZE)
      iconic = true;
    if (how == SW_RESTORE) {
      iconic = false;
      ++restores;
    }
  }
  if (window == (HWND)2)
    editorVisible = how != SW_HIDE;
  return TRUE;
}
static HWND WINAPI Foreground() { return foreground; }
static BOOL WINAPI Activate(HWND window) {
  foreground = window;
  return TRUE;
}
static DWORD WINAPI Process(HWND window, LPDWORD process) {
  *process =
      window == g_hwnd ? GetCurrentProcessId() : GetCurrentProcessId() + 1;
  return 1;
}
} // namespace capturetest
namespace image {
static std::shared_ptr<Pixels> FakeClipboard(HWND) {
  ++capturetest::reads;
  using capturetest::Scenario;
  if (capturetest::scenario == Scenario::InvalidImage ||
      (capturetest::scenario == Scenario::DelayedImage &&
       capturetest::now - capturetest::triggeredAt < 250))
    return {};
  return std::make_shared<Pixels>(
      Pixels{1, 1, {capturetest::delivered ? 0x123456u : 0xabcdefu}});
}
} // namespace image
#define GetTickCount capturetest::Tick
#define Sleep capturetest::Wait
#define PeekMessageW capturetest::Peek
#define PostQuitMessage capturetest::Quit
#define SendInput capturetest::Input
#define GetClipboardSequenceNumber capturetest::Sequence
#define GetWindow capturetest::Owner
#define IsWindowVisible capturetest::Visible
#define IsIconic capturetest::Iconic
#define IsWindow capturetest::Window
#define ShowWindow capturetest::Show
#define GetForegroundWindow capturetest::Foreground
#define SetForegroundWindow capturetest::Activate
#define GetWindowThreadProcessId capturetest::Process
#define Clipboard FakeClipboard
#define SHGetKnownFolderPath NoScreenshots
#include "ui/image_capture.h"
#undef SHGetKnownFolderPath
#undef GetTickCount
#undef Sleep
#undef PeekMessageW
#undef PostQuitMessage
#undef SendInput
#undef GetClipboardSequenceNumber
#undef GetWindow
#undef IsWindowVisible
#undef IsIconic
#undef IsWindow
#undef ShowWindow
#undef GetForegroundWindow
#undef SetForegroundWindow
#undef GetWindowThreadProcessId
#undef Clipboard
int main() {
  using namespace capturetest;
  Gdiplus::GdiplusStartupInput startup;
  ULONG_PTR token;
  assert(Gdiplus::GdiplusStartup(&token, &startup, nullptr) == Gdiplus::Ok);
  _set_error_mode(_OUT_TO_STDERR);
  for (auto next :
       {Scenario::OldImage, Scenario::FreshImage, Scenario::DelayedImage,
        Scenario::InvalidImage, Scenario::TrayCancel, Scenario::TaskbarRestore,
        Scenario::EarlyCancel, Scenario::QuitEarly,
        Scenario::QuitAfterTrigger}) {
    Reset(next);
    auto result = SnipImage((HWND)2);
    assert(!imageSnipping && editorVisible && !iconic && restores == 1);
    if (next == Scenario::FreshImage || next == Scenario::DelayedImage) {
      assert(result && result->rgb[0] == 0x123456 && sends == 1 && reads > 0);
      if (next == Scenario::DelayedImage)
        assert(reads > 1);
    } else
      assert(!result);
    if (next == Scenario::OldImage) {
      assert(reads == 0 && now >= 30350);
    }
    if (next == Scenario::EarlyCancel || next == Scenario::QuitEarly)
      assert(sends == 0);
    if (next == Scenario::TrayCancel || next == Scenario::TaskbarRestore)
      assert(now < 2000);
    if (next == Scenario::QuitEarly || next == Scenario::QuitAfterTrigger)
      assert(quitCode == 42);
  }
  // Retake records a NEW baseline: the preceding capture is now an old image.
  Reset(Scenario::FreshImage);
  assert(SnipImage((HWND)2));
  auto previousSequence = sequence;
  Reset(Scenario::OldImage, true);
  assert(!SnipImage((HWND)2));
  assert(sequence == previousSequence && reads == 0);
  // Dedicated watchers capture from the main window, without an editor owner.
  Reset(Scenario::FreshImage);
  assert(SnipImage(g_hwnd));
  assert(!iconic && restores == 1 && foreground == g_hwnd);
  assert(!Macros_RestoreImageCapture());
  auto folder = ConfigDirectory() + L"screenshots-test";
  assert(CreateDirectoryW(folder.c_str(), nullptr));
  image::Pixels captured{2, 2, {0x123456, 0x234567, 0x345678, 0x456789}};
  auto writePng = [&](const wchar_t *name, image::Pixels pixels) {
    const CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    Gdiplus::Bitmap bitmap(pixels.w, pixels.h, pixels.w * 4, PixelFormat32bppRGB, (BYTE *)pixels.rgb.data());
    assert(bitmap.Save((folder + L"\\" + name).c_str(), &png) == Gdiplus::Ok);
  };
  writePng(L"old.png", captured);
  ImageSnipFiles files(folder);
  assert(files.ready && !files.Cleanup(captured));
  writePng(L"different.png", {2, 2, {0, 0, 0, 0}});
  writePng(L"fresh.png", captured);
  failSnipSave = true;
  assert(!files.Cleanup(captured));
  assert(GetFileAttributesW((folder + L"\\fresh.png").c_str()) != INVALID_FILE_ATTRIBUTES);
  failSnipSave = false;
  HANDLE locked = CreateFileW((folder + L"\\fresh.png").c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
  assert(locked != INVALID_HANDLE_VALUE && !files.Cleanup(captured));
  CloseHandle(locked);
  assert(files.Cleanup(captured));
  assert(GetFileAttributesW((folder + L"\\fresh.png").c_str()) == INVALID_FILE_ATTRIBUTES);
  assert(GetFileAttributesW((folder + L"\\old.png").c_str()) != INVALID_FILE_ATTRIBUTES);
  assert(GetFileAttributesW((folder + L"\\different.png").c_str()) != INVALID_FILE_ATTRIBUTES);
  auto saved = image::Load(ConfigDirectory() + image::Name(captured));
  assert(saved && saved->rgb == captured.rgb);
  assert(!files.Cleanup(captured));
  Gdiplus::GdiplusShutdown(token);
  puts("Capture tests passed: fresh clipboard, old/invalid/delayed images, "
       "retake, tray/taskbar cancel, timeout, quit and verified screenshot cleanup.");
}
