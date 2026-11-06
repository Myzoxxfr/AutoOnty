#pragma once
#include "ui/image_capture.h"
#include <thread>
#include <tuple>
static constexpr DWORD IMAGE_DIALOG_STYLE = WS_POPUP | WS_CLIPCHILDREN;
static constexpr DWORD IMAGE_DIALOG_EX_STYLE = WS_EX_TOOLWINDOW;
static UINT ImageWindowDpi(HWND) { return 96; }
static SIZE ImageDialogSize(UINT) { return {620, 600}; }
static bool ImageNumber(HWND edit, DWORD &out, bool nonzero = false) {
  wchar_t text[32]{};
  GetWindowTextW(edit, text, 32);
  if (!*text || *text == L'-')
    return false;
  wchar_t *end = nullptr;
  errno = 0;
  unsigned long long value = wcstoull(text, &end, 10);
  if (errno == ERANGE || *end || value > MAXDWORD || (nonzero && !value))
    return false;
  out = (DWORD)value;
  return true;
}
struct ImageSourceDlg {
  HWND h = nullptr, owner = nullptr;
  bool done = false;
  int choice = 0, hover = -1;
};
static RECT ImageSourceButton(int i) {
  return {14 + i * 158, 158, 162 + i * 158, 192};
}
static LRESULT CALLBACK ImageSourceProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto d = (ImageSourceDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_NCCREATE) {
    d = (ImageSourceDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    d->h = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
  }
  if (!d)
    return DefWindowProcW(h, m, w, l);
  auto finish = [&](int choice) {
    d->choice = choice;
    d->done = true;
    DestroyWindow(h);
  };
  switch (m) {
  case WM_CREATE:
    ApplyWindowMaterial(h);
    GfxRoundRegion(h);
    return 0;
  case WM_NCHITTEST: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &p);
    auto close = GfxCloseRect(500);
    return p.y < 30 && !PtInRect(&close, p) ? HTCAPTION : HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int hover = -1;
    for (int i = 0; i < 3; ++i) {
      auto r = ImageSourceButton(i);
      if (PtInRect(&r, p))
        hover = i;
    }
    d->hover = hover;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  }
  case WM_LBUTTONUP: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    auto close = GfxCloseRect(500);
    if (PtInRect(&close, p)) {
      finish(0);
      return 0;
    }
    for (int i = 0; i < 3; ++i) {
      auto r = ImageSourceButton(i);
      if (PtInRect(&r, p)) {
        finish(i == 2 ? 0 : i + 1);
        break;
      }
    }
    return 0;
  }
  case WM_KEYDOWN:
    if (w == VK_ESCAPE)
      finish(0);
    else if (w == VK_RETURN)
      finish(1);
    return 0;
  case WM_CLOSE:
    finish(0);
    return 0;
  case WM_PAINT:
    PaintBuffered(h, [&](HDC dc, Gdiplus::Graphics &g, const RECT &) {
      GfxChrome(g, dc, 500, L(L_IMAGE_SOURCE_TITLE), 0, CHB_SEP);
      PutText(g, dc, L(L_IMAGE_SOURCE_HINT), 14, 43, 472, 28, GdiCol(ThText()),
              0, 10, true);
      Gdiplus::Font font(dc, ThFont(9));
      Gdiplus::SolidBrush brush(GdiCol(ThDim()));
      Gdiplus::StringFormat format;
      g.DrawString(L(L_IMAGE_CAPTURE_HINT), -1, &font,
                   Gdiplus::RectF(14, 82, 472, 65), &format, &brush);
      int ids[] = {L_IMAGE_SCREENSHOT, L_IMAGE_FILE, L_CANCEL};
      for (int i = 0; i < 3; ++i)
        PaintBtn(g, dc, ImageSourceButton(i), L(ids[i]), d->hover == i, i == 0,
                 true);
    });
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static std::shared_ptr<image::Pixels> ImageLoadFile(const wchar_t *path) {
  std::unique_ptr<Gdiplus::Bitmap> input(
      Gdiplus::Bitmap::FromFile(path, FALSE));
  std::shared_ptr<image::Pixels> result;
  if (input && input->GetLastStatus() == Gdiplus::Ok &&
      image::Dimensions((int)input->GetWidth(), (int)input->GetHeight())) {
    int width = input->GetWidth(), height = input->GetHeight();
    Gdiplus::Bitmap converted(width, height, PixelFormat32bppARGB);
    {
      Gdiplus::Graphics g(&converted);
      g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
      g.DrawImage(input.get(), 0, 0, width, height);
    }
    Gdiplus::BitmapData data{};
    Gdiplus::Rect rect(0, 0, width, height);
    if (converted.LockBits(&rect, Gdiplus::ImageLockModeRead,
                           PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
      try {
        result = std::make_shared<image::Pixels>();
        result->w = width;
        result->h = height;
        result->rgb.resize(size_t(width) * height);
        for (int y = 0; y < height; ++y)
          memcpy(result->rgb.data() + size_t(y) * width,
                 (BYTE *)data.Scan0 + ptrdiff_t(y) * data.Stride,
                 size_t(width) * 4);
      } catch (const std::bad_alloc &) {
        result.reset();
      }
      converted.UnlockBits(&data);
    }
  }
  return result;
}
static std::shared_ptr<image::Pixels> ImageFile(HWND owner) {
  wchar_t path[32768]{};
  OPENFILENAMEW file{sizeof(file)};
  file.hwndOwner = owner;
  file.lpstrFile = path;
  file.nMaxFile = 32768;
  file.lpstrFilter =
      L"Images (*.bmp;*.png;*.jpg;*.jpeg)\0*.bmp;*.png;*.jpg;*.jpeg\0All "
      L"files\0*.*\0";
  file.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&file))
    return {};
  auto result = ImageLoadFile(path);
  if (!result)
    UiMessage(owner, L(L_IMAGE_IMPORT_ERROR), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
  return result;
}
static std::shared_ptr<image::Pixels> ChooseImage(HWND owner) {
  ModalGuard guard;
  ImageSourceDlg d;
  d.owner = owner;
  WNDCLASSW wc{};
  wc.lpfnWndProc = ImageSourceProc;
  wc.hInstance = g_hInst;
  wc.lpszClassName = L"AutoOntyImageSource";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  int x, y;
  PlaceOver(owner, 500, 206, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, x,
                           y, 500, 206, owner, nullptr, g_hInst, &d);
  if (h) {
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(h);
    RunModal(h, d.done);
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
  if (d.choice == 1)
    return SnipImage(owner);
  if (d.choice == 2)
    return ImageFile(owner);
  return {};
}
struct ImageDialog {
  HWND h = nullptr, owner = nullptr, slider = nullptr, preset = nullptr,
       timeout = nullptr, result = nullptr;
  ImageWindows hidden;
  MAc value;
  bool done = false, ok = false, dedicated = false, startNow = false,
       tracking = false, autoclick = false;
  int page = 0, hover = -1;
  std::vector<std::pair<HWND, int>> pages;
  std::thread worker;
  std::atomic<bool> cancel{false}, ready{false}, captured{false};
  image::Match match;
  bool captureFailed = false;
  ~ImageDialog() {
    cancel = true;
    if (worker.joinable())
      worker.join();
  }
};
enum {
  IM_ACTION = 20,
  IM_TARGET,
  IM_X,
  IM_Y,
  IM_INTERVAL,
  IM_CLICKS,
  IM_AUTO_TIME,
  IM_REPEAT,
  IM_MATCHES,
  IM_DURATION,
  IM_COOLDOWN,
  IM_FOUND,
  IM_TIMEOUT_ACTION,
  IM_SIZES,
  IM_HIDE
};
static void ImagePreset(ImageDialog *d) {
  int value = (int)SendMessageW(d->slider, TBM_GETPOS, 0, 0);
  d->value.sensitivity = value;
  SendMessageW(d->preset, CB_SETCURSEL,
               value == 95   ? 0
               : value == 85 ? 1
               : value == 70 ? 2
                             : 3,
               0);
  InvalidateRect(d->h, nullptr, FALSE);
}
static RECT ImageButton(int id) {
  if (id >= 100 && id < 103)
    return {14 + (id - 100) * 198, 38, 202 + (id - 100) * 198, 68};
  if (id == IDOK)
    return {452, 554, 606, 586};
  if (id == IDCANCEL)
    return {14, 554, 150, 586};
  if (id == 16)
    return {274, 554, 438, 586};
  if (id == 14)
    return {14, 282, 172, 310};
  if (id == 15)
    return {184, 282, 342, 310};
  if (id == 12)
    return {478, 322, 606, 352};
  if (id == 17)
    return {230, 178, 410, 207};
  if (id == 18)
    return {230, 405, 410, 435};
  return {};
}
static bool ImageButtonActive(const ImageDialog *d, int id) {
  if (id >= 100 && id < 103)
    return !d->worker.joinable() && (!d->autoclick || id != 102);
  if (id == IDOK || id == IDCANCEL)
    return true;
  if (id == 16)
    return d->dedicated;
  if (id == 14 || id == 15 || id == 12)
    return d->page == 0;
  int action = d->autoclick ? IA_AUTOCLICK :
      (int)SendMessageW(GetDlgItem(d->h, IM_ACTION), CB_GETCURSEL, 0, 0);
  int target =
      (int)SendMessageW(GetDlgItem(d->h, IM_TARGET), CB_GETCURSEL, 0, 0);
  if (id == 17)
    return d->page == 1 && target == IT_FIXED &&
           (action == IA_CLICK || action == IA_AUTOCLICK);
  return id == 18 && d->page == 1 && action == IA_RUN_MACRO;
}
static bool ImageRead(ImageDialog *d) {
  auto select = [&](int id) {
    return (int)SendMessageW(GetDlgItem(d->h, id), CB_GETCURSEL, 0, 0);
  };
  auto options = d->value.imageOptions;
  int clickMode = select(IM_ACTION);
  if (d->autoclick && (clickMode < 0 || clickMode > 2)) return false;
  options.action = d->autoclick ? IA_AUTOCLICK : clickMode;
  options.target = select(IM_TARGET);
  options.repeat = select(IM_REPEAT);
  options.onFound = select(IM_FOUND);
  options.onTimeout = select(IM_TIMEOUT_ACTION);
  if (options.action < 0 || options.target < 0 || options.repeat < 0 ||
      options.onFound < 0 || options.onTimeout < 0)
    return false;
  auto error = [&](int page) {
    d->page = page;
    SetWindowTextW(d->result, L(L_IMAGE_NUMBER_ERROR));
    return false;
  };
  DWORD timeout, matches, clicks = options.clicks;
  if (!ImageNumber(d->timeout, timeout) ||
      !ImageNumber(GetDlgItem(d->h, IM_MATCHES), matches) ||
      !ImageNumber(GetDlgItem(d->h, IM_DURATION), options.duration) ||
      !ImageNumber(GetDlgItem(d->h, IM_COOLDOWN), options.cooldown))
    return error(2);
  if (options.action == IA_AUTOCLICK &&
      (!ImageNumber(GetDlgItem(d->h, IM_INTERVAL), options.interval, true) ||
       !ImageNumber(GetDlgItem(d->h, IM_AUTO_TIME), options.autoDuration)))
    return error(1);
  if (options.action == IA_AUTOCLICK && (!d->autoclick || clickMode == 1) &&
      !ImageNumber(GetDlgItem(d->h, IM_CLICKS), clicks, d->autoclick)) {
    error(1);
    if (d->autoclick) SetWindowTextW(d->result, L(L_AC_IMAGE_COUNT_ERROR));
    return false;
  }
  if (d->autoclick && clickMode != 1) clicks = clickMode == 0 ? 1 : 0;
  if ((options.action == IA_CLICK || options.action == IA_AUTOCLICK) &&
      options.target == IT_FIXED) {
    auto signedValue = [&](int id, LONG &out) {
      wchar_t text[32]{};
      GetWindowTextW(GetDlgItem(d->h, id), text, 32);
      wchar_t *end;
      errno = 0;
      long long value = wcstoll(text, &end, 10);
      if (!*text || *end || errno == ERANGE || value < LONG_MIN ||
          value > LONG_MAX)
        return false;
      out = (LONG)value;
      return true;
    };
    if (!signedValue(IM_X, options.targetX) ||
        !signedValue(IM_Y, options.targetY))
      return error(1);
  }
  if (options.action == IA_RUN_MACRO && !d->value.imageMacro) {
    d->page = 1;
    SetWindowTextW(d->result, L(L_IMAGE_NO_MACRO));
    return false;
  }
  d->value.imageTimeout = timeout;
  options.maxMatches = matches;
  if (options.action == IA_AUTOCLICK)
    options.clicks = clicks;
  options.differentSizes = SendMessageW(GetDlgItem(d->h, IM_SIZES), BM_GETCHECK,
                                        0, 0) == BST_CHECKED;
  options.hideApp =
      SendMessageW(GetDlgItem(d->h, IM_HIDE), BM_GETCHECK, 0, 0) == BST_CHECKED;
  d->value.imageOptions = options;
  return true;
}
static void ImagePage(ImageDialog *d, bool hint = true) {
  int action =
          d->autoclick ? IA_AUTOCLICK :
          (int)SendMessageW(GetDlgItem(d->h, IM_ACTION), CB_GETCURSEL, 0, 0),
      target =
          (int)SendMessageW(GetDlgItem(d->h, IM_TARGET), CB_GETCURSEL, 0, 0);
  for (auto [h, page] : d->pages) {
    int id = GetDlgCtrlID(h);
    bool show = page == d->page;
    if (id == IM_TARGET)
      show = show && (action == IA_CLICK || action == IA_AUTOCLICK);
    if (id == IM_X || id == IM_Y)
      show = show && (action == IA_CLICK || action == IA_AUTOCLICK) &&
             target == IT_FIXED;
    if (id == IM_INTERVAL || id == IM_CLICKS || id == IM_AUTO_TIME)
      show = show && action == IA_AUTOCLICK;
    if (id == IM_CLICKS && d->autoclick)
      show = show && SendMessageW(GetDlgItem(d->h, IM_ACTION), CB_GETCURSEL, 0, 0) == 1;
    ShowWindow(h, show ? SW_SHOW : SW_HIDE);
  }
  if (hint)
    SetWindowTextW(d->result, L(d->page == 0   ? L_IMAGE_TEST_HINT
                                : d->page == 1 ? L_IMAGE_ACTION_HINT
                                               : L_IMAGE_REPEAT_HINT));
  ShowWindow(d->result, SW_SHOW);
  InvalidateRect(d->h, nullptr, TRUE);
}
static void ImageStopTest(ImageDialog *d) {
  d->cancel = true;
  if (d->worker.joinable())
    d->worker.join();
  d->ready = false;
  d->captured = false;
  d->hidden.Restore();
  d->cancel = false;
  EnableWindow(d->slider, TRUE);
  EnableWindow(d->preset, TRUE);
  EnableWindow(GetDlgItem(d->h, IM_SIZES), TRUE);
}
static void ImagePaint(ImageDialog *d, HDC dc, Gdiplus::Graphics &g) {
  HWND h = d->h;

  GfxChrome(g, dc, 620, L(d->dedicated ? L_IMAGE_MACRO : L_IMAGE), 0, CHB_SEP);
  int tabs[] = {L_IMAGE_PAGE, L_IMAGE_ACTIONS, L_IMAGE_WAITING};
  for (int i = 0; i < (d->autoclick ? 2 : 3); ++i)
    PaintBtn(g, dc, ImageButton(100 + i), L(tabs[i]), d->hover == 100 + i,
             d->page == i, !d->worker.joinable());
  auto label = [&](int id, int y, int width = 370) {
    PutText(g, dc, L(id), 14, (REAL)y, (REAL)width, 26, GdiCol(ThText()));
  };
  if (d->page == 0) {
    RECT frame{14, 80, 606, 270};
    FrameRect(g, frame);
    if (d->value.image) {
      auto &p = *d->value.image;
      double scale = (std::min)(1.0, (std::min)(574.0 / p.w, 174.0 / p.h));
      int width = (int)(p.w * scale), height = (int)(p.h * scale);
      Gdiplus::Bitmap bitmap(p.w, p.h, p.w * 4, PixelFormat32bppRGB,
                             (BYTE *)p.rgb.data());
      g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
      g.DrawImage(&bitmap, 310 - width / 2, 175 - height / 2, width, height);
    }
    PaintBtn(g, dc, ImageButton(14), L(L_RETAKE), d->hover == 14, false, true);
    PaintBtn(g, dc, ImageButton(15), L(L_IMAGE_FILE), d->hover == 15, false,
             true);
    std::wstring sensitivity = std::wstring(L(L_SENSITIVITY)) + L" " +
                               std::to_wstring(d->value.sensitivity);
    PutText(g, dc, sensitivity, 14, 324, 112, 26, GdiCol(ThText()));
    PaintBtn(g, dc, ImageButton(12), L(L_IMAGE_TEST), d->hover == 12, false,
             !d->worker.joinable());
    PutText(g, dc, std::wstring(L(L_IMAGE_BACKEND)) + L": " + image::Backend(),
            340, 366, 265, 26, GdiCol(ThDim()));
  } else if (d->page == 1) {
    label(L_IMAGE_DO, 92, 205);
    int action =
            d->autoclick ? IA_AUTOCLICK :
            (int)SendMessageW(GetDlgItem(h, IM_ACTION), CB_GETCURSEL, 0, 0),
        target =
            (int)SendMessageW(GetDlgItem(h, IM_TARGET), CB_GETCURSEL, 0, 0);
    if (action == IA_CLICK || action == IA_AUTOCLICK) {
      label(L_IMAGE_TARGET, 136, 205);
      if (target == IT_FIXED) {
        PutText(g, dc, L"X", 14, 180, 35, 24, GdiCol(ThDim()));
        PutText(g, dc, L"Y", 140, 180, 15, 24, GdiCol(ThDim()));
        PaintBtn(g, dc, ImageButton(17), L(L_IMAGE_PICK), d->hover == 17, false,
                 true);
      }
    }
    if (action == IA_AUTOCLICK) {
      label(L_IMAGE_INTERVAL, 226);
      if (!d->autoclick || SendMessageW(GetDlgItem(h, IM_ACTION), CB_GETCURSEL, 0, 0) == 1)
        label(d->autoclick ? L_AC_REPEAT_COUNT : L_IMAGE_CLICK_COUNT, 270);
      label(L_IMAGE_AUTO_DURATION, 314);
    }
    if (action == IA_RUN_MACRO) {
      label(L_IMAGE_CHOOSE_MACRO, 368, 205);
      PutText(g, dc, d->value.imageMacroName, 230, 368, 360, 26,
              GdiCol(ThText()));
      PaintBtn(g, dc, ImageButton(18), L(L_IMAGE_CHOOSE_MACRO), d->hover == 18,
               false, true);
    }
  } else {
    label(L_IMAGE_TIMEOUT, 94);
    PutText(g, dc, L(L_IMAGE_TIMEOUT_HINT), 14, 126, 592, 35, GdiCol(ThDim()));
    label(L_IMAGE_REPEATS, 186, 205);
    label(L_IMAGE_MAX_MATCHES, 232);
    label(L_IMAGE_DURATION, 276);
    label(L_IMAGE_COOLDOWN, 320);
    label(L_IMAGE_FOUND_OUTCOME, 364, 205);
    label(L_IMAGE_TIMEOUT_OUTCOME, 408, 205);
  }
  for (auto [child, page] : d->pages)
    if ((GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0) {
      wchar_t cls[20]{};
      GetClassNameW(child, cls, 20);
      if (_wcsicmp(cls, L"Edit") == 0) {
        RECT field{};
        GetWindowRect(child, &field);
        MapWindowPoints(nullptr, h, (LPPOINT)&field, 2);
        InflateRect(&field, 3, 2);
        FrameRect(g, field);
      }
    }
  PaintBtn(g, dc, ImageButton(IDCANCEL), L(L_CANCEL), d->hover == IDCANCEL,
           false, true);
  if (d->dedicated)
    PaintBtn(g, dc, ImageButton(16), L(L_IMAGE_SAVE_ONLY), d->hover == 16,
             false, true);
  PaintBtn(g, dc, ImageButton(IDOK),
           L(d->dedicated ? L_IMAGE_START : L_USE_IMAGE), d->hover == IDOK,
           true, true);
}
static LRESULT CALLBACK ImageFieldProc(HWND h, UINT m, WPARAM w, LPARAM l,
                                       UINT_PTR id, DWORD_PTR kind) {
  if (m == WM_NCDESTROY) {
    RemoveWindowSubclass(h, ImageFieldProc, id);
    return DefSubclassProc(h, m, w, l);
  }
  if ((m == WM_PAINT && kind != 0) || m == WM_PRINT || m == WM_PRINTCLIENT) {
    PAINTSTRUCT paint{};
    HDC dc = m == WM_PAINT ? BeginPaint(h, &paint) : (HDC)w;
    RECT bounds{};
    GetClientRect(h, &bounds);
    {
      Gdiplus::Graphics g(dc);
      Gdiplus::SolidBrush background(
          GdiCol(kind == 2 ? ThBg() : FieldCol(false)));
      g.FillRectangle(&background, 0, 0, (int)bounds.right, (int)bounds.bottom);
      std::wstring text;
      if (kind == 1) {
        int selected = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
        if (selected >= 0) {
          int length = (int)SendMessageW(h, CB_GETLBTEXTLEN, selected, 0);
          if (length >= 0 && length <= 65536) {
            text.resize(length + 1);
            SendMessageW(h, CB_GETLBTEXT, selected, (LPARAM)text.data());
            text.resize(length);
          }
        }
      } else {
        int length = GetWindowTextLengthW(h);
        text.resize(length + 1);
        GetWindowTextW(h, text.data(), length + 1);
        text.resize(length);
      }
      if (kind == 2) {
        RECT check{1, 5, 17, 21};
        Gdiplus::SolidBrush fill(
            GdiCol(SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED
                       ? ThAccent()
                       : FieldCol(false)));
        g.FillRectangle(&fill, 1, 5, 16, 16);
        Gdiplus::Pen border(GdiCol(ThBorder()));
        g.DrawRectangle(&border, 1, 5, 16, 16);
        if (SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED) {
          Gdiplus::Pen tick(Gdiplus::Color(255, 255, 255), 1.7f);
          g.DrawLine(&tick, 4, 12, 8, 16);
          g.DrawLine(&tick, 8, 16, 14, 8);
        }
        PutText(g, dc, text, 25, 0, (REAL)bounds.right - 28,
                (REAL)bounds.bottom, GdiCol(ThText()));
      } else {
        PutText(g, dc, text, 6, 0, (REAL)bounds.right - (kind == 1 ? 30 : 10),
                (REAL)bounds.bottom, GdiCol(ThText()));
        if (kind == 1) {
          Gdiplus::Pen border(
              GdiCol(GetFocus() == h ? ThAccent() : ThBorder()));
          g.DrawRectangle(&border, 0, 0, (int)bounds.right - 1,
                          (int)bounds.bottom - 1);
          Gdiplus::SolidBrush arrow(GdiCol(ThDim()));
          Gdiplus::Point triangle[] = {
              {bounds.right - 18, bounds.bottom / 2 - 2},
              {bounds.right - 10, bounds.bottom / 2 - 2},
              {bounds.right - 14, bounds.bottom / 2 + 2}};
          g.FillPolygon(&arrow, triangle, 3);
        }
      }
    }
    if (m == WM_PAINT)
      EndPaint(h, &paint);
    return 0;
  }
  return DefSubclassProc(h, m, w, l);
}
static LRESULT CALLBACK ImageProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto d = (ImageDialog *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_NCCREATE) {
    d = (ImageDialog *)((CREATESTRUCTW *)l)->lpCreateParams;
    d->h = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
  }
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_CREATE: {
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common))
      return -1;
    auto control = [&](const wchar_t *cls, const std::wstring &text,
                       DWORD style, int x, int y, int width, int height, int id,
                       int page) {
      HWND c =
          CreateWindowW(cls, text.c_str(), WS_CHILD | WS_TABSTOP | style, x, y,
                        width, height, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
      SendMessageW(c, WM_SETFONT, (WPARAM)ThFont(10), TRUE);
      if (wcscmp(cls, L"COMBOBOX") == 0)
        SetWindowSubclass(c, ImageFieldProc, 1, 1);
      else if (wcscmp(cls, L"EDIT") == 0)
        SetWindowSubclass(c, ImageFieldProc, 1, 0);
      else if (wcscmp(cls, L"BUTTON") == 0 &&
               (style & BS_AUTOCHECKBOX) == BS_AUTOCHECKBOX)
        SetWindowSubclass(c, ImageFieldProc, 1, 2);
      SetWindowTheme(c, IsLight() ? L"Explorer" : L"DarkMode_Explorer",
                     nullptr);
      d->pages.push_back({c, page});
      return c;
    };
    auto edit = [&](int id, DWORD value, int x, int y, int width, int page) {
      HWND c = control(L"EDIT", std::to_wstring(value),
                       ES_AUTOHSCROLL | ES_NUMBER, x, y, width, 24, id, page);
      SendMessageW(c, EM_SETLIMITTEXT, 10, 0);
      return c;
    };
    auto combo = [&](int id, std::initializer_list<int> strings, int selected,
                     int x, int y, int width, int page) {
      auto c = control(L"COMBOBOX", L"",
                       CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                       x, y, width, 220, id, page);
      for (auto string : strings)
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)L(string));
      SendMessageW(c, CB_SETCURSEL, selected, 0);
      SendMessageW(c, CB_SETITEMHEIGHT, -1, 24);
      SendMessageW(c, CB_SETITEMHEIGHT, 0, 24);
      return c;
    };
    d->slider =
        control(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 126, 322, 340, 32, 10, 0);
    SendMessageW(d->slider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessageW(d->slider, TBM_SETPOS, TRUE, d->value.sensitivity);
    d->preset =
        combo(11, {L_STRICT, L_NORMAL, L_LOOSE, L_CUSTOM}, 1, 130, 366, 190, 0);
    auto &o = d->value.imageOptions;
    auto sizes = control(L"BUTTON", L(L_IMAGE_SIZE_OPTION), BS_AUTOCHECKBOX, 14,
                         418, 580, 26, IM_SIZES, 0);
    SendMessageW(sizes, BM_SETCHECK,
                 o.differentSizes ? BST_CHECKED : BST_UNCHECKED, 0);
    auto hide = control(L"BUTTON", L(L_IMAGE_HIDE), BS_AUTOCHECKBOX, 14, 450,
                        580, 26, IM_HIDE, 0);
    SendMessageW(hide, BM_SETCHECK, o.hideApp ? BST_CHECKED : BST_UNCHECKED, 0);
    if (d->autoclick)
      combo(IM_ACTION, {L_IMAGE_CLICK, L_AC_IMAGE_COUNT, L_AC_IMAGE_CONTINUOUS},
            o.clicks == 1 ? 0 : o.clicks ? 1 : 2, 230, 90, 360, 1);
    else
      combo(IM_ACTION,
            {L_IMAGE_CLICK, L_IMAGE_AUTOCLICK, L_IMAGE_RUN, L_IMAGE_WAIT_ONLY},
            o.action, 230, 90, 360, 1);
    combo(IM_TARGET, {L_IMAGE_CENTER, L_IMAGE_CURSOR, L_IMAGE_FIXED}, o.target,
          230, 134, 360, 1);
    for (auto [id, value, x] : {std::tuple<int, LONG, int>{IM_X, o.targetX, 52},
                                {IM_Y, o.targetY, 158}}) {
      auto c = control(L"EDIT", std::to_wstring(value), ES_AUTOHSCROLL, x, 180,
                       88, 24, id, 1);
      SendMessageW(c, EM_SETLIMITTEXT, 11, 0);
    }
    edit(IM_INTERVAL, o.interval, 390, 224, 200, 1);
    edit(IM_CLICKS, o.clicks, 390, 268, 200, 1);
    edit(IM_AUTO_TIME, o.autoDuration, 390, 312, 200, 1);
    d->timeout = edit(13, d->value.imageTimeout, 390, 92, 200, 2);
    combo(IM_REPEAT,
          {L_IMAGE_ONCE, L_IMAGE_REAPPEAR, L_IMAGE_REPEAT, L_IMAGE_LOCATIONS},
          o.repeat, 230, 184, 360, 2);
    edit(IM_MATCHES, o.maxMatches, 390, 230, 200, 2);
    edit(IM_DURATION, o.duration, 390, 274, 200, 2);
    edit(IM_COOLDOWN, o.cooldown, 390, 318, 200, 2);
    combo(IM_FOUND, {L_IMAGE_CONTINUE, L_IMAGE_PAUSE, L_IMAGE_STOP}, o.onFound,
          230, 362, 360, 2);
    combo(IM_TIMEOUT_ACTION, {L_IMAGE_CONTINUE, L_IMAGE_PAUSE, L_IMAGE_STOP},
          o.onTimeout, 230, 406, 360, 2);
    d->result = control(L"STATIC", L(L_IMAGE_TEST_HINT), SS_LEFT, 14, 490, 592,
                        48, 19, 3);
    ImagePreset(d);
    ImagePage(d);
    ApplyWindowMaterial(h);
    GfxRoundRegion(h);
    SetTimer(h, 1, 30, nullptr);
    return 0;
  }
  case WM_MEASUREITEM: {
    auto measure = (MEASUREITEMSTRUCT *)l;
    if (measure->CtlType == ODT_COMBOBOX) {
      measure->itemHeight = 24;
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    auto item = (DRAWITEMSTRUCT *)l;
    if (item->CtlType != ODT_COMBOBOX)
      break;
    Gdiplus::Graphics g(item->hDC);
    Gdiplus::SolidBrush brush(
        GdiCol((item->itemState & ODS_SELECTED) ? ThHover() : FieldCol(false)));
    g.FillRectangle(&brush, (int)item->rcItem.left, (int)item->rcItem.top,
                    (int)(item->rcItem.right - item->rcItem.left),
                    (int)(item->rcItem.bottom - item->rcItem.top));
    std::wstring text;
    if (item->itemID != (UINT)-1) {
      int length =
          (int)SendMessageW(item->hwndItem, CB_GETLBTEXTLEN, item->itemID, 0);
      if (length >= 0 && length <= 65536) {
        text.resize(length + 1);
        SendMessageW(item->hwndItem, CB_GETLBTEXT, item->itemID,
                     (LPARAM)text.data());
        text.resize(length);
      }
    }
    PutText(g, item->hDC, text, (REAL)item->rcItem.left + 6,
            (REAL)item->rcItem.top,
            (REAL)(item->rcItem.right - item->rcItem.left - 12),
            (REAL)(item->rcItem.bottom - item->rcItem.top), GdiCol(ThText()));
    return TRUE;
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORLISTBOX: {
    auto dc = (HDC)w;
    SetTextColor(dc, ThText());
    SetBkColor(dc, ThBg());
    static HBRUSH brush = nullptr;
    static COLORREF previous = 0;
    if (!brush || previous != ThBg()) {
      if (brush)
        DeleteObject(brush);
      previous = ThBg();
      brush = CreateSolidBrush(previous);
    }
    return (LRESULT)brush;
  }
  case WM_HSCROLL:
    ImagePreset(d);
    return 0;
  case WM_TIMER:
    if (d->captured.exchange(false))
      d->hidden.Restore();
    if (d->ready.exchange(false)) {
      if (d->worker.joinable())
        d->worker.join();
      d->hidden.Restore();
      std::wstring text;
      if (d->captureFailed)
        text = L(L_IMAGE_TEST_ERROR);
      else if (d->match.failed)
        text = L(L_IMAGE_MATCH_ERROR);
      else if (d->match.found) {
        text = L(L_IMAGE_FOUND);
        auto pos = text.find(L"%.1f%%");
        if (pos != text.npos) {
          wchar_t number[32];
          swprintf_s(number, L"%.1f%%", d->match.score);
          text.replace(pos, 6, number);
        }
      } else {
        wchar_t confidence[32];
        swprintf_s(confidence, L"%.1f%%",
                   std::floor(d->match.score * 10 + 1e-8) / 10);
        text = std::wstring(L(L_IMAGE_TEST_LOW)) + confidence;
      }
      SetWindowTextW(d->result, text.c_str());
      EnableWindow(d->slider, TRUE);
      EnableWindow(d->preset, TRUE);
      EnableWindow(GetDlgItem(d->h, IM_SIZES), TRUE);
      InvalidateRect(h, nullptr, FALSE);
    }
    return 0;
  case WM_NCHITTEST: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &p);
    auto close = GfxCloseRect(620);
    if (p.y < 30 && !PtInRect(&close, p))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_DPICHANGED: {
    if (l) {
      auto position = (RECT *)l;
      SetWindowPos(h, nullptr, position->left, position->top, 620, 600,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return 0;
  }
  case WM_COMMAND: {
    int id = LOWORD(w);
    if (id == 11 && HIWORD(w) == CBN_SELCHANGE) {
      int i = (int)SendMessageW(d->preset, CB_GETCURSEL, 0, 0);
      if (i >= 0 && i < 3) {
        int values[] = {95, 85, 70};
        SendMessageW(d->slider, TBM_SETPOS, TRUE, values[i]);
        ImagePreset(d);
      }
    }
    if (HIWORD(w) == CBN_SELCHANGE && (id == IM_ACTION || id == IM_TARGET))
      ImagePage(d);
    if (id == IDOK || id == 16) {
      if (ImageRead(d)) {
        d->startNow = d->dedicated && id == IDOK;
        d->ok = true;
        SendMessageW(h, WM_CLOSE, 0, 0);
      } else
        ImagePage(d, false);
    }
    if (id == IDCANCEL)
      SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  }
  case DM_GETDEFID:
    return MAKELONG(IDOK, DC_HASDEFID);
  case WM_KEYDOWN:
    if (w == VK_ESCAPE)
      SendMessageW(h, WM_CLOSE, 0, 0);
    else if (w == VK_RETURN)
      SendMessageW(h, WM_COMMAND, IDOK, 0);
    return 0;
  case WM_MOUSEMOVE: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int hover = -1;
    for (int id : {100, 101, 102, IDOK, IDCANCEL, 16, 14, 15, 12, 17, 18}) {
      auto r = ImageButton(id);
      if (ImageButtonActive(d, id) && PtInRect(&r, p))
        hover = id;
    }
    d->hover = hover;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  }
  case WM_LBUTTONUP: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    auto close = GfxCloseRect(620);
    if (PtInRect(&close, p)) {
      SendMessageW(h, WM_CLOSE, 0, 0);
      return 0;
    }
    int id = -1;
    for (int button : {100, 101, 102, IDOK, IDCANCEL, 16, 14, 15, 12, 17, 18}) {
      auto r = ImageButton(button);
      if (ImageButtonActive(d, button) && PtInRect(&r, p))
        id = button;
    }
    if (id >= 100) {
      if (!d->worker.joinable()) {
        d->page = id - 100;
        ImagePage(d);
      }
      return 0;
    }
    if (id == IDOK || id == IDCANCEL || (id == 16 && d->dedicated)) {
      SendMessageW(h, WM_COMMAND, id, 0);
      return 0;
    }
    if (d->page == 0 && (id == 14 || id == 15)) {
      ImageStopTest(d);
      auto image = id == 15 ? ImageFile(h) : ChooseImage(h);
      if (image) {
        d->value.image = image;
        SetWindowTextW(d->result, L(L_IMAGE_TEST_HINT));
        InvalidateRect(h, nullptr, TRUE);
      }
      return 0;
    }
    if (d->page == 0 && id == 12 && !d->worker.joinable()) {
      d->cancel = false;
      d->captureFailed = false;
      d->hidden.Hide(h);
      EnableWindow(d->slider, FALSE);
      EnableWindow(d->preset, FALSE);
      EnableWindow(GetDlgItem(d->h, IM_SIZES), FALSE);
      SetWindowTextW(d->result, L"...");
      auto snippet = d->value.image;
      int sensitivity = d->value.sensitivity;
      bool sizes = SendMessageW(GetDlgItem(h, IM_SIZES), BM_GETCHECK, 0, 0) ==
                   BST_CHECKED;
      try {
        d->worker = std::thread([d, snippet, sensitivity, sizes] {
          try {
            Sleep(200);
            if (!d->cancel) {
              int x, y;
              auto screen = image::Screen(x, y);
              d->captured = true;
              d->captureFailed = !screen;
              d->match =
                  screen ? image::Scan(*screen, *snippet, sensitivity, sizes,
                                       [d] { return !d->cancel.load(); })
                         : image::Match{};
            }
          } catch (...) {
            d->captureFailed = true;
          }
          d->ready = true;
        });
      } catch (...) {
        d->captureFailed = true;
        d->ready = true;
      }
      return 0;
    }
    if (d->page == 1 && id == 17) {
      int vk = 0;
      bool ext = false;
      MAc input;
      if (CaptureKey(h, L(L_IMAGE_PICK), vk, ext, &input) &&
          input.type == A_MD) {
        SetWindowTextW(GetDlgItem(h, IM_X), std::to_wstring(input.x).c_str());
        SetWindowTextW(GetDlgItem(h, IM_Y), std::to_wstring(input.y).c_str());
      }
      return 0;
    }
    if (d->page == 1 && id == 18) {
      wchar_t path[32768]{};
      OPENFILENAMEW file{sizeof(file)};
      file.hwndOwner = h;
      file.lpstrFile = path;
      file.nMaxFile = 32768;
      file.lpstrFilter = L"AutoOnty macros (*.onty;*.json)\0*.onty;*.json\0";
      auto directory = Macros_Dir();
      file.lpstrInitialDir = directory.c_str();
      file.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
      if (GetOpenFileNameW(&file)) {
        auto child = std::make_shared<std::vector<MAc>>();
        std::function<bool(const std::vector<MAc> &, unsigned)> fits =
            [&](const std::vector<MAc> &events, unsigned depth) {
              if (depth > 8)
                return false;
              for (const auto &event : events)
                if (event.imageMacro && !fits(*event.imageMacro, depth + 1))
                  return false;
              return true;
            };
        if (Engine_ReadFile(path, *child) && !child->empty() &&
            fits(*child, 1)) {
          d->value.imageMacro = child;
          d->value.imageMacroName = Macros_Stem(path);
          SetWindowTextW(d->result, L(L_IMAGE_TEST_HINT));
          InvalidateRect(h, nullptr, FALSE);
        } else
          SetWindowTextW(d->result, L(L_IMAGE_MACRO_ERROR));
      }
      return 0;
    }
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC dc, Gdiplus::Graphics &g, const RECT &) {
      ImagePaint(d, dc, g);
    });
    return 0;
  case WM_PRINTCLIENT: {
    auto dc = (HDC)w;
    Gdiplus::Graphics g(dc);
    Gdiplus::SolidBrush background(GdiCol(ThBg()));
    g.FillRectangle(&background, 0, 0, 620, 600);
    ImagePaint(d, dc, g);
    return 0;
  }
  case WM_DESTROY:
    if (imageSnipping)
      imageSnipCancelled = true;
    d->cancel = true;
    if (d->worker.joinable())
      d->worker.join();
    d->done = true;
    return 0;
  case WM_CLOSE:
    d->cancel = true;
    if (d->worker.joinable())
      d->worker.join();
    d->hidden.Restore();
    d->done = true;
    DestroyWindow(h);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static bool EditImage(HWND owner, MAc &value, bool capture,
                      bool dedicated = false, bool *startNow = nullptr,
                      bool autoclick = false) {
  ModalGuard guard;
  ImageDialog d;
  d.owner = owner;
  d.value = value;
  d.dedicated = dedicated;
  d.autoclick = autoclick;
  if (capture) {
    d.value.image = ChooseImage(owner);
    if (!d.value.image)
      return false;
  }
  if (!d.value.image)
    return false;
  WNDCLASSW wc{};
  wc.lpfnWndProc = ImageProc;
  wc.hInstance = g_hInst;
  wc.lpszClassName = L"AutoOntyImage";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  int x, y;
  PlaceOver(owner, 620, 600, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(IMAGE_DIALOG_EX_STYLE, wc.lpszClassName, L"",
                           IMAGE_DIALOG_STYLE, x, y, 620, 600, owner, nullptr,
                           g_hInst, &d);
  if (h) {
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(d.slider);
    MSG message;
    while (!d.done && IsWindow(h)) {
      BOOL result = GetMessageW(&message, nullptr, 0, 0);
      if (result <= 0) {
        if (!result)
          PostQuitMessage((int)message.wParam);
        SendMessageW(h, WM_CLOSE, 0, 0);
        break;
      }
      if (!IsDialogMessageW(h, &message)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
  }
  EnableWindow(owner, TRUE);
  SetForegroundWindow(owner);
  if (!d.ok)
    return false;
  d.value.type = A_IMAGE;
  d.value.imageFile = image::Name(*d.value.image);
  value = d.value;
  if (startNow)
    *startNow = d.startNow;
  return true;
}
