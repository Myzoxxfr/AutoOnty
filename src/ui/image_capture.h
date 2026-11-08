#pragma once
// Internal capture helpers shared by the themed image editor and isolated
// capture tests.
#include "ui/image_files.h"
static bool imageSnipping = false, imageSnipCancelled = false;
static bool imageSnipSaveFailed = false;
bool Macros_RestoreImageCapture() {
  if (!imageSnipping)
    return false;
  imageSnipCancelled = true;
  return true;
}
struct ImageWindows {
  std::vector<HWND> visible;
  bool minimizedMain = false;
  void Hide(HWND h) {
    for (HWND p = h; p && p != g_hwnd; p = GetWindow(p, GW_OWNER))
      if (IsWindowVisible(p)) {
        visible.push_back(p);
        ShowWindow(p, SW_HIDE);
      }
    minimizedMain = IsWindow(g_hwnd) != FALSE;
    ShowWindow(g_hwnd, SW_MINIMIZE);
  }
  void Restore() {
    if (visible.empty() && !minimizedMain)
      return;
    if (minimizedMain && IsWindow(g_hwnd))
      ShowWindow(g_hwnd, SW_RESTORE);
    minimizedMain = false;
    for (auto it = visible.rbegin(); it != visible.rend(); ++it)
      if (IsWindow(*it))
        ShowWindow(*it, SW_SHOW);
    if (!visible.empty())
      SetForegroundWindow(visible.front());
    else if (IsWindow(g_hwnd))
      SetForegroundWindow(g_hwnd);
    visible.clear();
  }
};
static bool SnipShortcut() {
  INPUT keys[6]{};
  WORD vk[] = {VK_LWIN, VK_SHIFT, 'S', 'S', VK_SHIFT, VK_LWIN};
  for (int i = 0; i < 6; ++i) {
    keys[i].type = INPUT_KEYBOARD;
    keys[i].ki.wVk = vk[i];
    keys[i].ki.dwFlags = i >= 3 ? KEYEVENTF_KEYUP : 0;
  }
  UINT sent = SendInput(6, keys, sizeof(INPUT));
  if (sent != 6) {
    INPUT release[3]{};
    UINT count = 0;
    for (int i = 2; i >= 0; --i)
      if (sent > UINT(i) && sent <= UINT(5 - i)) {
        release[count] = keys[5 - i];
        ++count;
      }
    if (count)
      SendInput(count, release, sizeof(INPUT));
  }
  return sent == 6;
}
static std::shared_ptr<image::Pixels> SnipImage(HWND owner, ImagePendingFiles *pending = nullptr) {
  imageSnipping = true;
  imageSnipCancelled = false;
  imageSnipSaveFailed = false;
  ImageWindows hidden;
  hidden.Hide(owner);
  DWORD began = GetTickCount();
  MSG msg;
  auto pump = [&] {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        imageSnipCancelled = true;
        PostQuitMessage((int)msg.wParam);
        return false;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    return true;
  };
  while (GetTickCount() - began < 350) {
    if (!pump())
      break;
    Sleep(10);
  }
  DWORD sequence = GetClipboardSequenceNumber();
  ImageSnipFiles screenshots(ImageScreenshotFolder());
  bool sent = !imageSnipCancelled && SnipShortcut();
  std::shared_ptr<image::Pixels> pixels;
  began = GetTickCount();
  while (sent && !imageSnipCancelled && GetTickCount() - began < 30000) {
    if (!pump())
      break;
    if (GetClipboardSequenceNumber() != sequence) {
      pixels = image::Clipboard(owner);
      if (pixels)
        break;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    if (GetTickCount() - began > 800 &&
        (process == GetCurrentProcessId() || !IsIconic(g_hwnd)))
      break;
    Sleep(15);
  }
  imageSnipping = false;
  hidden.Restore();
  if (pixels && !imageSnipCancelled && !(pending ? pending->Store(*pixels) : ImageStoreFile(*pixels))) {
    imageSnipSaveFailed = true;
    return {};
  }
  if (pixels && !imageSnipCancelled) {
    began = GetTickCount();
    while (!screenshots.Cleanup(*pixels) && screenshots.ready && GetTickCount() - began < 1000)
      Sleep(50);
  }
  return imageSnipCancelled ? nullptr : pixels;
}
