#pragma once
#include <gdiplus.h>
#include <shlobj.h>
#include <objidl.h>

std::wstring ConfigDirectory();

static std::shared_ptr<image::Pixels> ImageBitmapPixels(Gdiplus::Bitmap &input) {
  if (input.GetLastStatus() != Gdiplus::Ok ||
      !image::Dimensions((int)input.GetWidth(), (int)input.GetHeight())) return {};
  int width = input.GetWidth(), height = input.GetHeight();
  Gdiplus::Bitmap converted(width, height, PixelFormat32bppARGB);
  {
    Gdiplus::Graphics graphics(&converted);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    if (graphics.DrawImage(&input, 0, 0, width, height) != Gdiplus::Ok) return {};
  }
  Gdiplus::BitmapData data{};
  Gdiplus::Rect rect(0, 0, width, height);
  if (converted.LockBits(&rect, Gdiplus::ImageLockModeRead,
                         PixelFormat32bppARGB, &data) != Gdiplus::Ok) return {};
  std::shared_ptr<image::Pixels> result;
  try {
    result = std::make_shared<image::Pixels>();
    result->w = width; result->h = height;
    result->rgb.resize(size_t(width) * height);
    for (int y = 0; y < height; ++y)
      memcpy(result->rgb.data() + size_t(y) * width,
             (BYTE *)data.Scan0 + ptrdiff_t(y) * data.Stride, size_t(width) * 4);
  } catch (const std::bad_alloc &) { result.reset(); }
  converted.UnlockBits(&data);
  return result;
}

static std::shared_ptr<image::Pixels> ImageLoadFile(const wchar_t *path) {
  Gdiplus::Bitmap input(path, FALSE);
  return ImageBitmapPixels(input);
}

static std::shared_ptr<image::Pixels> ImageReadScreenshot(HANDLE file) {
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 64000000) return {};
  HGLOBAL bytes = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)size.QuadPart);
  if (!bytes) return {};
  void *data = GlobalLock(bytes);
  DWORD read = 0;
  bool ok = data && ReadFile(file, data, (DWORD)size.QuadPart, &read, nullptr) && read == size.QuadPart;
  if (data) GlobalUnlock(bytes);
  IStream *stream = nullptr;
  if (!ok || FAILED(CreateStreamOnHGlobal(bytes, TRUE, &stream))) {
    GlobalFree(bytes);
    return {};
  }
  std::shared_ptr<image::Pixels> result;
  {
    Gdiplus::Bitmap bitmap(stream, FALSE);
    result = ImageBitmapPixels(bitmap);
  }
  stream->Release();
  return result;
}

static std::wstring ImageScreenshotFolder() {
  PWSTR path = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Screenshots, KF_FLAG_DONT_VERIFY, nullptr, &path))) return {};
  std::wstring folder(path);
  CoTaskMemFree(path);
  return folder;
}

static bool ImageStoreFile(const image::Pixels &pixels) {
  auto directory = ConfigDirectory();
  if (directory.empty()) return false;
  auto folder = directory + L"ImageDetections";
  if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
  return image::Save(directory + image::Name(pixels), pixels);
}

struct ImagePendingFiles {
  std::vector<std::wstring> created;
  bool Store(const image::Pixels &pixels) {
    auto path = ConfigDirectory() + image::Name(pixels);
    DWORD attributes = GetFileAttributesW(path.c_str());
    bool missing = attributes == INVALID_FILE_ATTRIBUTES &&
        (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND);
    if (!ImageStoreFile(pixels)) return false;
    if (missing) created.push_back(path);
    return true;
  }
  void Keep(const image::Pixels &pixels, ImagePendingFiles *parent = nullptr) {
    auto path = ConfigDirectory() + image::Name(pixels);
    auto found = std::find(created.begin(), created.end(), path);
    if (found == created.end()) return;
    if (parent) parent->created.push_back(*found);
    created.erase(found);
  }
  ~ImagePendingFiles() {
    for (const auto &path : created) DeleteFileW(path.c_str());
  }
};

struct ImageSnipFiles {
  std::wstring folder;
  std::vector<std::wstring> before;
  bool ready = false;
  explicit ImageSnipFiles(std::wstring path) : folder(std::move(path)) {
    if (folder.empty()) return;
    WIN32_FIND_DATAW entry{};
    HANDLE search = FindFirstFileW((folder + L"\\*.png").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      ready = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
      return;
    }
    do { before.emplace_back(entry.cFileName); } while (FindNextFileW(search, &entry));
    ready = GetLastError() == ERROR_NO_MORE_FILES;
    FindClose(search);
  }
  bool Cleanup(const image::Pixels &captured) const {
    if (!ready || !image::Dimensions(captured.w, captured.h) ||
        captured.rgb.size() != size_t(captured.w) * captured.h) return false;
    WIN32_FIND_DATAW entry{};
    HANDLE search = FindFirstFileW((folder + L"\\*.png").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool removed = false;
    do {
      if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
          std::any_of(before.begin(), before.end(), [&](const auto &name) {
            return _wcsicmp(name.c_str(), entry.cFileName) == 0;
          })) continue;
      auto source = folder + L"\\" + entry.cFileName;
      // Keep this exact file locked against replacement until verification and deletion finish.
      HANDLE file = CreateFileW(source.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
      if (file == INVALID_HANDLE_VALUE) continue;
      BY_HANDLE_FILE_INFORMATION info{};
      auto pixels = GetFileInformationByHandle(file, &info) &&
                            !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                        ? ImageReadScreenshot(file) : nullptr;
      bool same = pixels && pixels->w == captured.w && pixels->h == captured.h;
      if (same)
        for (size_t i = 0; i < captured.rgb.size(); ++i)
          if ((pixels->rgb[i] & 0xffffff) != (captured.rgb[i] & 0xffffff)) { same = false; break; }
      if (same) {
        if (ImageStoreFile(captured)) {
          FILE_DISPOSITION_INFO disposition{TRUE};
          removed = SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition)) != FALSE;
        }
      }
      CloseHandle(file);
      if (removed) break;
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return removed;
  }
};
