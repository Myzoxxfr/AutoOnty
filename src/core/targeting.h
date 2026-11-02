#pragma once
#include <windows.h>
#include <string>
#include <algorithm>
#include <cmath>
inline std::wstring ExeName(const std::wstring &path) {
  return path.substr(path.find_last_of(L"\\/") == std::wstring::npos ? 0 : path.find_last_of(L"\\/") + 1);
}
inline std::wstring WindowExe(HWND window) {
  if (!window) return {};
  window = GetAncestor(window, GA_ROOT);
  DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
  if (!pid || pid == GetCurrentProcessId()) return {};
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) return {};
  wchar_t path[32768]; DWORD size = 32768;
  bool ok = QueryFullProcessImageNameW(process, 0, path, &size) != FALSE;
  CloseHandle(process);
  return ok ? ExeName(std::wstring(path, size)) : L"";
}
inline bool TargetMatches(const std::wstring &targets, const std::wstring &exe) {
  if (targets.empty()) return true;
  if (exe.empty()) return false;
  size_t start = 0;
  do {
    auto end = targets.find(L';', start);
    if (_wcsicmp(targets.substr(start, end-start).c_str(), exe.c_str()) == 0) return true;
    if (end == std::wstring::npos) break;
    start = end + 1;
  } while (start < targets.size());
  return false;
}
inline void RemoveTarget(std::wstring &targets, size_t index) {
  size_t start = 0;
  while (index-- && start < targets.size()) {
    auto end = targets.find(L';', start);
    start = end == std::wstring::npos ? targets.size() : end+1;
  }
  if (start >= targets.size()) return;
  auto end = targets.find(L';', start);
  if (end != std::wstring::npos) targets.erase(start, end-start+1);
  else targets.erase(start ? start-1 : 0);
}
inline void ToggleTarget(std::wstring &targets, const std::wstring &exe) {
  if (exe.empty()) return;
  size_t start = 0, index = 0;
  while (start < targets.size()) {
    auto end = targets.find(L';', start);
    if (_wcsicmp(targets.substr(start, end-start).c_str(), exe.c_str()) == 0) {
      RemoveTarget(targets, index); return;
    }
    if (end == std::wstring::npos) break;
    start = end+1; ++index;
  }
  if (!targets.empty()) targets += L';';
  targets += ExeName(exe);
}
inline bool TargetFocused(const std::wstring &targets) {
  if (targets.empty()) return true;
  // Cache process lookup for 10 ms while still noticing a new foreground HWND immediately.
  static thread_local HWND previous = nullptr;
  static thread_local DWORD checked = 0;
  static thread_local std::wstring exe;
  HWND foreground = GetForegroundWindow();
  DWORD now = GetTickCount();
  if (foreground != previous || now - checked >= 10) {
    exe = WindowExe(foreground); previous = foreground; checked = now;
  }
  return TargetMatches(targets, exe);
}
// Match the visible top-level application under a point, regardless of keyboard focus.
inline bool TargetAtPoint(const std::wstring &targets, POINT point) {
  if (targets.empty()) return true;
  HWND window = WindowFromPoint(point);
  HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
  static thread_local HWND previous = nullptr;
  static thread_local DWORD checked = 0;
  static thread_local std::wstring exe;
  DWORD now = GetTickCount();
  if (root != previous || now - checked >= 10) {
    exe = WindowExe(root);
    previous = root;
    checked = now;
  }
  return TargetMatches(targets, exe);
}
inline bool TargetHovered(const std::wstring &targets) {
  if (targets.empty()) return true;
  POINT point{};
  return GetCursorPos(&point) && TargetAtPoint(targets, point);
}
inline INPUT AbsoluteMouseMove(POINT point, RECT bounds) {
  INPUT input{};
  input.type = INPUT_MOUSE;
  LONG width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  // Map to the center of the screen pixel's normalized input bin.
  input.mi.dx = width > 0 ? LONG(((long long)(point.x - bounds.left) * 65536 + 32768) / width) : 0;
  input.mi.dy = height > 0 ? LONG(((long long)(point.y - bounds.top) * 65536 + 32768) / height) : 0;
  input.mi.dx = (std::clamp)(input.mi.dx, LONG(0), LONG(65535));
  input.mi.dy = (std::clamp)(input.mi.dy, LONG(0), LONG(65535));
  return input;
}
inline RECT DesktopBounds() {
  RECT bounds{GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0};
  bounds.right = bounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  bounds.bottom = bounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  return bounds;
}
inline POINT JitterPoint(POINT center, int radius, double angleUnit, double distanceUnit, RECT bounds) {
  double distance = radius * std::sqrt((std::clamp)(distanceUnit, 0.0, 1.0));
  // Truncate offsets so integer rounding cannot escape the radius.
  POINT p{center.x + LONG(distance * std::cos(angleUnit * 6.283185307179586)),
          center.y + LONG(distance * std::sin(angleUnit * 6.283185307179586))};
  p.x = (std::clamp)(p.x, bounds.left, bounds.right-1);
  p.y = (std::clamp)(p.y, bounds.top, bounds.bottom-1);
  return p;
}
