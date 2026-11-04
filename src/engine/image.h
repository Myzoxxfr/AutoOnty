#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
namespace image {
struct Pixels {
  int w = 0, h = 0;
  std::vector<uint32_t> rgb;
};
inline bool Dimensions(int w, int h) {
  return w > 0 && h > 0 && w <= 16384 && h <= 16384 &&
         uint64_t(w) * h <= 32000000;
}
// Search desktops can be wider/larger than an individual saved snippet.
inline bool ScreenDimensions(int w, int h) {
  return w > 0 && h > 0 && w <= 65536 && h <= 65536 &&
         uint64_t(w) * h <= 64000000;
}
inline std::shared_ptr<Pixels> Dib(const void *data, size_t size) {
  if (!data || size < sizeof(BITMAPINFOHEADER))
    return {};
  BITMAPINFOHEADER b;
  memcpy(&b, data, sizeof(b));
  if ((b.biSize != 40 && b.biSize != 52 && b.biSize != 56 && b.biSize != 108 &&
       b.biSize != 124) ||
      b.biSize > size || b.biHeight == LONG_MIN ||
      !Dimensions(b.biWidth, abs(b.biHeight)) || b.biPlanes != 1 ||
      (b.biCompression != BI_RGB && b.biCompression != BI_BITFIELDS) ||
      b.biClrUsed != 0 || (b.biBitCount != 24 && b.biBitCount != 32))
    return {};
  size_t offset = b.biSize;
  if (b.biCompression == BI_BITFIELDS) {
    if (b.biBitCount != 32 || size < 52 || (b.biSize != 40 && b.biSize < 52))
      return {};
    DWORD masks[3];
    memcpy(masks, (const BYTE *)data + 40, sizeof(masks));
    if (masks[0] != 0xff0000 || masks[1] != 0xff00 || masks[2] != 0xff)
      return {};
    if (b.biSize == 40)
      offset += 12;
  }
  if (offset > size)
    return {};
  size_t stride = ((size_t(b.biWidth) * b.biBitCount + 31) / 32) * 4;
  if (stride * abs(b.biHeight) > size - offset)
    return {};
  auto p = std::make_shared<Pixels>();
  p->w = b.biWidth;
  p->h = abs(b.biHeight);
  p->rgb.resize(size_t(p->w) * p->h);
  auto bits = (const BYTE *)data + offset;
  for (int y = 0; y < p->h; ++y)
    for (int x = 0; x < p->w; ++x) {
      auto c = bits + (b.biHeight > 0 ? p->h - 1 - y : y) * stride +
               x * (b.biBitCount / 8);
      p->rgb[size_t(y) * p->w + x] = c[0] | (c[1] << 8) | (c[2] << 16);
    }
  return p;
}
inline std::shared_ptr<Pixels> Clipboard(HWND owner) {
  if (!OpenClipboard(owner))
    return {};
  HANDLE h = GetClipboardData(CF_DIB);
  void *data = h ? GlobalLock(h) : nullptr;
  std::shared_ptr<Pixels> p;
  try {
    if (data)
      p = Dib(data, GlobalSize(h));
  } catch (const std::bad_alloc &) {
  } // Always unlock/close the shared clipboard.
  if (data)
    GlobalUnlock(h);
  CloseClipboard();
  return p;
}
inline std::shared_ptr<Pixels> Screen(int &ox, int &oy) {
  ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
  oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int w = GetSystemMetrics(SM_CXVIRTUALSCREEN),
      h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (!ScreenDimensions(w, h))
    return {};
  HDC screen = GetDC(nullptr);
  if (!screen)
    return {};
  HDC dc = CreateCompatibleDC(screen);
  BITMAPINFO info{};
  info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
  void *bits = nullptr;
  HBITMAP bm =
      CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  std::shared_ptr<Pixels> p;
  if (bm && bits && dc) {
    auto old = SelectObject(dc, bm);
    if (old && old != HGDI_ERROR) {
      if (BitBlt(dc, 0, 0, w, h, screen, ox, oy, SRCCOPY | CAPTUREBLT) &&
          GdiFlush()) {
        try {
          p = std::make_shared<Pixels>();
          p->w = w;
          p->h = h;
          p->rgb.assign((uint32_t *)bits, (uint32_t *)bits + size_t(w) * h);
        } catch (const std::bad_alloc &) {
          p.reset();
        }
      }
      SelectObject(dc, old);
    }
  }
  if (bm)
    DeleteObject(bm);
  if (dc)
    DeleteDC(dc);
  ReleaseDC(nullptr, screen);
  return p;
}
inline bool Save(const std::wstring &path, const Pixels &p) {
  if (!Dimensions(p.w, p.h) || p.rgb.size() != size_t(p.w) * p.h)
    return false;
  // Publish only complete files: an interrupted save must not damage an image
  // already shared by several saved macros.
  static volatile LONG sequence = 0;
  std::wstring temporary;
  HANDLE h = INVALID_HANDLE_VALUE;
  for (int attempt = 0; attempt < 16 && h == INVALID_HANDLE_VALUE; ++attempt) {
    temporary = path + L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(InterlockedIncrement(&sequence));
    h = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS)
      return false;
  }
  if (h == INVALID_HANDLE_VALUE)
    return false;
  BITMAPFILEHEADER f{};
  BITMAPINFOHEADER b{sizeof(b), p.w, -p.h, 1, 32, BI_RGB};
  f.bfType = 0x4d42;
  f.bfOffBits = sizeof(f) + sizeof(b);
  f.bfSize = DWORD(f.bfOffBits + p.rgb.size() * 4);
  DWORD done;
  bool ok =
      WriteFile(h, &f, sizeof(f), &done, nullptr) && done == sizeof(f) &&
      WriteFile(h, &b, sizeof(b), &done, nullptr) && done == sizeof(b) &&
      WriteFile(h, p.rgb.data(), DWORD(p.rgb.size() * 4), &done, nullptr) &&
      done == p.rgb.size() * 4;
  if (ok)
    ok = FlushFileBuffers(h) != FALSE;
  if (!CloseHandle(h))
    ok = false;
  if (ok)
    ok = MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
         FALSE;
  if (!ok)
    DeleteFileW(temporary.c_str());
  return ok;
}
inline std::shared_ptr<Pixels> Load(const std::wstring &path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return {};
  LARGE_INTEGER length{};
  DWORD done;
  std::shared_ptr<Pixels> p;
  try {
    if (GetFileSizeEx(h, &length) && length.QuadPart >= 54 &&
        length.QuadPart <= 128000256) {
      DWORD size = (DWORD)length.QuadPart;
      std::vector<BYTE> bytes(size);
      if (ReadFile(h, bytes.data(), size, &done, nullptr) && done == size) {
        BITMAPFILEHEADER f;
        BITMAPINFOHEADER b;
        memcpy(&f, bytes.data(), sizeof(f));
        memcpy(&b, bytes.data() + sizeof(f), sizeof(b));
        size_t offset = sizeof(f) + size_t(b.biSize);
        if (b.biSize == 40 && b.biCompression == BI_BITFIELDS)
          offset += 12;
        if (f.bfType == 0x4d42 && f.bfOffBits == offset)
          p = Dib(bytes.data() + 14, size - 14);
      }
    }
  } catch (const std::bad_alloc &) {
    p.reset();
  }
  CloseHandle(h);
  return p;
}
inline bool SafeName(const std::wstring &path) {
  // Only this one relative directory is allowed; legacy bare names still load.
  std::wstring s = path;
  if (s.starts_with(L"ImageDetections/") || s.starts_with(L"ImageDetections\\"))
    s.erase(0, 16);
  if (s.empty() || s.size() > 180)
    return false;
  for (wchar_t c : s)
    if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
          (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'.'))
      return false;
  auto stem = s.substr(0, s.find(L'.'));
  for (auto &c : stem)
    c = towupper(c);
  if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
      (stem.size() == 4 &&
       (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
       stem[3] >= L'1' && stem[3] <= L'9'))
    return false;
  return s != L"." && s != L".." && s.ends_with(L".bmp");
}
inline std::wstring Directory(const wchar_t *path) {
  std::wstring s = path;
  auto i = s.find_last_of(L"/\\");
  return i == s.npos ? L"" : s.substr(0, i + 1);
}
inline std::wstring Name(const Pixels &p) {
  uint64_t hash = 14695981039346656037ull;
  for (auto c : p.rgb) {
    hash ^= c & 0xffffff;
    hash *= 1099511628211ull;
  }
  hash ^= uint64_t(p.w) << 32 | unsigned(p.h);
  return L"ImageDetections/image-" + std::to_wstring(hash) + L".bmp";
}
} // namespace image

#include "engine/image_match.h"
