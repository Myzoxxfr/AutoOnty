#include "ui/macros.h"
#include "core/config.h"
#include "core/editorprefs.h"
#include "core/macrometa.h"
#include "engine/engine.h"
#include "lang/lang.h"
#include "ui/editor_model.h"
#include "ui/gfx.h"
#include "ui/msgbox.h"
#include "ui/statusbar.h"
#include <algorithm>
#include <cerrno>
#include <commctrl.h>
#include <commdlg.h>
#include <cwctype>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <vector>
#include <windows.h>
#include <windowsx.h>

#ifndef CSIDL_PERSONAL
#define CSIDL_PERSONAL 0x0005
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

static int g_modal = 0;
bool Macros_IsModalOpen() { return g_modal > 0; }

struct ModalGuard {
  ModalGuard() { g_modal++; }
  ~ModalGuard() { g_modal--; }
};

static const int MAX_EDIT_EVENTS = 100000; // same cap the engine uses

std::wstring Macros_Dir() {
  wchar_t docs[MAX_PATH] = L"";
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr,
                              SHGFP_TYPE_CURRENT, docs)) ||
      !docs[0]) {
    wchar_t prof[MAX_PATH] = L"";
    if (GetEnvironmentVariableW(L"USERPROFILE", prof, MAX_PATH) > 0)
      swprintf_s(docs, L"%s\\Documents", prof);
  }
  std::wstring dir = std::wstring(docs) + L"\\AutoOnty";
  CreateDirectoryW(dir.c_str(), nullptr); // fine if it already exists
  return dir;
}

std::wstring Macros_Stem(const std::wstring &path) {
  size_t s = path.find_last_of(L"\\/");
  std::wstring f = (s == std::wstring::npos) ? path : path.substr(s + 1);
  size_t dot = f.find_last_of(L'.');
  if (dot != std::wstring::npos && dot > 0)
    f.erase(dot);
  return f;
}

std::wstring Macros_CurrentLabel() {
  std::wstring s = L(L_CUR_MACRO);
  s += L' ';
  if (!g_lastFile.empty())
    s += Macros_Stem(g_lastFile);
  else if (!g_events.empty())
    s += L(L_NEWMACRO);
  else
    s += L(L_MC_NONE);
  return s;
}

static bool SamePath(const std::wstring &a, const std::wstring &b) {
  return !a.empty() && !b.empty() && _wcsicmp(a.c_str(), b.c_str()) == 0;
}

static std::wstring DirOf(const std::wstring &p) {
  size_t s = p.find_last_of(L"\\/");
  return s == std::wstring::npos ? std::wstring() : p.substr(0, s);
}

static void TrimName(std::wstring &o) {
  while (!o.empty() && (o.back() == L' ' || o.back() == L'.'))
    o.pop_back();
  size_t b = 0;
  while (b < o.size() && o[b] == L' ')
    b++;
  o.erase(0, b);
}

// Turns whatever the user typed into something that is safe as a file name.
static std::wstring CleanName(const std::wstring &in) {
  std::wstring o;
  for (wchar_t c : in) {
    if (c < 32 || wcschr(L"<>:\"/\\|?*", c))
      continue;
    o += c;
  }
  TrimName(o);
  if (o.size() > 80)
    o.resize(80);
  TrimName(o);
  if (o.empty())
    return o;
  std::wstring head = o.substr(0, o.find(L'.'));
  for (auto &ch : head)
    ch = (wchar_t)towupper(ch);
  bool reserved = head == L"CON" || head == L"PRN" || head == L"AUX" ||
                  head == L"NUL";
  if (head.size() == 4 && (head.compare(0, 3, L"COM") == 0 ||
                           head.compare(0, 3, L"LPT") == 0) &&
      head[3] >= L'1' && head[3] <= L'9')
    reserved = true;
  if (reserved)
    o = L"_" + o;
  return o;
}

static std::vector<std::wstring> ListMacros() {
  std::vector<std::wstring> v;
  std::wstring dir = Macros_Dir();
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((dir + L"\\*.onty").c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        v.push_back(dir + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(v.begin(), v.end(),
            [](const std::wstring &a, const std::wstring &b) {
              return _wcsicmp(Macros_Stem(a).c_str(),
                              Macros_Stem(b).c_str()) < 0;
            });
  return v;
}

static void PlaceOver(HWND owner, int W, int H, int &x, int &y) {
  RECT rw = {0, 0, 0, 0};
  if (owner)
    GetWindowRect(owner, &rw);
  x = rw.left + ((rw.right - rw.left) - W) / 2;
  y = rw.top + ((rw.bottom - rw.top) - H) / 2;
  HMONITOR mon = MonitorFromRect(&rw, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(mon, &mi)) {
    if (x + W > mi.rcWork.right)
      x = mi.rcWork.right - W;
    if (y + H > mi.rcWork.bottom)
      y = mi.rcWork.bottom - H;
    if (x < mi.rcWork.left)
      x = mi.rcWork.left;
    if (y < mi.rcWork.top)
      y = mi.rcWork.top;
  }
}

static void RunModal(HWND h, const bool &done) {
  MSG m;
  while (!done && IsWindow(h)) {
    BOOL r = GetMessageW(&m, nullptr, 0, 0);
    if (r == 0) {
      PostQuitMessage((int)m.wParam);
      break;
    }
    if (r < 0)
      break;
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
}

static bool IsLight() { return g_cfg.theme == THEME_LIGHT; }
static bool IsAcry() { return g_cfg.theme == THEME_ACRYLIC; }

static COLORREF FieldCol(bool focus) {
  if (IsLight())
    return focus ? RGB(225, 225, 225) : RGB(238, 238, 238);
  if (IsAcry())
    return focus ? RGB(34, 34, 34) : RGB(21, 21, 21);
  return focus ? RGB(46, 46, 46) : RGB(37, 37, 37);
}

static Gdiplus::Color SelCol() {
  if (IsLight())
    return Gdiplus::Color(255, 80, 155, 210);
  if (IsAcry())
    return Gdiplus::Color(255, 60, 120, 180);
  return Gdiplus::Color(255, 55, 125, 185);
}

static Gdiplus::Color BtnFill(bool hover, bool primary) {
  if (primary) {
    if (IsAcry())
      return hover ? Gdiplus::Color(140, 80, 160, 224)
                   : Gdiplus::Color(100, 70, 140, 200);
    if (IsLight())
      return hover ? Gdiplus::Color(255, 90, 170, 220)
                   : Gdiplus::Color(255, 80, 155, 210);
    return hover ? Gdiplus::Color(255, 70, 145, 205)
                 : Gdiplus::Color(255, 55, 125, 185);
  }
  if (IsAcry())
    return hover ? Gdiplus::Color(70, 70, 70, 70)
                 : Gdiplus::Color(40, 50, 50, 50);
  if (IsLight())
    return hover ? Gdiplus::Color(230, 220, 220, 220)
                 : Gdiplus::Color(255, 245, 245, 245);
  return hover ? Gdiplus::Color(255, 60, 60, 60)
               : Gdiplus::Color(255, 42, 42, 42);
}

// 0 = left, 1 = centre, 2 = right
static void PutText(Gdiplus::Graphics &g, HDC dc, const std::wstring &s,
                    REAL x, REAL y, REAL w, REAL h, Gdiplus::Color col,
                    int align = 0, int pt = 9, bool bold = false) {
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  Gdiplus::Font f(dc, ThFont(pt, bold));
  Gdiplus::StringFormat sf;
  sf.SetAlignment(align == 1   ? Gdiplus::StringAlignmentCenter
                  : align == 2 ? Gdiplus::StringAlignmentFar
                               : Gdiplus::StringAlignmentNear);
  sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
  sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  Gdiplus::SolidBrush b(col);
  g.DrawString(s.c_str(), -1, &f, Gdiplus::RectF(x, y, w, h), &sf, &b);
}

static void PaintBtn(Gdiplus::Graphics &g, HDC mem, const RECT &r,
                     const wchar_t *text, bool hover, bool primary,
                     bool enabled) {
  g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
  Gdiplus::SolidBrush fill(BtnFill(hover && enabled, primary && enabled));
  g.FillRectangle(&fill, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                  (REAL)(r.bottom - r.top));
  Gdiplus::Pen pen(GdiCol(ThBorder()));
  g.DrawRectangle(&pen, (int)r.left, (int)r.top, (int)(r.right - r.left - 1),
                  (int)(r.bottom - r.top - 1));
  Gdiplus::Color tc = (primary && enabled) ? Gdiplus::Color(240, 245, 250)
                                           : GdiCol(enabled ? ThText()
                                                            : ThDim());
  PutText(g, mem, text, (REAL)r.left + 4, (REAL)r.top,
          (REAL)(r.right - r.left - 8), (REAL)(r.bottom - r.top), tc, 1, 9);
}

template <class F> static void PaintBuffered(HWND h, F body) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(h, &ps);
  RECT rc;
  GetClientRect(h, &rc);
  HDC mem = CreateCompatibleDC(hdc);
  HBITMAP bm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
  HGDIOBJ old = SelectObject(mem, bm);
  {
    Gdiplus::Graphics g(mem);
    bool acry = IsAcry();
    Gdiplus::SolidBrush bg(acry ? GfxAcrylicBg() : GdiCol(ThBg()));
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.FillRectangle(&bg, 0, 0, rc.right, rc.bottom);
    Gdiplus::Pen pen(acry ? Gdiplus::Color(180, 56, 56, 56)
                          : GdiCol(ThBorder()));
    g.DrawRectangle(&pen, 0, 0, rc.right - 1, rc.bottom - 1);
    body(mem, g, rc);
  }
  BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old);
  DeleteObject(bm);
  DeleteDC(mem);
  EndPaint(h, &ps);
}

static void FrameRect(Gdiplus::Graphics &g, const RECT &r) {
  g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
  Gdiplus::Pen pen(GdiCol(ThBorder()));
  g.DrawRectangle(&pen, (int)r.left, (int)r.top, (int)(r.right - r.left - 1),
                  (int)(r.bottom - r.top - 1));
}

static HBRUSH FieldBrush() {
  static COLORREF cached = (COLORREF)-1;
  static HBRUSH br = nullptr;
  COLORREF c = FieldCol(false);
  if (!br || c != cached) {
    if (br)
      DeleteObject(br);
    br = CreateSolidBrush(c);
    cached = c;
  }
  return br;
}

static void ApplyListTheme(HWND lb) {
  SetWindowTheme(lb, IsLight() ? L"Explorer" : L"DarkMode_Explorer", nullptr);
}

static std::wstring KeyLabel(int vk) {
  switch (vk) {
  case VK_LSHIFT:
    return L"Left Shift";
  case VK_RSHIFT:
    return L"Right Shift";
  case VK_LCONTROL:
    return L"Left Ctrl";
  case VK_RCONTROL:
    return L"Right Ctrl";
  case VK_LMENU:
    return L"Left Alt";
  case VK_RMENU:
    return L"Right Alt";
  case VK_LWIN:
    return L"Left Win";
  case VK_RWIN:
    return L"Right Win";
  case VK_SHIFT:
    return L"Shift";
  case VK_CONTROL:
    return L"Ctrl";
  case VK_MENU:
    return L"Alt";
  case VK_RETURN:
    return L"Enter";
  case VK_SPACE:
    return L"Space";
  case VK_BACK:
    return L"Backspace";
  case VK_TAB:
    return L"Tab";
  case VK_DELETE:
    return L"Delete";
  case VK_INSERT:
    return L"Insert";
  case VK_HOME:
    return L"Home";
  case VK_END:
    return L"End";
  case VK_PRIOR:
    return L"Page Up";
  case VK_NEXT:
    return L"Page Down";
  case VK_LEFT:
    return L"Left";
  case VK_RIGHT:
    return L"Right";
  case VK_UP:
    return L"Up";
  case VK_DOWN:
    return L"Down";
  case VK_APPS:
    return L"Menu";
  case VK_CAPITAL:
    return L"Caps Lock";
  default:
    return FmtHk(0, vk);
  }
}

// ---------------------------------------------------------------------------
// Text prompt (macro name / delay in ms)
// ---------------------------------------------------------------------------

static const int PR_W = 300, PR_H = 114;
enum { PM_OK = WM_APP + 120, PM_CANCEL = WM_APP + 121 };

struct PromptDlg {
  HWND h = nullptr, edit = nullptr, owner = nullptr;
  std::wstring title, prompt, text;
  bool numeric = false;
  int maxLen = 64;
  bool ok = false, done = false, tracking = false, focus = false;
  int hover = 0;
};

static RECT PrBanner() {
  RECT r = {0, 30, PR_W, 56};
  return r;
}
static RECT PrInput() {
  RECT r = {0, 56, PR_W, 84};
  return r;
}
static RECT PrCancel() {
  RECT r = {0, 84, PR_W / 2, PR_H};
  return r;
}
static RECT PrOk() {
  RECT r = {PR_W / 2, 84, PR_W, PR_H};
  return r;
}

static LRESULT CALLBACK PromptEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  WNDPROC old = (WNDPROC)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) {
    PostMessageW(GetParent(h), w == VK_RETURN ? PM_OK : PM_CANCEL, 0, 0);
    return 0;
  }
  if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE))
    return 0; // no "ding"
  return CallWindowProcW(old, h, m, w, l);
}

static LRESULT CALLBACK PromptProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    PromptDlg *nd = (PromptDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
    nd->h = h;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL |
                  (nd->numeric ? ES_NUMBER : 0);
    nd->edit = CreateWindowExW(0, L"EDIT", L"", style, 14, 60, PR_W - 28, 20, h,
                               (HMENU)1, g_hInst, nullptr);
    SendMessageW(nd->edit, WM_SETFONT, (WPARAM)ThFont(10), TRUE);
    SendMessageW(nd->edit, EM_SETLIMITTEXT, (WPARAM)nd->maxLen, 0);
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(nd->edit, GWLP_WNDPROC,
                                             (LONG_PTR)PromptEditProc);
    SetWindowLongPtrW(nd->edit, GWLP_USERDATA, (LONG_PTR)old);
    SetWindowTextW(nd->edit, nd->text.c_str());
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  PromptDlg *d = (PromptDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w;
    SetTextColor(dc, ThText());
    SetBkColor(dc, FieldCol(d->focus));
    static COLORREF bc = (COLORREF)-1;
    static HBRUSH br = nullptr;
    COLORREF c = FieldCol(d->focus);
    if (!br || c != bc) {
      if (br)
        DeleteObject(br);
      br = CreateSolidBrush(c);
      bc = c;
    }
    return (LRESULT)br;
  }
  case PM_OK: {
    wchar_t b[256] = L"";
    GetWindowTextW(d->edit, b, 256);
    std::wstring t = b;
    if (t.find_first_not_of(L' ') == std::wstring::npos) {
      MessageBeep(MB_OK);
      return 0;
    }
    d->text = t;
    d->ok = true;
    d->done = true;
    DestroyWindow(h);
    return 0;
  }
  case PM_CANCEL:
    d->done = true;
    DestroyWindow(h);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = GfxCloseRect(PR_W);
    if (pt.y < 30 && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_COMMAND:
    if (HIWORD(w) == EN_SETFOCUS) {
      d->focus = true;
      InvalidateRect(h, nullptr, FALSE);
    } else if (HIWORD(w) == EN_KILLFOCUS) {
      d->focus = false;
      InvalidateRect(h, nullptr, FALSE);
    }
    return 0;
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT ro = PrOk(), rca = PrCancel(), rcl = GfxCloseRect(PR_W);
    int nh = PtInRect(&ro, pt)    ? 1
             : PtInRect(&rca, pt) ? 2
             : PtInRect(&rcl, pt) ? 3
                                  : 0;
    if (nh != d->hover) {
      d->hover = nh;
      InvalidateRect(h, nullptr, FALSE);
    }
    SetCursor(LoadCursorW(nullptr, nh ? IDC_HAND : IDC_ARROW));
    if (!d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    d->tracking = false;
    d->hover = 0;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT ro = PrOk(), rca = PrCancel(), rcl = GfxCloseRect(PR_W);
    if (PtInRect(&ro, pt))
      PostMessageW(h, PM_OK, 0, 0);
    else if (PtInRect(&rca, pt) || PtInRect(&rcl, pt))
      PostMessageW(h, PM_CANCEL, 0, 0);
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC mem, Gdiplus::Graphics &g, const RECT &rc) {
      GfxChrome(g, mem, rc.right, d->title.c_str(), d->hover == 3 ? 2 : 0,
                CHB_SEP);
      RECT rb = PrBanner();
      Gdiplus::SolidBrush banf(IsAcry() ? Gdiplus::Color(40, 50, 50, 50)
                               : IsLight() ? Gdiplus::Color(245, 245, 245)
                                           : Gdiplus::Color(60, 45, 45, 45));
      g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
      g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
      g.FillRectangle(&banf, (REAL)rb.left, (REAL)rb.top,
                      (REAL)(rb.right - rb.left), (REAL)(rb.bottom - rb.top));
      FrameRect(g, rb);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
      DrawGlyph(mem, g, 16.0f, 43.0f, 11, (wchar_t)0xE946, ThAccent());
      PutText(g, mem, d->prompt, 30.0f, (REAL)rb.top, (REAL)(rb.right - 38),
              (REAL)(rb.bottom - rb.top), GdiCol(ThDim()), 0, 9);
      RECT ri = PrInput();
      Gdiplus::SolidBrush inf(GdiCol(FieldCol(d->focus)));
      g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
      g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
      g.FillRectangle(&inf, (REAL)ri.left, (REAL)ri.top,
                      (REAL)(ri.right - ri.left), (REAL)(ri.bottom - ri.top));
      FrameRect(g, ri);
      PaintBtn(g, mem, PrCancel(), L(L_CANCEL), d->hover == 2, false, true);
      PaintBtn(g, mem, PrOk(), L(L_OK), d->hover == 1, true, true);
    });
    return 0;
  case WM_DESTROY:
    if (d->owner) {
      EnableWindow(d->owner, TRUE);
      SetForegroundWindow(d->owner);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static bool PromptText(HWND owner, const wchar_t *title, const wchar_t *prompt,
                       const std::wstring &init, bool numeric, int maxLen,
                       std::wstring &out) {
  if (!owner)
    owner = g_hwnd;
  PromptDlg d;
  d.title = title;
  d.prompt = prompt;
  d.text = init;
  d.numeric = numeric;
  d.maxLen = maxLen;
  d.owner = owner;
  ModalGuard mg;
  int x, y;
  PlaceOver(owner, PR_W, PR_H, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyPrompt", L"",
                           WS_POPUP | WS_CLIPCHILDREN, x, y, PR_W, PR_H, owner,
                           nullptr, g_hInst, &d);
  if (!h) {
    EnableWindow(owner, TRUE);
    return false;
  }
  GfxRoundRegion(h);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  SetFocus(d.edit);
  SendMessageW(d.edit, EM_SETSEL, 0, -1);
  RunModal(h, d.done);
  if (d.ok)
    out = d.text;
  return d.ok;
}

// ---------------------------------------------------------------------------
// Key capture ("press the key you want")
// ---------------------------------------------------------------------------

static const int KC_W = 300, KC_H = 112;

struct KeyDlg {
  HWND h = nullptr, owner = nullptr;
  std::wstring title;
  int vk = 0;
  bool ext = false;
  bool allowMouse = false;
  int mouseButton = -1;
  POINT mousePosition{};
  bool ok = false, done = false, tracking = false, hoverClose = false;
};

static KeyDlg *g_keyMouseCapture = nullptr;
static const UINT KC_MOUSE_DONE = WM_APP + 42;

static bool CaptureMouseButton(KeyDlg *d, UINT message,
                               const MSLLHOOKSTRUCT &mouse) {
  if (!d || !d->allowMouse || (mouse.flags & LLMHF_INJECTED))
    return false;
  int button = -1;
  bool down = false;
  switch (message) {
  case WM_LBUTTONDOWN:
    down = true;
    [[fallthrough]];
  case WM_LBUTTONUP:
    button = 0;
    break;
  case WM_RBUTTONDOWN:
    down = true;
    [[fallthrough]];
  case WM_RBUTTONUP:
    button = 1;
    break;
  case WM_MBUTTONDOWN:
    down = true;
    [[fallthrough]];
  case WM_MBUTTONUP:
    button = 2;
    break;
  case WM_XBUTTONDOWN:
    down = true;
    [[fallthrough]];
  case WM_XBUTTONUP:
    button = HIWORD(mouse.mouseData) == XBUTTON1 ? 3 : 4;
    break;
  default:
    return false;
  }
  if (d->mouseButton < 0) {
    if (!down)
      return false; // Release of a button held before opening the dialog.
    POINT client = mouse.pt;
    ScreenToClient(d->h, &client);
    RECT close = GfxCloseRect(KC_W);
    if (button == 0 && PtInRect(&close, client))
      return false; // Keep the dialog close button usable.
    d->mouseButton = button;
    d->mousePosition = mouse.pt;
  }
  if (!down && button == d->mouseButton)
    PostMessageW(d->h, KC_MOUSE_DONE, 0, 0);
  // Consume the capture click, including release, before re-enabling the
  // editor.
  return true;
}

static LRESULT CALLBACK KeyMouseProc(int code, WPARAM w, LPARAM l) {
  if (code == HC_ACTION && g_keyMouseCapture &&
      CaptureMouseButton(g_keyMouseCapture, (UINT)w,
                         *(const MSLLHOOKSTRUCT *)l))
    return 1;
  return CallNextHookEx(nullptr, code, w, l);
}

static LRESULT CALLBACK KeyCapProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    KeyDlg *nd = (KeyDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
    nd->h = h;
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  KeyDlg *d = (KeyDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case KC_MOUSE_DONE:
    d->ok = true;
    d->done = true;
    DestroyWindow(h);
    return 0;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (d->mouseButton >= 0)
      return 0;
    if (l & (1 << 30)) // auto-repeat of a key that was already down
      return 0;
    int vk = (int)w;
    UINT sc = (UINT)((l >> 16) & 0xFF);
    bool ext = ((l >> 24) & 1) != 0;
    if (vk == VK_SHIFT)
      vk = (int)MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX);
    else if (vk == VK_CONTROL)
      vk = ext ? VK_RCONTROL : VK_LCONTROL;
    else if (vk == VK_MENU)
      vk = ext ? VK_RMENU : VK_LMENU;
    if (vk <= 0 || vk > 255)
      return 0;
    d->vk = vk;
    d->ext = ext;
    d->ok = true;
    d->done = true;
    DestroyWindow(h);
    return 0;
  }
  case WM_KEYUP:
  case WM_SYSKEYUP:
  case WM_CHAR:
  case WM_SYSCHAR:
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = GfxCloseRect(KC_W);
    if (pt.y < 30 && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT cr = GfxCloseRect(KC_W);
    bool nc = PtInRect(&cr, pt) != 0;
    if (nc != d->hoverClose) {
      d->hoverClose = nc;
      InvalidateRect(h, nullptr, FALSE);
    }
    SetCursor(LoadCursorW(nullptr, nc ? IDC_HAND : IDC_ARROW));
    if (!d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    d->tracking = false;
    d->hoverClose = false;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT cr = GfxCloseRect(KC_W);
    if (PtInRect(&cr, pt)) {
      d->done = true;
      DestroyWindow(h);
    }
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC mem, Gdiplus::Graphics &g, const RECT &rc) {
      GfxChrome(g, mem, rc.right, d->title.c_str(), d->hoverClose ? 2 : 0,
                CHB_SEP);
      PutText(g, mem, L(d->allowMouse ? L_PRESS_INPUT : L_ED_PRESS_KEY), 0,
              42.0f, (REAL)rc.right, 24.0f, GdiCol(ThDim()), 1, 10);
      PutText(g, mem, L"\u2026", 0, 66.0f, (REAL)rc.right, 32.0f,
              GdiCol(ThText()), 1, 15, true);
    });
    return 0;
  case WM_DESTROY:
    if (d->owner) {
      EnableWindow(d->owner, TRUE);
      SetForegroundWindow(d->owner);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static bool CaptureKey(HWND owner, const wchar_t *title, int &vk, bool &ext,
                       MAc *mouse = nullptr) {
  KeyDlg d;
  d.title = title;
  d.owner = owner;
  d.allowMouse = mouse != nullptr;
  ModalGuard mg;
  int x, y;
  PlaceOver(owner, KC_W, KC_H, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyKeyCap", L"", WS_POPUP, x,
                           y, KC_W, KC_H, owner, nullptr, g_hInst, &d);
  if (!h) {
    EnableWindow(owner, TRUE);
    return false;
  }
  GfxRoundRegion(h);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  SetFocus(h);
  HHOOK hook = nullptr;
  if (d.allowMouse) {
    g_keyMouseCapture = &d;
    hook = SetWindowsHookExW(WH_MOUSE_LL, KeyMouseProc, g_hInst, 0);
    if (!hook) {
      g_keyMouseCapture = nullptr;
      DestroyWindow(h);
      UiMessage(owner, L(L_INPUT_CAPTURE_FAIL), title, UI_OK, UI_ICON_ERROR);
      return false;
    }
  }
  RunModal(h, d.done);
  if (hook)
    UnhookWindowsHookEx(hook);
  g_keyMouseCapture = nullptr;
  if (d.ok) {
    if (mouse) {
      *mouse = MAc();
      mouse->type = d.mouseButton >= 0 ? A_MD : A_KD;
      mouse->btn = (BYTE)(std::max)(0, d.mouseButton);
      mouse->x = d.mousePosition.x;
      mouse->y = d.mousePosition.y;
    }
    vk = d.vk;
    ext = d.ext;
  }
  return d.ok;
}

// Shared, searchable checklist for preview exclusions and per-macro visibility.
static unsigned EdDefaultVisibility() {
  return (g_cfg.edKeys ? EV_KEYS : 0) | (g_cfg.edLeft ? EV_LEFT : 0) |
         (g_cfg.edRight ? EV_RIGHT : 0) | (g_cfg.edMiddle ? EV_MIDDLE : 0) |
         (g_cfg.edX1 ? EV_X1 : 0) | (g_cfg.edX2 ? EV_X2 : 0) |
         (g_cfg.edWheelUp ? EV_WHEELUP : 0) |
         (g_cfg.edWheelDown ? EV_WHEELDOWN : 0) |
         (g_cfg.edWheelLeft ? EV_WHEELLEFT : 0) |
         (g_cfg.edWheelRight ? EV_WHEELRIGHT : 0) |
         (g_cfg.edMoves ? EV_MOVES : 0);
}
struct CheckRow {
  int id;
  std::wstring label;
};
struct CheckDlg {
  HWND h = nullptr, owner = nullptr, list = nullptr, search = nullptr;
  EditorPreferences prefs;
  unsigned defaults = EV_ALL;
  bool visibility = false, ok = false, done = false;
  int hover = -1;
  std::vector<CheckRow> rows;
  std::vector<int> visible;
  std::wstring lastQuery;
};
static const int CK_W = 460, CK_H = 480, CK_ALLKEYS = 300;
static int CheckState(const CheckDlg *d, int id) {
  if (d->visibility)
    return (d->prefs.visibility & (1u << id)) ? 1 : 0;
  const auto &e = d->prefs.exclude;
  if (id == CK_ALLKEYS) {
    size_t count = 0;
    for (unsigned key : PreviewStandardKeys())
      if (e.keys[key])
        ++count;
    return count == PreviewStandardKeys().size() ? 1 : (count ? 2 : 0);
  }
  if (id < 10)
    return (e.mouse & (1u << id)) ? 1 : 0;
  if (id < 14)
    return (e.wheel & (1u << (id - 10))) ? 1 : 0;
  if (id == 14)
    return e.movement ? 1 : 0;
  return e.keys[(size_t)(id - 32)] ? 1 : 0;
}
static void CheckSet(CheckDlg *d, int id, bool checked) {
  if (d->visibility) {
    if (checked)
      d->prefs.visibility |= 1u << id;
    else
      d->prefs.visibility &= ~(1u << id);
    d->prefs.customVisibility = true;
  } else if (id == CK_ALLKEYS) {
    for (size_t key = 0; key < 256; ++key)
      if (PreviewKeyboardKey((unsigned)key))
        d->prefs.exclude.keys[key] = checked;
  } else if (id < 10) {
    if (checked)
      d->prefs.exclude.mouse |= 1u << id;
    else
      d->prefs.exclude.mouse &= ~(1u << id);
  } else if (id < 14) {
    if (checked)
      d->prefs.exclude.wheel |= 1u << (id - 10);
    else
      d->prefs.exclude.wheel &= ~(1u << (id - 10));
  } else if (id == 14)
    d->prefs.exclude.movement = checked;
  else
    d->prefs.exclude.keys[(size_t)(id - 32)] = checked;
}
static void CheckRows(CheckDlg *d) {
  const int mouse[] = {L_EX_LEFT, L_EX_RIGHT, L_EX_MIDDLE, L_EX_X1, L_EX_X2};
  const int wheel[] = {L_REC_WHEELUP, L_REC_WHEELDOWN, L_WHEEL_LEFT,
                       L_WHEEL_RIGHT};
  if (d->visibility) {
    d->rows.push_back({0, L(L_ED_SHOW_KEYS)});
    for (int i = 0; i < 5; ++i)
      d->rows.push_back({i + 1, L(mouse[i])});
    for (int i = 0; i < 4; ++i)
      d->rows.push_back({i + 6, L(wheel[i])});
    d->rows.push_back({10, L(L_REC_MOVE)});
    return;
  }
  d->rows.push_back({CK_ALLKEYS, L(L_ALL_KEYS)});
  d->rows.push_back({14, L(L_REC_MOVE)});
  for (int i = 0; i < 5; ++i) {
    d->rows.push_back(
        {i * 2, std::wstring(L(mouse[i])) + L" - " + L(L_MOUSE_DOWN)});
    d->rows.push_back(
        {i * 2 + 1, std::wstring(L(mouse[i])) + L" - " + L(L_MOUSE_UP)});
  }
  for (int i = 0; i < 4; ++i)
    d->rows.push_back({10 + i, L(wheel[i])});
  for (unsigned key : PreviewStandardKeys()) {
    std::wstring name =
        key == 255 ? L"Win (left + right)" : KeyLabel((BYTE)key);
    if (key == VK_CONTROL)
      name = L"Ctrl (left + right)";
    if (key == VK_SHIFT)
      name = L"Shift (left + right)";
    if (key == VK_MENU)
      name = L"Alt (left + right)";
    if (key == VK_LCONTROL)
      name = L"Left Ctrl";
    if (key == VK_RCONTROL)
      name = L"Right Ctrl";
    if (key == VK_LSHIFT)
      name = L"Left Shift";
    if (key == VK_RSHIFT)
      name = L"Right Shift";
    if (key == VK_LMENU)
      name = L"Left Alt";
    if (key == VK_RMENU)
      name = L"Right Alt";
    if (key == VK_LWIN)
      name = L"Left Win";
    if (key == VK_RWIN)
      name = L"Right Win";
    d->rows.push_back(
        {32 + (int)key, std::wstring(L(L_KEYBOARD)) + L": " + name});
  }
}
static std::wstring CheckRowText(const CheckDlg *d, const CheckRow &row) {
  bool excluded = false;
  if (!d->visibility) {
    excluded = row.id >= 32 && row.id < CK_ALLKEYS
                   ? d->prefs.exclude.KeyBlocked((BYTE)(row.id - 32))
                   : CheckState(d, row.id) == 1;
  }
  return row.label +
         (excluded ? std::wstring(L" \u2014 ") + L(L_EXCLUDED) : L"");
}
static void CheckFilter(CheckDlg *d) {
  if (!d->list)
    return;
  int previousTop = (int)SendMessageW(d->list, LB_GETTOPINDEX, 0, 0);
  int previousSelection = (int)SendMessageW(d->list, LB_GETCURSEL, 0, 0);
  std::wstring query((size_t)GetWindowTextLengthW(d->search) + 1, L'\0');
  query.resize(
      (size_t)GetWindowTextW(d->search, query.data(), (int)query.size()));
  if (!query.empty())
    CharLowerBuffW(query.data(), (DWORD)query.size());
  d->visible.clear();
  SendMessageW(d->list, WM_SETREDRAW, FALSE, 0);
  SendMessageW(d->list, LB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < d->rows.size(); ++i) {
    auto display = CheckRowText(d, d->rows[i]);
    auto name = display;
    if (!name.empty())
      CharLowerBuffW(name.data(), (DWORD)name.size());
    if (name.find(query) == std::wstring::npos)
      continue;
    d->visible.push_back((int)i);
    SendMessageW(d->list, LB_ADDSTRING, 0, (LPARAM)display.c_str());
  }
  bool sameQuery = query == d->lastQuery;
  if (!d->visible.empty()) {
    int selected = sameQuery ? (std::clamp)(previousSelection, 0,
                                            (int)d->visible.size() - 1)
                             : 0;
    SendMessageW(d->list, LB_SETCURSEL, selected, 0);
    if (sameQuery)
      SendMessageW(d->list, LB_SETTOPINDEX, (std::max)(0, previousTop), 0);
  }
  d->lastQuery = query;
  SendMessageW(d->list, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(d->list, nullptr, TRUE);
  InvalidateRect(d->h, nullptr, FALSE);
}
static void CheckToggle(CheckDlg *d, int index) {
  if (index < 0 || index >= (int)d->visible.size())
    return;
  int id = d->rows[(size_t)d->visible[(size_t)index]].id;
  CheckSet(d, id, CheckState(d, id) != 1);
  CheckFilter(d);
}
static RECT CheckButton(int i) {
  if (i < 3) {
    int x = 10 + i * 148;
    return {x, 390, x + 144, 418};
  }
  int x = i == 3 ? CK_W - 230 : CK_W - 116;
  return {x, CK_H - 38, x + 106, CK_H - 10};
}
static void CheckFinish(CheckDlg *d, bool apply) {
  d->ok = apply;
  d->done = true;
  DestroyWindow(d->h);
}
static void CheckRun(CheckDlg *d, int button) {
  if (button == 3 || button == 4) {
    CheckFinish(d, button == 3);
    return;
  }
  if (button == 1 && !d->visibility) {
    d->prefs.exclude = {};
  } else if (button == 0 || button == 1) {
    for (const auto &row : d->rows)
      CheckSet(d, row.id, button == 0);
  } else if (d->visibility) {
    d->prefs.customVisibility = false;
    d->prefs.visibility = d->defaults;
  } else {
    int vk = 0;
    bool ext = false;
    if (CaptureKey(d->h, L(L_BLOCK_KEY), vk, ext) && PreviewKeyboardKey(vk) &&
        vk <= 254) {
      d->prefs.exclude.keys[(size_t)vk] = true;
      bool listed = false;
      for (const auto &row : d->rows)
        if (row.id == 32 + vk)
          listed = true;
      if (!listed)
        d->rows.push_back(
            {32 + vk, std::wstring(L(L_KEYBOARD)) + L": " + KeyLabel(vk)});
    }
  }
  CheckFilter(d);
}
static LRESULT CALLBACK CheckListProc(HWND h, UINT m, WPARAM w, LPARAM l,
                                      UINT_PTR, DWORD_PTR data) {
  auto *d = (CheckDlg *)data;
  if (m == WM_KEYDOWN && w == VK_TAB) {
    SetFocus(d->search);
    return 0;
  }
  if (m == WM_CHAR && (w == VK_SPACE || w == VK_TAB))
    return 0;
  if (m == WM_KEYDOWN && w == VK_SPACE) {
    CheckToggle(d, (int)SendMessageW(h, LB_GETCURSEL, 0, 0));
    return 0;
  }
  if (m == WM_KEYDOWN && w == VK_RETURN) {
    CheckFinish(d, true);
    return 0;
  }
  if (m == WM_KEYDOWN && w == VK_ESCAPE) {
    CheckFinish(d, false);
    return 0;
  }
  if (m == WM_LBUTTONUP) {
    LRESULT hit = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
    LRESULT result = DefSubclassProc(h, m, w, l);
    if (!HIWORD(hit))
      CheckToggle(d, LOWORD(hit));
    return result;
  }
  return DefSubclassProc(h, m, w, l);
}
static LRESULT CALLBACK CheckSearchProc(HWND h, UINT m, WPARAM w, LPARAM l,
                                        UINT_PTR, DWORD_PTR data) {
  auto *d = (CheckDlg *)data;
  if (m == WM_KEYDOWN && w == VK_TAB) {
    SetFocus(d->list);
    return 0;
  }
  if (m == WM_CHAR && w == VK_TAB)
    return 0;
  if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) {
    CheckFinish(d, w == VK_RETURN);
    return 0;
  }
  return DefSubclassProc(h, m, w, l);
}
static LRESULT CALLBACK CheckProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    auto *d = (CheckDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
    d->h = h;
    d->search = CreateWindowExW(
        0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        12, 82, CK_W - 24, 26, h, (HMENU)1, g_hInst, nullptr);
    SendMessageW(d->search, WM_SETFONT, (WPARAM)ThFont(9), TRUE);
    SendMessageW(d->search, EM_SETCUEBANNER, TRUE, (LPARAM)L(L_FILTER));
    d->list = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED |
            LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        12, 116, CK_W - 24, 266, h, (HMENU)2, g_hInst, nullptr);
    SendMessageW(d->list, LB_SETITEMHEIGHT, 0, 23);
    SetWindowSubclass(d->list, CheckListProc, 1, (DWORD_PTR)d);
    SetWindowSubclass(d->search, CheckSearchProc, 1, (DWORD_PTR)d);
    ApplyListTheme(d->list);
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    CheckFilter(d);
    return 0;
  }
  auto *d = (CheckDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_COMMAND:
    if ((HWND)l == d->search && HIWORD(w) == EN_CHANGE)
      CheckFilter(d);
    return 0;
  case WM_CTLCOLORLISTBOX:
    return (LRESULT)FieldBrush();
  case WM_CTLCOLOREDIT:
    SetTextColor((HDC)w, ThText());
    SetBkColor((HDC)w, FieldCol(false));
    return (LRESULT)FieldBrush();
  case WM_MEASUREITEM:
    ((MEASUREITEMSTRUCT *)l)->itemHeight = 23;
    return TRUE;
  case WM_DRAWITEM: {
    auto *di = (DRAWITEMSTRUCT *)l;
    if (di->itemID == (UINT)-1 || di->itemID >= d->visible.size())
      return TRUE;
    Gdiplus::Graphics g(di->hDC);
    RECT r = di->rcItem;
    int x = (int)r.left, y = (int)r.top;
    Gdiplus::SolidBrush bg(
        GdiCol((di->itemState & ODS_SELECTED) ? ThHover() : FieldCol(false)));
    g.FillRectangle(&bg, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                    (REAL)(r.bottom - r.top));
    const auto &row = d->rows[(size_t)d->visible[di->itemID]];
    int check = CheckState(d, row.id);
    Gdiplus::Pen outline(GdiCol(check ? ThAccent() : ThDim()), 1);
    g.DrawRectangle(&outline, x + 7, y + 5, 13, 13);
    if (check) {
      Gdiplus::Pen mark(GdiCol(ThAccent()), 2);
      if (check == 2)
        g.DrawLine(&mark, x + 10, y + 11, x + 17, y + 11);
      else {
        g.DrawLine(&mark, x + 9, y + 11, x + 12, y + 15);
        g.DrawLine(&mark, x + 12, y + 15, x + 18, y + 8);
      }
    }
    PutText(g, di->hDC, CheckRowText(d, row), (REAL)x + 29, (REAL)r.top,
            (REAL)(r.right - r.left - 32), 23, GdiCol(ThText()));
    return TRUE;
  }
  case WM_KEYDOWN:
    if (w == VK_ESCAPE || w == VK_RETURN)
      CheckFinish(d, w == VK_RETURN);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT close = GfxCloseRect(CK_W);
    return pt.y < 30 && !PtInRect(&close, pt) ? HTCAPTION : HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int hover = -1;
    RECT close = GfxCloseRect(CK_W);
    if (PtInRect(&close, pt))
      hover = 100;
    for (int i = 0; i < 5; ++i) {
      RECT r = CheckButton(i);
      if (PtInRect(&r, pt))
        hover = i;
    }
    if (hover != d->hover) {
      d->hover = hover;
      InvalidateRect(h, nullptr, FALSE);
    }
    return 0;
  }
  case WM_LBUTTONUP: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT close = GfxCloseRect(CK_W);
    if (PtInRect(&close, pt)) {
      CheckFinish(d, false);
      return 0;
    }
    for (int i = 0; i < 5; ++i) {
      RECT r = CheckButton(i);
      if (PtInRect(&r, pt)) {
        CheckRun(d, i);
        return 0;
      }
    }
    return 0;
  }
  case WM_CLOSE:
    CheckFinish(d, false);
    return 0;
  case WM_PAINT:
    PaintBuffered(h, [&](HDC dc, Gdiplus::Graphics &g, const RECT &) {
      GfxChrome(g, dc, CK_W, L(d->visibility ? L_VIS_TITLE : L_EX_TITLE),
                d->hover == 100 ? 2 : 0, CHB_SEP);
      PutText(g, dc, L(d->visibility ? L_VIS_HINT : L_EX_HINT), 12, 34,
              CK_W - 24, 22, GdiCol(ThText()));
      const wchar_t *hint =
          d->visibility
              ? L(d->prefs.customVisibility ? L_VIS_CUSTOM : L_VIS_DEFAULT)
              : L(L_EX_UNCHECKED);
      PutText(g, dc, hint, 12, 56, CK_W - 24, 20, GdiCol(ThDim()), 0, 8);
      RECT searchFrame = {10, 80, CK_W - 10, 110};
      FrameRect(g, searchFrame);
      RECT frame = {10, 114, CK_W - 10, 384};
      FrameRect(g, frame);
      const int labels[] = {L_SELECT_ALL, L_CLEAR_ALL,
                            d->visibility ? L_RESET_DEFAULT : L_BLOCK_KEY, L_OK,
                            L_CANCEL};
      for (int i = 0; i < 5; ++i)
        PaintBtn(g, dc, CheckButton(i), L(labels[i]), d->hover == i, i == 3,
                 true);
      auto count = std::to_wstring(d->visible.size()) + L" " + L(L_MATCHING);
      PutText(g, dc, count, 12, 420, CK_W - 24, 20, GdiCol(ThDim()), 0, 8);
    });
    return 0;
  case WM_DESTROY:
    if (d->owner) {
      EnableWindow(d->owner, TRUE);
      SetForegroundWindow(d->owner);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static bool ShowChecklist(HWND owner, bool visibility,
                          EditorPreferences &prefs) {
  CheckDlg d;
  d.owner = owner;
  d.prefs = prefs;
  d.visibility = visibility;
  d.defaults = EdDefaultVisibility();
  if (!d.prefs.customVisibility)
    d.prefs.visibility = d.defaults;
  CheckRows(&d);
  ModalGuard guard;
  int x, y;
  PlaceOver(owner, CK_W, CK_H, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyEditorOptions", L"",
                           WS_POPUP | WS_CLIPCHILDREN, x, y, CK_W, CK_H, owner,
                           nullptr, g_hInst, &d);
  if (!h) {
    EnableWindow(owner, TRUE);
    return false;
  }
  GfxRoundRegion(h);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  SetFocus(d.search);
  RunModal(h, d.done);
  if (d.ok)
    prefs = d.prefs;
  return d.ok;
}

// ---------------------------------------------------------------------------
// Macro editor
// ---------------------------------------------------------------------------
//
// The editor shows a macro as a flat list: key/mouse events and, between them,
// separate "Delay" rows (like a classic macro recorder). Internally every
// event stores the wait *before* it, so we split that into Delay rows when the
// editor opens and fold consecutive Delay rows back together on OK.

static const int EDT_W = 500, EDT_H = 410, EDT_ROW = 22, EDT_LIST_ID = 101;
static const int EDT_LX = 10, EDT_LY = 40, EDT_LW = 354, EDT_LH = 336;
static const long MAX_DELAY_MS = 3600000;

enum {
  EB_ADD,
  EB_ADDDELAY,
  EB_KEY,
  EB_DELAY,
  EB_DEL,
  EB_UP,
  EB_DOWN,
  EB_UNDO,
  EB_REDO,
  EB_OK,
  EB_CANCEL,
  EB_PLAY,
  EB_COUNT
};

using EdItem = macroedit::Item;

// One undo/redo step: the whole row list plus what was selected at that point.
struct EdSnap {
  std::vector<EdItem> it;
  std::vector<int> sel;
  std::vector<MAc> tail;
};

struct EdDlg {
  HWND h = nullptr, lb = nullptr, owner = nullptr;
  std::vector<EdItem> it;
  std::vector<EdSnap> undo, redo;
  std::vector<MAc> tail; // hidden events after the last visible row
  bool changed = false, ok = false, done = false, tracking = false;
  int hover = -1; // EB_* index, or 100 for the close button
  bool editMode = false, preview = false;
  bool dragPending = false, dragging = false, dragCtrl = false;
  POINT dragStart{}, dragPoint{};
  int dragStartY = 0;
  std::vector<int> dragBase;
  EditorPreferences prefs;
  unsigned visibilityMask = EV_ALL;
};

static const int ED_PANEL_H = 112, ED_MODE = 110, ED_PANEL_BASE = 200;
static int EdOffset(const EdDlg *d) { return d->editMode ? ED_PANEL_H : 0; }
static RECT EdModeRect() { return {EDT_W - 158, 5, EDT_W - 36, 27}; }

static RECT EdBtn(const EdDlg *d, int i) {
  int x = EDT_W - 126, offset = EdOffset(d);
  RECT r;
  if (i == EB_PLAY)
    r = {EDT_LX, EDT_LY + EDT_LH + 4, EDT_LX + 126, EDT_LY + EDT_LH + 28};
  else if (i == EB_OK)
    r = {x, EDT_H - 74, x + 116, EDT_H - 46};
  else if (i == EB_CANCEL)
    r = {x, EDT_H - 40, x + 116, EDT_H - 12};
  else if (i == EB_UNDO || i == EB_REDO) {
    int y = EDT_LY + EB_UNDO * 34;
    int hx = i == EB_UNDO ? x : x + 61;
    r = {hx, y, hx + 55, y + 28};
  } else {
    int y = EDT_LY + i * 34;
    r = {x, y, x + 116, y + 28};
  }
  OffsetRect(&r, 0, offset);
  return r;
}

static const wchar_t *EdBtnText(int i) {
  switch (i) {
  case EB_ADD:
    return L(L_ED_ADD);
  case EB_ADDDELAY:
    return L(L_ED_ADD_DELAY);
  case EB_KEY:
    return L(L_ED_CHANGE);
  case EB_DELAY:
    return L(L_ED_DELAY);
  case EB_DEL:
    return L(L_MC_DELETE);
  case EB_UP:
    return L(L_ED_UP);
  case EB_DOWN:
    return L(L_ED_DOWN);
  case EB_UNDO:
    return L(L_ED_UNDO);
  case EB_REDO:
    return L(L_ED_REDO);
  case EB_PLAY:
    return L(L_PLAY_SELECTED);
  case EB_OK:
    return L(L_OK);
  default:
    return L(L_CANCEL);
  }
}

static bool IsKeyEv(const MAc &a) { return a.type == A_KD || a.type == A_KU; }
static bool IsKeyIt(const EdItem &e) { return !e.isDelay && IsKeyEv(e.a); }

// The default is inherited dynamically; a per-macro override is explicit.
static std::vector<EdItem>
EdFromEvents(const std::vector<MAc> &events, std::vector<MAc> &tail,
             unsigned visibility = EdDefaultVisibility()) {
  return macroedit::FromEvents(events, tail, visibility);
}
static std::vector<MAc> EdToEvents(const std::vector<EdItem> &items,
                                   const std::vector<MAc> &tail) {
  return macroedit::ToEvents(items, tail);
}

static size_t EdEventCount(const EdDlg *d) {
  size_t n = d->tail.size();
  for (const EdItem &e : d->it)
    if (!e.isDelay)
      n += 1 + e.pre.size();
  return n;
}

static std::vector<int> EdSel(EdDlg *d) {
  std::vector<int> v;
  int n = (int)SendMessageW(d->lb, LB_GETSELCOUNT, 0, 0);
  if (n > 0) {
    v.resize((size_t)n);
    int got = (int)SendMessageW(d->lb, LB_GETSELITEMS, (WPARAM)n,
                                (LPARAM)v.data());
    if (got < 0)
      got = 0;
    v.resize((size_t)got);
  }
  return v;
}

static bool EdEnabled(EdDlg *d, int b) {
  if (d->preview)
    return b == EB_PLAY || b == EB_CANCEL;
  if (b == EB_PLAY)
    return !EdSel(d).empty() && g_state.load() == ST_IDLE;
  std::vector<int> sel;
  switch (b) {
  case EB_ADD:
    return (int)d->it.size() + 4 <= MAX_EDIT_EVENTS * 2;
  case EB_ADDDELAY:
    return (int)d->it.size() + 3 <= MAX_EDIT_EVENTS * 2;
  case EB_KEY:
    sel = EdSel(d);
    for (int row : sel)
      if (IsKeyIt(d->it[(size_t)row]))
        return true;
    return false;
  case EB_DELAY:
    sel = EdSel(d);
    for (int i : sel)
      if (i >= 0 && i < (int)d->it.size() && d->it[(size_t)i].isDelay)
        return true;
    return false;
  case EB_DEL:
  case EB_UP:
  case EB_DOWN:
    return (int)SendMessageW(d->lb, LB_GETSELCOUNT, 0, 0) > 0;
  case EB_UNDO:
    return !d->undo.empty();
  case EB_REDO:
    return !d->redo.empty();
  default:
    return true;
  }
}

// Keeps `idx` on screen and returns the top index the list should use.
static int EdTopFor(EdDlg *d, int idx) {
  int top = (int)SendMessageW(d->lb, LB_GETTOPINDEX, 0, 0);
  if (top < 0)
    top = 0;
  RECT rc;
  GetClientRect(d->lb, &rc);
  int vis = (std::max)(1, (int)(rc.bottom / EDT_ROW));
  if (idx < top)
    top = idx;
  else if (idx >= top + vis)
    top = idx - vis + 1;
  return (std::max)(0, top);
}

static void EdReload(EdDlg *d, const std::vector<int> &sel, int top) {
  SendMessageW(d->lb, WM_SETREDRAW, FALSE, 0);
  SendMessageW(d->lb, LB_SETCOUNT, (WPARAM)d->it.size(), 0);
  SendMessageW(d->lb, LB_SETSEL, FALSE, (LPARAM)-1);
  for (int i : sel)
    if (i >= 0 && i < (int)d->it.size())
      SendMessageW(d->lb, LB_SETSEL, TRUE, (LPARAM)i);
  if (!sel.empty())
    SendMessageW(d->lb, LB_SETCARETINDEX, (WPARAM)sel[0], FALSE);
  if (top >= 0)
    SendMessageW(d->lb, LB_SETTOPINDEX, (WPARAM)top, 0);
  SendMessageW(d->lb, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(d->lb, nullptr, TRUE);
  InvalidateRect(d->h, nullptr, FALSE);
}

static int EdKeepTop(EdDlg *d) {
  int t = (int)SendMessageW(d->lb, LB_GETTOPINDEX, 0, 0);
  return t < 0 ? 0 : t;
}

// ---- Undo / redo ----------------------------------------------------------
// Every edit stores a snapshot of the row list first (EdPush). Snapshots are
// capped so a huge macro can't eat all the memory.
static const size_t ED_UNDO_MAX = 100;
static const size_t ED_UNDO_MAX_ITEMS = 600000; // rows across all snapshots

static void EdTrim(std::vector<EdSnap> &v) {
  size_t total = 0;
  for (const EdSnap &s : v)
    total += s.it.size() + s.tail.size();
  while (v.size() > 1 && (v.size() > ED_UNDO_MAX || total > ED_UNDO_MAX_ITEMS)) {
    total -= v.front().it.size() + v.front().tail.size();
    v.erase(v.begin());
  }
}

// Call right before changing d->it.
static void EdPush(EdDlg *d) {
  EdSnap s;
  s.it = d->it;
  s.tail = d->tail;
  s.sel = EdSel(d);
  d->undo.push_back(std::move(s));
  d->redo.clear();
  EdTrim(d->undo);
}

// For an edit that turned out to change nothing.
static void EdPushCancel(EdDlg *d) {
  if (!d->undo.empty())
    d->undo.pop_back();
}

// Swaps the current rows with the newest snapshot of `from` (the current rows
// go to `to`, so the opposite action can bring them back).
static void EdStep(EdDlg *d, std::vector<EdSnap> &from,
                   std::vector<EdSnap> &to) {
  if (from.empty())
    return;
  EdSnap cur;
  cur.sel = EdSel(d);
  cur.it = std::move(d->it);
  cur.tail = std::move(d->tail);
  EdSnap prev = std::move(from.back());
  from.pop_back();
  d->it = std::move(prev.it);
  d->tail = std::move(prev.tail);
  to.push_back(std::move(cur));
  EdTrim(to);
  d->changed = true;
  int top = prev.sel.empty() ? EdKeepTop(d) : EdTopFor(d, prev.sel[0]);
  EdReload(d, prev.sel, top);
}

static void EdUndo(EdDlg *d) { EdStep(d, d->undo, d->redo); }
static void EdRedo(EdDlg *d) { EdStep(d, d->redo, d->undo); }

static long ParseMs(const std::wstring &s) {
  long v = wcstol(s.c_str(), nullptr, 10);
  if (v < 0)
    v = 0;
  if (v > MAX_DELAY_MS)
    v = MAX_DELAY_MS;
  return v;
}

static EdItem MakeDelay(long ms) {
  EdItem e;
  e.isDelay = true;
  e.ms = (DWORD)ms;
  return e;
}

static EdItem MakeKey(int type, int vk, bool ext) {
  EdItem e;
  e.a.type = type;
  e.a.vk = (BYTE)vk;
  e.a.flags = ext ? KEYEVENTF_EXTENDEDKEY : 0;
  return e;
}

// Inserts a key tap or mouse click at the captured screen position.
static std::vector<EdItem> EdInputTap(int vk, bool ext, const MAc &input) {
  EdItem press = MakeKey(A_KD, vk, ext);
  EdItem release = MakeKey(A_KU, vk, ext);
  if (input.type == A_MD) {
    press.a = release.a = input;
    release.a.type = A_MU;
  }
  return {MakeDelay(100), press, MakeDelay(50), release};
}

static void EdAddKey(EdDlg *d) {
  if (!EdEnabled(d, EB_ADD))
    return;
  int vk = 0;
  bool ext = false;
  MAc input;
  if (!CaptureKey(d->h, L(L_ED_ADD), vk, ext, &input))
    return;
  std::vector<int> sel = EdSel(d);
  int pos = sel.empty() ? (int)d->it.size() : sel.back() + 1;
  if (pos > (int)d->it.size())
    pos = (int)d->it.size();
  EdPush(d);
  auto add = EdInputTap(vk, ext, input);
  d->it.insert(d->it.begin() + pos, add.begin(), add.end());
  d->changed = true;
  std::vector<int> addedSelection = {pos + 1, pos + 3};
  if (!macroedit::Shown(add[1].a, d->visibilityMask))
    macroedit::Reproject(d->it, d->tail, addedSelection, d->visibilityMask);
  EdReload(d, addedSelection, EdTopFor(d, pos + 3));
}

static void EdAddDelay(EdDlg *d) {
  if (!EdEnabled(d, EB_ADDDELAY))
    return;
  std::wstring out;
  if (!PromptText(d->h, L(L_ED_ADD_DELAY), L(L_ED_DELAY_PROMPT), L"100", true,
                  7, out))
    return;
  long v = ParseMs(out);
  std::vector<int> sel = EdSel(d);
  int pos = sel.empty() ? (int)d->it.size() : sel.back() + 1;
  if (pos > (int)d->it.size())
    pos = (int)d->it.size();
  EdPush(d);
  d->it.insert(d->it.begin() + pos, MakeDelay(v));
  d->changed = true;
  EdReload(d, {pos}, EdTopFor(d, pos));
}

static void EdBulk(EdDlg *d, macroedit::Field field);
static void EdChangeKey(EdDlg *d) {
  std::vector<int> sel = EdSel(d);
  if (d->editMode || sel.size() > 1) {
    EdBulk(d, macroedit::Key);
    return;
  }
  if (sel.empty() || sel[0] >= (int)d->it.size() ||
      !IsKeyIt(d->it[(size_t)sel[0]]))
    return;
  int i = sel[0];
  int vk = 0;
  bool ext = false;
  if (!CaptureKey(d->h, L(L_ED_CHANGE), vk, ext))
    return;
  EdPush(d);
  BYTE oldVk = d->it[(size_t)i].a.vk;
  DWORD fl = ext ? KEYEVENTF_EXTENDEDKEY : 0;
  // A key press is a down + an up. Change the matching half as well, so
  // "h" -> "j" turns the whole press into "j" instead of leaving a stuck key.
  int partner = -1;
  int n = (int)d->it.size();
  if (d->it[(size_t)i].a.type == A_KD) {
    for (int k = i + 1; k < n; k++) {
      const EdItem &e = d->it[(size_t)k];
      if (!IsKeyIt(e) || e.a.vk != oldVk)
        continue;
      if (e.a.type == A_KU)
        partner = k;
      break;
    }
  } else {
    for (int k = i - 1; k >= 0; k--) {
      const EdItem &e = d->it[(size_t)k];
      if (!IsKeyIt(e) || e.a.vk != oldVk)
        continue;
      if (e.a.type == A_KD)
        partner = k;
      break;
    }
  }
  d->it[(size_t)i].a.vk = (BYTE)vk;
  d->it[(size_t)i].a.flags = fl;
  if (partner >= 0) {
    d->it[(size_t)partner].a.vk = (BYTE)vk;
    d->it[(size_t)partner].a.flags = fl;
  }
  d->changed = true;
  EdReload(d, sel, EdKeepTop(d));
}

// Sets every selected Delay row to the same value (Ctrl+A selects them all).
static void EdEditDelay(EdDlg *d) {
  std::vector<int> sel = EdSel(d);
  int first = -1;
  for (int i : sel)
    if (i >= 0 && i < (int)d->it.size() && d->it[(size_t)i].isDelay) {
      first = i;
      break;
    }
  if (first < 0)
    return;
  std::wstring out;
  if (!PromptText(d->h, L(L_ED_DELAY), L(L_ED_DELAY_PROMPT),
                  std::to_wstring(d->it[(size_t)first].ms), true, 7, out))
    return;
  long v = ParseMs(out);
  EdPush(d);
  for (int i : sel)
    if (i >= 0 && i < (int)d->it.size() && d->it[(size_t)i].isDelay)
      d->it[(size_t)i].ms = (DWORD)v;
  d->changed = true;
  EdReload(d, sel, EdKeepTop(d));
}

static void EdDelete(EdDlg *d) {
  std::vector<int> sel = EdSel(d);
  if (sel.empty())
    return;
  size_t n = d->it.size();
  std::vector<char> mask(n, 0);
  for (int i : sel)
    if (i >= 0 && (size_t)i < n)
      mask[(size_t)i] = 1;
  EdPush(d);
  std::vector<EdItem> out;
  out.reserve(n);
  for (size_t i = 0; i < n; i++)
    if (!mask[i])
      out.push_back(std::move(d->it[i]));
  d->it.swap(out);
  d->changed = true;
  std::vector<int> ns;
  if (!d->it.empty())
    ns.push_back((std::min)(sel[0], (int)d->it.size() - 1));
  EdReload(d, ns, EdKeepTop(d));
}

static void EdMove(EdDlg *d, int dir) {
  std::vector<int> sel = EdSel(d);
  if (sel.empty())
    return;
  int n = (int)d->it.size();
  std::vector<char> s((size_t)n, 0);
  for (int i : sel)
    if (i >= 0 && i < n)
      s[(size_t)i] = 1;
  EdPush(d);
  bool moved = false;
  if (dir < 0) {
    for (int i = 1; i < n; i++)
      if (s[(size_t)i] && !s[(size_t)i - 1]) {
        std::swap(d->it[(size_t)i], d->it[(size_t)i - 1]);
        std::swap(s[(size_t)i], s[(size_t)i - 1]);
        moved = true;
      }
  } else {
    for (int i = n - 2; i >= 0; i--)
      if (s[(size_t)i] && !s[(size_t)i + 1]) {
        std::swap(d->it[(size_t)i], d->it[(size_t)i + 1]);
        std::swap(s[(size_t)i], s[(size_t)i + 1]);
        moved = true;
      }
  }
  if (!moved) {
    EdPushCancel(d);
    return;
  }
  d->changed = true;
  std::vector<int> ns;
  for (int i = 0; i < n; i++)
    if (s[(size_t)i])
      ns.push_back(i);
  int focus = dir < 0 ? ns.front() : ns.back();
  EdReload(d, ns, EdTopFor(d, focus));
}

static void EdRowAction(EdDlg *d) {
  std::vector<int> sel = EdSel(d);
  if (sel.empty() || sel[0] >= (int)d->it.size())
    return;
  if (IsKeyIt(d->it[(size_t)sel[0]]))
    EdChangeKey(d);
  else if (d->it[(size_t)sel[0]].isDelay)
    EdEditDelay(d);
}

static void EdFinish(EdDlg *d, bool apply) {
  if (d->preview) {
    if (apply)
      return;
    Engine_PlayStop();
    Engine_Shutdown();
    d->preview = false;
  }
  if (apply) {
    if (EdEventCount(d) == 0) {
      StatusShow(L(L_ST_NOTHING), 2500);
      return;
    }
    if (d->changed) {
      g_events = EdToEvents(d->it, d->tail);
      g_dirty = true;
      if (!g_lastFile.empty()) {
        if (Engine_Save(g_lastFile.c_str())) {
          g_dirty = false;
          StatusShow(L(L_ST_SAVED), 2500);
        } else
          StatusShow(L(L_SAVEFAIL), 3000);
      }
    }
    d->ok = true;
  } else if (d->changed) {
    if (UiMessage(d->h, L(L_ED_DISCARD_Q), L(L_ED_TITLE), UI_YESNO,
                  UI_ICON_WARN) != IDYES)
      return;
  }
  d->done = true;
  DestroyWindow(d->h);
}

static bool EdNumber(EdDlg *d, const wchar_t *prompt, LONG initial, LONG lo,
                     LONG hi, LONG &value, const wchar_t *title = nullptr);

static void EdPlaySelection(EdDlg *d) {
  if (d->preview) {
    Engine_PlayStop();
    return;
  }
  auto selected = macroedit::Selection(d->it, EdSel(d), d->prefs.exclude);
  if (selected.empty()) {
    UiMessage(d->h, L(L_DELAY_ONLY), L(L_PLAY_SELECTED), UI_OK, UI_ICON_INFO);
    return;
  }
  LONG countdown = 0;
  if (!EdNumber(d, L(L_COUNTDOWN_PROMPT),
                (std::clamp)(g_cfg.playCountdown, 0, 60), 0, 60, countdown,
                L(L_PLAY_SELECTED)))
    return;
  if (Engine_PlaySelection(selected, countdown)) {
    std::wstring status = countdown > 0 ? std::wstring(L(L_COUNTDOWN)) + L": " +
                                              std::to_wstring(countdown) + L" s"
                                        : std::wstring(L(L_PLAY_SELECTED));
    StatusPersistent(status.c_str());
    d->preview = true;
    EnableWindow(d->lb, FALSE);
    InvalidateRect(d->h, nullptr, FALSE);
  }
}

static void EdApplyVisibility(EdDlg *d) {
  unsigned visibility =
      d->prefs.customVisibility ? d->prefs.visibility : EdDefaultVisibility();
  if (visibility == d->visibilityMask)
    return;
  auto selection = EdSel(d);
  macroedit::Reproject(d->it, d->tail, selection, visibility);
  for (auto *history : {&d->undo, &d->redo})
    for (auto &snapshot : *history)
      macroedit::Reproject(snapshot.it, snapshot.tail, snapshot.sel,
                           visibility);
  d->visibilityMask = visibility;
  EdReload(d, selection, 0);
}
static void EdOpenOptions(EdDlg *d, bool visibility) {
  if (d->preview)
    return;
  auto prefs = d->prefs;
  if (!ShowChecklist(d->h, visibility, prefs))
    return;
  if (!EditorPrefs_Set(g_lastFile, prefs)) {
    UiMessage(d->h, L(L_PREFS_SAVEFAIL), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
    return;
  }
  d->prefs = prefs;
  EdApplyVisibility(d);
  InvalidateRect(d->lb, nullptr, FALSE);
  InvalidateRect(d->h, nullptr, FALSE);
}

static void EdRunButton(EdDlg *d, int b) {
  if (!EdEnabled(d, b))
    return;
  switch (b) {
  case EB_PLAY:
    EdPlaySelection(d);
    break;
  case EB_ADD:
    EdAddKey(d);
    break;
  case EB_ADDDELAY:
    EdAddDelay(d);
    break;
  case EB_KEY:
    EdChangeKey(d);
    break;
  case EB_DELAY:
    EdEditDelay(d);
    break;
  case EB_DEL:
    EdDelete(d);
    break;
  case EB_UP:
    EdMove(d, -1);
    break;
  case EB_DOWN:
    EdMove(d, 1);
    break;
  case EB_UNDO:
    EdUndo(d);
    break;
  case EB_REDO:
    EdRedo(d);
    break;
  case EB_OK:
    EdFinish(d, true);
    break;
  case EB_CANCEL:
    EdFinish(d, false);
    break;
  }
}

enum {
  EP_UNIVERSAL,
  EP_KEYDOWN,
  EP_KEYUP,
  EP_BUTTON,
  EP_MOUSEDOWN,
  EP_MOUSEUP,
  EP_COORD,
  EP_WHEEL,
  EP_PAUSE,
  EP_VISIBILITY,
  EP_EXCLUSIONS,
  EP_COUNT
};
static RECT EdPanelRect(int i) {
  int x = EDT_LX + (i % 4) * 120, y = 40 + (i / 4) * 32;
  return {x, y, x + 112, y + 26};
}
static const wchar_t *EdPanelText(int i) {
  const int labels[EP_COUNT] = {
      L_UNIVERSAL_DELAY, L_KEY_DOWN,   L_KEY_UP,      L_MOUSE_BUTTON,
      L_MOUSE_DOWN,      L_MOUSE_UP,   L_COORDINATES, L_WHEEL_AMOUNT,
      L_PAUSE_RESUME,    L_VISIBILITY, L_EXCLUSIONS};
  return L(labels[i]);
}
static bool EdPanelEnabled(EdDlg *d, int i) {
  if (i == EP_PAUSE)
    return d->preview;
  if (d->preview)
    return false;
  if (i == EP_VISIBILITY || i == EP_EXCLUSIONS)
    return true;
  if (i == EP_UNIVERSAL)
    return !d->it.empty();
  macroedit::Field field = i == EP_KEYDOWN     ? macroedit::KeyDown
                           : i == EP_KEYUP     ? macroedit::KeyUp
                           : i == EP_BUTTON    ? macroedit::Button
                           : i == EP_MOUSEDOWN ? macroedit::MouseDown
                           : i == EP_MOUSEUP   ? macroedit::MouseUp
                           : i == EP_COORD     ? macroedit::Coordinates
                                               : macroedit::Wheel;
  for (int row : EdSel(d))
    if (macroedit::Compatible(d->it[(size_t)row], field))
      return true;
  return false;
}

static bool EdNumber(EdDlg *d, const wchar_t *prompt, LONG initial, LONG lo,
                     LONG hi, LONG &value, const wchar_t *title) {
  std::wstring out;
  for (;;) {
    if (!PromptText(d->h, title ? title : L(L_EDIT_MODE), prompt,
                    std::to_wstring(initial), false, 12, out))
      return false;
    wchar_t *end = nullptr;
    errno = 0;
    long long parsed = wcstoll(out.c_str(), &end, 10);
    if (!out.empty() && end == out.c_str() + out.size() && errno != ERANGE &&
        parsed >= lo && parsed <= hi) {
      value = (LONG)parsed;
      return true;
    }
    UiMessage(d->h, L(L_RANGE_ERROR), L(L_EDIT_MODE), UI_OK, UI_ICON_ERROR);
  }
}

static void EdBulk(EdDlg *d, macroedit::Field field) {
  auto sel = EdSel(d);
  int first = -1;
  for (int row : sel)
    if (macroedit::Compatible(d->it[(size_t)row], field)) {
      first = row;
      break;
    }
  if (first < 0)
    return;
  const auto &a = d->it[(size_t)first].a;
  LONG value = 0, second = 0;
  bool ext = false;
  if (field == macroedit::Key) {
    int vk = 0;
    if (!CaptureKey(d->h, L(L_ED_CHANGE), vk, ext))
      return;
    value = vk;
  } else if (field == macroedit::Button) {
    if (!EdNumber(d, L(L_MOUSE_BUTTON_PROMPT), a.btn, 0, 4, value))
      return;
  } else if (field == macroedit::Coordinates) {
    if (!EdNumber(d, L(L_X_PROMPT), a.x, LONG_MIN, LONG_MAX, value) ||
        !EdNumber(d, L(L_Y_PROMPT), a.y, LONG_MIN, LONG_MAX, second))
      return;
  } else if (field == macroedit::Wheel) {
    if (!EdNumber(d, L(L_WHEEL_PROMPT), a.wheel, -32768, 32767, value))
      return;
  }
  EdPush(d);
  for (int row : sel)
    macroedit::Apply(d->it[(size_t)row], field, value, second, ext);
  d->changed = true;
  EdReload(d, sel, EdKeepTop(d));
}

static void EdUniversal(EdDlg *d) {
  auto sel = EdSel(d);
  LONG value = 0;
  if (!EdNumber(d, L(L_UNIVERSAL_PROMPT), 100, 0, MAX_DELAY_MS, value))
    return;
  bool changes = false;
  for (int row : macroedit::UniversalDelayRows(d->it, sel))
    if (d->it[(size_t)row].ms != (DWORD)value)
      changes = true;
  if (!changes)
    return;
  EdPush(d);
  macroedit::UniversalDelay(d->it, sel, (DWORD)value);
  d->changed = true;
  EdReload(d, sel, EdKeepTop(d));
}

static void EdPanelRun(EdDlg *d, int i) {
  if (!EdPanelEnabled(d, i))
    return;
  switch (i) {
  case EP_EXCLUSIONS:
    EdOpenOptions(d, false);
    break;
  case EP_VISIBILITY:
    EdOpenOptions(d, true);
    break;
  case EP_UNIVERSAL:
    EdUniversal(d);
    break;
  case EP_KEYDOWN:
    EdBulk(d, macroedit::KeyDown);
    break;
  case EP_KEYUP:
    EdBulk(d, macroedit::KeyUp);
    break;
  case EP_BUTTON:
    EdBulk(d, macroedit::Button);
    break;
  case EP_MOUSEDOWN:
    EdBulk(d, macroedit::MouseDown);
    break;
  case EP_MOUSEUP:
    EdBulk(d, macroedit::MouseUp);
    break;
  case EP_COORD:
    EdBulk(d, macroedit::Coordinates);
    break;
  case EP_WHEEL:
    EdBulk(d, macroedit::Wheel);
    break;
  case EP_PAUSE:
    Engine_TogglePause();
    break;
  default:
    break;
  }
}

static void EdToggleMode(EdDlg *d) {
  if (d->preview)
    return;
  RECT window;
  GetWindowRect(d->h, &window);
  int delta = d->editMode ? -ED_PANEL_H : ED_PANEL_H;
  d->editMode = !d->editMode;
  // Expand upwards: the list and existing buttons stay in their screen
  // positions.
  MONITORINFO monitor = {sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(d->h, MONITOR_DEFAULTTONEAREST), &monitor);
  int y = (std::max)((int)monitor.rcWork.top, (int)window.top - delta);
  SetWindowPos(d->h, nullptr, window.left, y, EDT_W, EDT_H + EdOffset(d),
               SWP_NOZORDER | SWP_NOACTIVATE);
  SetWindowPos(d->lb, nullptr, EDT_LX + 1, EDT_LY + 1 + EdOffset(d), 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  GfxRoundRegion(d->h);
  InvalidateRect(d->h, nullptr, TRUE);
}

static int EdHit(EdDlg *d, const POINT &pt) {
  RECT cr = GfxCloseRect(EDT_W);
  if (PtInRect(&cr, pt))
    return 100;
  RECT mode = EdModeRect();
  if (PtInRect(&mode, pt))
    return ED_MODE;
  if (d->editMode)
    for (int i = 0; i < EP_COUNT; ++i) {
      RECT r = EdPanelRect(i);
      if (PtInRect(&r, pt))
        return ED_PANEL_BASE + i;
    }
  for (int i = 0; i < EB_COUNT; i++) {
    RECT r = EdBtn(d, i);
    if (PtInRect(&r, pt))
      return i;
  }
  return -1;
}

static std::wstring EdKeyText(const MAc &action,
                              const EditorPreferences &prefs) {
  std::wstring text = KeyLabel(action.vk);
  if (prefs.exclude.KeyBlocked(action.vk, action.flags))
    text += std::wstring(L" \u2014 ") + L(L_EXCLUDED);
  return text;
}

static void EdDrawRow(EdDlg *d, DRAWITEMSTRUCT *di) {
  HDC dc = di->hDC;
  RECT r = di->rcItem;
  bool sel = (di->itemState & ODS_SELECTED) != 0;
  int idx = (int)di->itemID;
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
  Gdiplus::SolidBrush bg(sel ? SelCol() : GdiCol(FieldCol(false)));
  g.FillRectangle(&bg, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                  (REAL)(r.bottom - r.top));
  if (idx < 0 || idx >= (int)d->it.size())
    return;
  const EdItem &e = d->it[(size_t)idx];
  const MAc &a = e.a;
  const wchar_t *act = L"";
  std::wstring det;
  wchar_t b[96];
  if (e.isDelay) {
    act = L(L_ED_DELAY_ROW);
    swprintf_s(b, L"%lu ms", (unsigned long)e.ms);
    det = b;
  } else {
    switch (a.type) {
    case A_KD:
      act = L(L_ED_KD);
      det = EdKeyText(a, d->prefs);
      break;
    case A_KU:
      act = L(L_ED_KU);
      det = EdKeyText(a, d->prefs);
      break;
    case A_MD:
    case A_MU: {
      act = a.type == A_MD ? L(L_ED_MD) : L(L_ED_MU);
      switch (a.btn) {
      case 0:
        det = L(L_REC_LEFT);
        break;
      case 1:
        det = L(L_REC_RIGHT);
        break;
      case 2:
        det = L(L_REC_MIDDLE);
        break;
      case 3:
        det = L"X1";
        break;
      default:
        det = L"X2";
        break;
      }
      break;
    }
    case A_MOVE:
      act = L(L_ED_MV);
      break;
    default:
      act = L(L_ED_WH);
      det = a.horizontal ? (a.wheel > 0 ? L"\u2192" : L"\u2190")
                         : (a.wheel > 0 ? L"\u2191" : L"\u2193");
      break;
    }
  }
  Gdiplus::Color txt = sel ? Gdiplus::Color(240, 245, 250) : GdiCol(ThText());
  Gdiplus::Color dim = sel ? Gdiplus::Color(215, 225, 235) : GdiCol(ThDim());
  bool mouseBtn = !e.isDelay && (a.type == A_MD || a.type == A_MU);
  Gdiplus::Color actCol =
      (e.isDelay || !(IsKeyEv(a) || mouseBtn)) ? dim : txt;
  REAL y = (REAL)r.top, h = (REAL)(r.bottom - r.top);
  swprintf_s(b, L"%d", idx + 1);
  PutText(g, dc, b, (REAL)r.left + 4, y, 38, h, dim, 2, 8);
  PutText(g, dc, act, (REAL)r.left + 50, y, 110, h, actCol, 0, 9, mouseBtn);
  PutText(g, dc, det, (REAL)r.left + 164, y, (REAL)(r.right - r.left - 170), h,
          txt, 0, 9, IsKeyIt(e) || mouseBtn || e.isDelay);
}

static void EdDragUpdate(EdDlg *d) {
  RECT client;
  GetClientRect(d->lb, &client);
  int top = (int)SendMessageW(d->lb, LB_GETTOPINDEX, 0, 0);
  int endY = d->dragPoint.y + top * EDT_ROW;
  int first = (std::min)(d->dragStartY, endY) / EDT_ROW;
  int last = (std::max)(d->dragStartY, endY) / EDT_ROW;
  SendMessageW(d->lb, WM_SETREDRAW, FALSE, 0);
  SendMessageW(d->lb, LB_SETSEL, FALSE, -1);
  if (d->dragCtrl)
    for (int row : d->dragBase)
      SendMessageW(d->lb, LB_SETSEL, TRUE, row);
  bool intersects = (std::max)(d->dragStart.x, d->dragPoint.x) >= 0 &&
                    (std::min)(d->dragStart.x, d->dragPoint.x) < client.right;
  if (intersects)
    for (int row = (std::max)(0, first); row <= last && row < (int)d->it.size();
         ++row) {
      bool selected =
          !d->dragCtrl ||
          !std::binary_search(d->dragBase.begin(), d->dragBase.end(), row);
      SendMessageW(d->lb, LB_SETSEL, selected, row);
    }
  SendMessageW(d->lb, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(d->lb, nullptr, FALSE);
  InvalidateRect(d->h, nullptr, FALSE);
}

static LRESULT CALLBACK EdListProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR,
                                   DWORD_PTR data) {
  EdDlg *d = (EdDlg *)data;
  if (m == WM_LBUTTONDOWN && !d->preview) {
    d->dragPending = true;
    d->dragging = false;
    d->dragCtrl = (w & MK_CONTROL) != 0;
    d->dragStart = d->dragPoint = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    d->dragStartY =
        d->dragStart.y + (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0) * EDT_ROW;
    d->dragBase = EdSel(d);
    SetTimer(h, 0xED17, 30, nullptr);
  } else if (m == WM_MOUSEMOVE && d->dragPending) {
    d->dragPoint = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    if (!d->dragging &&
        (abs(d->dragPoint.x - d->dragStart.x) >= GetSystemMetrics(SM_CXDRAG) ||
         abs(d->dragPoint.y - d->dragStart.y) >= GetSystemMetrics(SM_CYDRAG))) {
      d->dragging = true;
      SetCapture(h);
    }
    if (d->dragging) {
      EdDragUpdate(d);
      return 0;
    }
  } else if (m == WM_TIMER && w == 0xED17 && d->dragging) {
    RECT client;
    GetClientRect(h, &client);
    int top = (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
    if (d->dragPoint.y < 0)
      --top;
    else if (d->dragPoint.y >= client.bottom)
      ++top;
    SendMessageW(h, LB_SETTOPINDEX, (std::max)(0, top), 0);
    EdDragUpdate(d);
    return 0;
  } else if (m == WM_LBUTTONUP || m == WM_CAPTURECHANGED ||
             m == WM_CANCELMODE) {
    bool dragged = d->dragging;
    d->dragPending = d->dragging = false;
    KillTimer(h, 0xED17);
    if (m == WM_LBUTTONUP && dragged) {
      if (GetCapture() == h)
        ReleaseCapture();
      InvalidateRect(h, nullptr, FALSE);
      return 0;
    }
    InvalidateRect(h, nullptr, FALSE);
  }
  LRESULT result = DefSubclassProc(h, m, w, l);
  if (m == WM_PAINT && d->dragging) {
    int top = (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
    LONG beginY = d->dragStartY - top * EDT_ROW;
    RECT box = {(std::min)(d->dragStart.x, d->dragPoint.x),
                (std::min)(beginY, d->dragPoint.y),
                (std::max)(d->dragStart.x, d->dragPoint.x),
                (std::max)(beginY, d->dragPoint.y)};
    HDC dc = GetDC(h);
    DrawFocusRect(dc, &box);
    ReleaseDC(h, dc);
  }
  return result;
}

static LRESULT CALLBACK EditorProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    EdDlg *nd = (EdDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
    nd->h = h;
    nd->visibilityMask = nd->prefs.customVisibility ? nd->prefs.visibility
                                                    : EdDefaultVisibility();
    nd->lb = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED |
            LBS_NODATA | LBS_EXTENDEDSEL | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT |
            LBS_NOINTEGRALHEIGHT,
        EDT_LX + 1, EDT_LY + 1, EDT_LW - 2, EDT_LH - 2, h,
        (HMENU)(INT_PTR)EDT_LIST_ID, g_hInst, nullptr);
    SendMessageW(nd->lb, LB_SETITEMHEIGHT, 0, EDT_ROW);
    SendMessageW(nd->lb, LB_SETCOUNT, (WPARAM)nd->it.size(), 0);
    SetWindowSubclass(nd->lb, EdListProc, 1, (DWORD_PTR)nd);
    SetTimer(h, 7, 30, nullptr);
    ApplyListTheme(nd->lb);
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  EdDlg *d = (EdDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_TIMER:
    if (w == 7 && !d->preview)
      EdApplyVisibility(d);
    if (w == 7 && d->preview && g_state.load() == ST_IDLE) {
      d->preview = false;
      EnableWindow(d->lb, TRUE);
      SetFocus(d->lb);
      InvalidateRect(h, nullptr, FALSE);
    }
    return 0;
  case WM_KEYDOWN:
    if (d->preview && w == VK_ESCAPE) {
      Engine_PlayStop();
      return 0;
    }
    break;
  case WM_MEASUREITEM: {
    MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)l;
    if (mi->CtlType == ODT_LISTBOX) {
      mi->itemHeight = EDT_ROW;
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)l;
    if (di->CtlType == ODT_LISTBOX) {
      EdDrawRow(d, di);
      return TRUE;
    }
    break;
  }
  case WM_CTLCOLORLISTBOX:
    return (LRESULT)FieldBrush();
  case WM_VKEYTOITEM: {
    int vk = LOWORD(w);
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (vk == VK_DELETE) {
      EdRunButton(d, EB_DEL);
      return -2;
    }
    if (vk == VK_INSERT) {
      EdRunButton(d, EB_ADD);
      return -2;
    }
    if (vk == VK_RETURN) {
      EdRowAction(d);
      return -2;
    }
    if (ctrl && vk == 'A') { // select everything, e.g. to set all delays at once
      SendMessageW(d->lb, LB_SETSEL, TRUE, (LPARAM)-1);
      InvalidateRect(h, nullptr, FALSE);
      return -2;
    }
    if (ctrl && vk == 'Z') { // Ctrl+Z undo, Ctrl+Shift+Z redo
      bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      EdRunButton(d, shift ? EB_REDO : EB_UNDO);
      return -2;
    }
    if (ctrl && vk == 'Y') {
      EdRunButton(d, EB_REDO);
      return -2;
    }
    if (ctrl && vk == VK_UP) {
      EdRunButton(d, EB_UP);
      return -2;
    }
    if (ctrl && vk == VK_DOWN) {
      EdRunButton(d, EB_DOWN);
      return -2;
    }
    if (vk == VK_ESCAPE) {
      EdFinish(d, false);
      return -2;
    }
    return -1;
  }
  case WM_COMMAND:
    if (LOWORD(w) == EDT_LIST_ID) {
      if (HIWORD(w) == LBN_DBLCLK)
        EdRowAction(d);
      else if (HIWORD(w) == LBN_SELCHANGE)
        InvalidateRect(h, nullptr, FALSE);
    }
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = GfxCloseRect(EDT_W);
    RECT mode = EdModeRect();
    if (pt.y < 30 && !PtInRect(&cr, pt) && !PtInRect(&mode, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int nh = EdHit(d, pt);
    if (nh != d->hover) {
      d->hover = nh;
      InvalidateRect(h, nullptr, FALSE);
    }
    SetCursor(LoadCursorW(nullptr, nh >= 0 ? IDC_HAND : IDC_ARROW));
    if (!d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    d->tracking = false;
    d->hover = -1;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int b = EdHit(d, pt);
    if (b == 100)
      EdFinish(d, false);
    else if (b == ED_MODE)
      EdToggleMode(d);
    else if (b >= ED_PANEL_BASE)
      EdPanelRun(d, b - ED_PANEL_BASE);
    else if (b >= 0)
      EdRunButton(d, b);
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC mem, Gdiplus::Graphics &g, const RECT &rc) {
      std::wstring title = L(L_ED_TITLE);
      if (!g_lastFile.empty())
        title += L" \u2014 " + Macros_Stem(g_lastFile);
      GfxChrome(g, mem, rc.right, L"", d->hover == 100 ? 2 : 0, CHB_SEP);
      PutText(g, mem, title, 30, 0, (REAL)(EdModeRect().left - 40), 30,
              GdiCol(ThText()));
      PaintBtn(g, mem, EdModeRect(), L(L_EDIT_MODE), d->hover == ED_MODE,
               d->editMode, !d->preview);
      if (d->editMode)
        for (int i = 0; i < EP_COUNT; ++i) {
          const wchar_t *label = EdPanelText(i);
          PaintBtn(g, mem, EdPanelRect(i), label, d->hover == ED_PANEL_BASE + i,
                   false, EdPanelEnabled(d, i));
        }
      int offset = EdOffset(d);
      RECT fr = {EDT_LX, EDT_LY + offset, EDT_LX + EDT_LW,
                 EDT_LY + EDT_LH + offset};
      FrameRect(g, fr);
      for (int i = 0; i < EB_COUNT; i++)
        PaintBtn(g, mem, EdBtn(d, i),
                 d->preview && i == EB_PLAY ? L(L_STOP) : EdBtnText(i),
                 d->hover == i, i == EB_OK, EdEnabled(d, i));

    });
    return 0;
  case WM_CLOSE:
    EdFinish(d, false);
    return 0;
  case WM_DESTROY:
    KillTimer(h, 7);
    if (d->preview) {
      Engine_PlayStop();
      Engine_Shutdown();
    }
    if (d->owner) {
      EnableWindow(d->owner, TRUE);
      SetForegroundWindow(d->owner);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

// Edits the current macro (g_events). Saves straight to its file if it has
// one, otherwise leaves it marked as unsaved.
static void RunEditor(HWND owner) {
  if (g_events.empty()) {
    StatusShow(L(L_ST_NOTHING), 2500);
    return;
  }
  EdDlg d;
  d.prefs = EditorPrefs_Get(g_lastFile);
  d.visibilityMask =
      d.prefs.customVisibility ? d.prefs.visibility : EdDefaultVisibility();
  d.it = EdFromEvents(g_events, d.tail, d.visibilityMask);
  d.owner = owner;
  ModalGuard mg;
  int x, y;
  PlaceOver(owner, EDT_W, EDT_H, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyEditor", L"",
                           WS_POPUP | WS_CLIPCHILDREN, x, y, EDT_W, EDT_H,
                           owner, nullptr, g_hInst, &d);
  if (!h) {
    EnableWindow(owner, TRUE);
    return;
  }
  GfxRoundRegion(h);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  SetFocus(d.lb);
  SendMessageW(d.lb, LB_SETSEL, TRUE, 0);
  RunModal(h, d.done);
  if (g_hwnd)
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Saving / loading the current macro
// ---------------------------------------------------------------------------

bool Macros_SaveCurrent(HWND owner) {
  if (!owner)
    owner = g_hwnd;
  if (g_state.load() != ST_IDLE)
    return false;
  if (g_events.empty()) {
    StatusShow(L(L_ST_NOTHING), 2500);
    return false;
  }
  std::wstring init = g_lastFile.empty() ? std::wstring() : Macros_Stem(g_lastFile);
  for (;;) {
    std::wstring typed;
    if (!PromptText(owner, L(L_MACROS), L(L_MC_NAME_PROMPT), init, false, 80,
                    typed))
      return false;
    std::wstring nm = CleanName(typed);
    if (nm.empty()) {
      init.clear();
      continue;
    }
    std::wstring path = Macros_Dir() + L"\\" + nm + L".onty";
    if (!SamePath(path, g_lastFile) &&
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
      if (UiMessage(owner, L(L_MC_REPLACE_Q), L(L_MACROS), UI_YESNO,
                    UI_ICON_WARN) != IDYES) {
        init = nm;
        continue;
      }
    }
    if (!Engine_Save(path.c_str())) {
      StatusShow(L(L_SAVEFAIL), 3000);
      return false;
    }
    bool prefsSaved =
        g_lastFile.empty()
            ? EditorPrefs_SaveTemporary(path)
            : (SamePath(g_lastFile, path) ||
               EditorPrefs_Set(path, EditorPrefs_Get(g_lastFile)));
    if (!prefsSaved)
      UiMessage(owner, L(L_PREFS_SAVEFAIL), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
    g_lastFile = path;
    g_dirty = false;
    RecentAdd(path);
    StatusShow(L(L_ST_SAVED), 2500);
    if (g_hwnd)
      InvalidateRect(g_hwnd, nullptr, FALSE);
    return true;
  }
}

// Makes `path` the current macro. Asks first if that would throw away a
// recording or edits that were never saved.
static bool LoadIntoApp(HWND parent, const std::wstring &path) {
  if (g_dirty && !g_events.empty()) {
    if (UiMessage(parent, L(L_MC_DISCARD_Q), L(L_MACROS), UI_YESNO,
                  UI_ICON_WARN) != IDYES)
      return false;
  }
  if (!Engine_Load(path.c_str())) {
    StatusShow(L(L_ST_INVALID), 2500);
    UiMessage(parent, L(L_ST_INVALID), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
    return false;
  }
  g_lastFile = path;
  g_dirty = false;
  RecentAdd(path);
  StatusShow(L(L_ST_LOADED), 2500);
  if (g_hwnd)
    InvalidateRect(g_hwnd, nullptr, FALSE);
  return true;
}

bool Macros_LoadByName(HWND owner, const std::wstring &name) {
  if (!owner)
    owner = g_hwnd;
  if (g_modal > 0 || name.empty())
    return false;
  std::wstring path = Macros_Dir() + L"\\" + name + L".onty";
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    StatusShow(L(L_ST_INVALID), 2500);
    return false;
  }
  bool ok = true;
  if (!(SamePath(path, g_lastFile) && !g_dirty))
    ok = LoadIntoApp(owner, path);
  if (ok)
    StatusShow((std::wstring(L(L_ST_LOADED)) + L": " + name).c_str(), 2500);
  return ok;
}

// ---------------------------------------------------------------------------
// Macro library window
// ---------------------------------------------------------------------------

static const int LIB_W = 360, LIB_H = 374, LIB_LIST_ID = 201, LIB_SEARCH_ID = 202;
static const int LIB_LX = 10, LIB_LY = 64, LIB_LW = 210, LIB_LH = 298;
static const int LIB_SEARCH_H = 28;            // search strip under the list
static const size_t LIB_SEARCH_MIN = 6;        // show the search from this many macros
enum { LIB_MSG_ESC = WM_APP + 130 };

enum {
  LBT_LOAD,
  LBT_EDIT,
  LBT_RENAME,
  LBT_DELETE,
  LBT_HOTKEY,
  LBT_SETTINGS,
  LBT_SAVE,
  LBT_BROWSE,
  LBT_FOLDER,
  LBT_COUNT
};

struct LibRow {
  std::wstring path; // empty = the current, never-saved recording
  std::wstring name;
  bool current = false;
};

struct LibDlg {
  HWND h = nullptr, lb = nullptr, owner = nullptr, search = nullptr;
  std::vector<LibRow> rows;
  std::wstring filter;  // what is typed in the search box
  std::wstring libDir;  // the library folder (cached)
  bool showSearch = false;
  bool done = false, tracking = false;
  int hover = -1; // LB_* index, or 100 for the close button
};

static RECT LibBtn(int i) {
  int x = LIB_W - 130;
  int y = LIB_LY + i * 34;
  return {x, y, x + 120, y + 28};
}

static const wchar_t *LibBtnText(int i) {
  switch (i) {
  case LBT_LOAD:
    return L(L_MC_LOAD);
  case LBT_EDIT:
    return L(L_MC_EDIT);
  case LBT_RENAME:
    return L(L_MC_RENAME);
  case LBT_DELETE:
    return L(L_MC_DELETE);
  case LBT_HOTKEY:
    return L(L_MC_HOTKEY);
  case LBT_SETTINGS:
    return L(L_MC_SETTINGS);
  case LBT_SAVE:
    return L(L_MC_SAVE);
  case LBT_BROWSE:
    return L(L_MC_BROWSE);
  default:
    return L(L_MC_FOLDER);
  }
}

static int LibCur(LibDlg *d) {
  int s = (int)SendMessageW(d->lb, LB_GETCURSEL, 0, 0);
  return (s >= 0 && s < (int)d->rows.size()) ? s : -1;
}

// Hotkeys and per-macro settings only exist for files in the library folder.
static bool LibInLibrary(LibDlg *d, const std::wstring &path) {
  return !path.empty() && SamePath(DirOf(path), d->libDir);
}

static std::wstring LowerStr(std::wstring s) {
  for (wchar_t &c : s)
    c = (wchar_t)towlower(c);
  return s;
}

// The list's frame; shrinks when the search strip is shown under it.
static RECT LibListRect(const LibDlg *d) {
  return {LIB_LX, LIB_LY, LIB_LX + LIB_LW,
          LIB_LY + LIB_LH - (d->showSearch ? LIB_SEARCH_H + 4 : 0)};
}

static RECT LibSearchRect() {
  return {LIB_LX, LIB_LY + LIB_LH - LIB_SEARCH_H, LIB_LX + LIB_LW,
          LIB_LY + LIB_LH};
}

static bool LibEnabled(LibDlg *d, int b) {
  int s = LibCur(d);
  switch (b) {
  case LBT_LOAD:
  case LBT_RENAME:
  case LBT_DELETE:
    return s >= 0 && !d->rows[(size_t)s].path.empty();
  case LBT_EDIT:
    return s >= 0;
  case LBT_HOTKEY:
  case LBT_SETTINGS:
    return s >= 0 && LibInLibrary(d, d->rows[(size_t)s].path);
  case LBT_SAVE:
    return !g_events.empty();
  default:
    return true;
  }
}

static void LibRebuild(LibDlg *d, const std::wstring &keepPath) {
  d->rows.clear();
  std::vector<std::wstring> files = ListMacros();
  // The search box only shows up once there are enough macros to need it.
  bool wantSearch = files.size() >= LIB_SEARCH_MIN;
  if (wantSearch != d->showSearch || (!wantSearch && !d->filter.empty())) {
    d->showSearch = wantSearch;
    if (!wantSearch) {
      d->filter.clear();
      if (d->search)
        SetWindowTextW(d->search, L"");
    }
  }
  std::wstring needle = d->showSearch ? LowerStr(d->filter) : std::wstring();
  auto matches = [&](const std::wstring &name) {
    return needle.empty() || LowerStr(name).find(needle) != std::wstring::npos;
  };
  bool curFound = false;
  for (const auto &p : files) {
    LibRow r;
    r.path = p;
    r.name = Macros_Stem(p);
    r.current = SamePath(p, g_lastFile);
    curFound = curFound || r.current;
    if (matches(r.name))
      d->rows.push_back(r);
  }
  // The current macro may be a file opened from somewhere else, or a fresh
  // recording that has no file yet - keep it visible at the top either way.
  if (!curFound && (!g_lastFile.empty() || !g_events.empty())) {
    LibRow r;
    r.path = g_lastFile;
    r.name = g_lastFile.empty() ? std::wstring(L(L_NEWMACRO))
                                : Macros_Stem(g_lastFile);
    r.current = true;
    if (matches(r.name))
      d->rows.insert(d->rows.begin(), r);
  }
  if (d->lb) {
    RECT lr = LibListRect(d);
    MoveWindow(d->lb, lr.left + 1, lr.top + 1, lr.right - lr.left - 2,
               lr.bottom - lr.top - 2, TRUE);
  }
  if (d->search)
    ShowWindow(d->search, d->showSearch ? SW_SHOW : SW_HIDE);
  SendMessageW(d->lb, LB_SETCOUNT, (WPARAM)d->rows.size(), 0);
  ShowWindow(d->lb, d->rows.empty() ? SW_HIDE : SW_SHOW);
  int pick = -1;
  for (size_t i = 0; i < d->rows.size(); i++) {
    if (!keepPath.empty() && SamePath(d->rows[i].path, keepPath)) {
      pick = (int)i;
      break;
    }
    if (pick < 0 && d->rows[i].current)
      pick = (int)i;
  }
  if (pick < 0 && !d->rows.empty())
    pick = 0;
  if (pick >= 0)
    SendMessageW(d->lb, LB_SETCURSEL, (WPARAM)pick, 0);
  InvalidateRect(d->lb, nullptr, TRUE);
  InvalidateRect(d->h, nullptr, FALSE);
}

static void LibDrawRow(LibDlg *d, DRAWITEMSTRUCT *di) {
  HDC dc = di->hDC;
  RECT r = di->rcItem;
  bool sel = (di->itemState & ODS_SELECTED) != 0;
  int idx = (int)di->itemID;
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
  Gdiplus::SolidBrush bg(sel ? SelCol() : GdiCol(FieldCol(false)));
  g.FillRectangle(&bg, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                  (REAL)(r.bottom - r.top));
  if (idx < 0 || idx >= (int)d->rows.size())
    return;
  const LibRow &row = d->rows[(size_t)idx];
  Gdiplus::Color txt = sel ? Gdiplus::Color(240, 245, 250) : GdiCol(ThText());
  REAL cy = (REAL)(r.top + r.bottom) / 2.0f;
  if (row.current) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    bool unsaved = g_dirty || row.path.empty();
    DrawGlyph(dc, g, (REAL)r.left + 14.0f, cy, 11,
              unsaved ? (wchar_t)0xE7BA : (wchar_t)0xE73E,
              unsaved ? RGB(224, 176, 64) : (sel ? RGB(240, 245, 250) : ThText()));
  }
  // load hotkey, right-aligned
  REAL nameW = (REAL)(r.right - r.left - 36);
  if (!row.path.empty()) {
    if (const MacroMeta *mm = MacroMeta_Find(row.path)) {
      if (mm->hkVk) {
        Gdiplus::Color dim =
            sel ? Gdiplus::Color(215, 225, 235) : GdiCol(ThDim());
        PutText(g, dc, FmtHk(mm->hkMods, mm->hkVk), (REAL)r.right - 72,
                (REAL)r.top, 66.0f, (REAL)(r.bottom - r.top), dim, 2, 8);
        nameW -= 70.0f;
      }
    }
  }
  PutText(g, dc, row.name, (REAL)r.left + 30, (REAL)r.top, nameW,
          (REAL)(r.bottom - r.top), txt, 0, 9, row.current);
}

static void LibLoad(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0 || d->rows[(size_t)s].path.empty())
    return;
  const LibRow &row = d->rows[(size_t)s];
  if (row.current && !g_dirty) { // already the current, untouched macro
    d->done = true;
    DestroyWindow(d->h);
    return;
  }
  if (LoadIntoApp(d->h, row.path)) {
    d->done = true;
    DestroyWindow(d->h);
  }
}

static void LibEdit(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0)
    return;
  LibRow row = d->rows[(size_t)s];
  if (!row.current) {
    if (!LoadIntoApp(d->h, row.path))
      return;
  }
  RunEditor(d->h);
  LibRebuild(d, g_lastFile);
}

static void LibRename(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0 || d->rows[(size_t)s].path.empty())
    return;
  LibRow row = d->rows[(size_t)s];
  std::wstring typed;
  if (!PromptText(d->h, L(L_MC_RENAME), L(L_MC_NAME_PROMPT), row.name, false, 80,
                  typed))
    return;
  std::wstring nm = CleanName(typed);
  if (nm.empty() || nm == row.name)
    return;
  std::wstring np = DirOf(row.path) + L"\\" + nm + L".onty";
  if (!SamePath(np, row.path) &&
      GetFileAttributesW(np.c_str()) != INVALID_FILE_ATTRIBUTES) {
    UiMessage(d->h, L(L_MC_EXISTS), L(L_MACROS), UI_OK, UI_ICON_WARN);
    return;
  }
  if (!MoveFileW(row.path.c_str(), np.c_str())) {
    StatusShow(L(L_MC_FAIL), 2500);
    return;
  }
  if (!EditorPrefs_Rename(row.path, np))
    UiMessage(d->h, L(L_PREFS_SAVEFAIL), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
  MacroMeta_Rename(row.path, np); // hotkey / settings follow the new name
  if (SamePath(row.path, g_lastFile)) {
    g_lastFile = np;
    RecentAdd(np);
    if (g_hwnd)
      InvalidateRect(g_hwnd, nullptr, FALSE);
  }
  LibRebuild(d, np);
}

static void LibDelete(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0 || d->rows[(size_t)s].path.empty())
    return;
  LibRow row = d->rows[(size_t)s];
  if (UiMessage(d->h, L(L_MC_DELETE_Q), L(L_MACROS), UI_YESNO, UI_ICON_WARN) !=
      IDYES)
    return;
  if (!DeleteFileW(row.path.c_str())) {
    StatusShow(L(L_MC_FAIL), 2500);
    return;
  }
  if (SamePath(row.path, g_lastFile))
    EditorPrefs_Set(L"", EditorPrefs_Get(row.path));
  if (!EditorPrefs_Remove(row.path))
    UiMessage(d->h, L(L_PREFS_SAVEFAIL), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
  MacroMeta_Remove(row.path);
  if (SamePath(row.path, g_lastFile)) {
    // the recording is still in memory, it just has no file any more
    g_lastFile.clear();
    g_dirty = !g_events.empty();
    if (g_hwnd)
      InvalidateRect(g_hwnd, nullptr, FALSE);
  }
  StatusShow(L(L_MC_DELETED), 2500);
  LibRebuild(d, std::wstring());
}

// ---- Hotkey / per-macro settings -------------------------------------------

static bool IsPlainOkKey(int vk) {
  return (vk >= VK_F1 && vk <= VK_F24) || vk == VK_PAUSE || vk == VK_SCROLL;
}

// Name of whatever already uses this hotkey, or empty.
static std::wstring HotkeyUser(int mods, int vk, const MacroMeta *except) {
  struct {
    int m, v;
    const wchar_t *n;
  } fixed[] = {{g_cfg.recordMods, g_cfg.recordVk, L(L_HK_REC)},
               {g_cfg.playMods, g_cfg.playVk, L(L_HK_PLAY)},
               {g_cfg.pauseMods, g_cfg.pauseVk, L(L_PAUSE_RESUME)},
               {g_cfg.acMods, g_cfg.acVk, L(L_AUTOCLICK)},
               {g_cfg.acPosMods, g_cfg.acPosVk, L(L_HK_AC_POS)},
               {g_cfg.panicMods, g_cfg.panicVk, L(L_PANIC_KEYS)}};
  for (const auto &f : fixed)
    if (f.v && f.v == vk && f.m == mods)
      return f.n;
  if (const MacroMeta *o = MacroMeta_ByHotkey(mods, vk, except))
    return o->name;
  return std::wstring();
}

static void LibSetHotkey(LibDlg *d, const LibRow &row) {
  int mo = 0, vk = 0;
  if (!UiHotkeyCapture(d->h, L(L_MC_HK_TITLE), mo, vk))
    return;
  if (mo == 0 && !IsPlainOkKey(vk)) { // a bare letter would break typing everywhere
    UiMessage(d->h, L(L_MC_HK_NEEDMOD), L(L_MACROS), UI_OK, UI_ICON_WARN);
    return;
  }
  MacroMeta *m = MacroMeta_Edit(row.path);
  if (!m)
    return;
  std::wstring who = HotkeyUser(mo, vk, m);
  if (!who.empty()) {
    std::wstring msg = std::wstring(L(L_MC_HK_TAKEN)) + L" " + who;
    UiMessage(d->h, msg.c_str(), L(L_MACROS), UI_OK, UI_ICON_ERROR);
    return;
  }
  m->hkMods = mo;
  m->hkVk = vk;
  MacroMeta_Save();
  StatusShow((std::wstring(L(L_MC_HK_SAVED)) + L": " + FmtHk(mo, vk)).c_str(),
             2500);
  LibRebuild(d, row.path);
}

static void LibRemoveHotkey(LibDlg *d, const LibRow &row) {
  MacroMeta *m = MacroMeta_Find(row.path);
  if (!m || !m->hkVk)
    return;
  m->hkMods = 0;
  m->hkVk = 0;
  MacroMeta_Save();
  StatusShow(L(L_MC_HK_REMOVED), 2500);
  LibRebuild(d, row.path);
}

// Opens a popup menu under the given button and returns the chosen command.
static int LibPopup(LibDlg *d, HMENU m, int btn) {
  GfxMenuTheme();
  RECT br = LibBtn(btn);
  POINT p = {br.left, br.top};
  ClientToScreen(d->h, &p);
  int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_RIGHTALIGN,
                                p.x, p.y, 0, d->h, nullptr);
  DestroyMenu(m);
  return cmd;
}

static void LibHotkey(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0 || !LibInLibrary(d, d->rows[(size_t)s].path))
    return;
  LibRow row = d->rows[(size_t)s];
  const MacroMeta *cur = MacroMeta_Find(row.path);
  HMENU m = CreatePopupMenu();
  std::wstring set = L(L_MC_HK_SET);
  if (cur && cur->hkVk)
    set += L"   (" + FmtHk(cur->hkMods, cur->hkVk) + L")";
  AppendMenuW(m, MF_STRING, 1, set.c_str());
  AppendMenuW(m, MF_STRING | ((cur && cur->hkVk) ? 0 : MF_GRAYED), 2,
              L(L_MC_HK_REMOVE));
  int cmd = LibPopup(d, m, LBT_HOTKEY);
  if (cmd == 1)
    LibSetHotkey(d, row);
  else if (cmd == 2)
    LibRemoveHotkey(d, row);
}

enum {
  MS_LOOPS_DEF = 100,
  MS_LOOPS_1,
  MS_LOOPS_2,
  MS_LOOPS_3,
  MS_LOOPS_5,
  MS_LOOPS_10,
  MS_LOOPS_100,
  MS_LOOPS_CONT,
  MS_LOOPS_CUSTOM,
  MS_SPEED_DEF = 200,
  MS_SPEED_05,
  MS_SPEED_1,
  MS_SPEED_2,
  MS_SPEED_5,
  MS_SPEED_10,
  MS_SPEED_TURBO,
  MS_SPEED_MAX,
  MS_SPEED_CUSTOM
};

static void LibSettings(LibDlg *d) {
  int s = LibCur(d);
  if (s < 0 || !LibInLibrary(d, d->rows[(size_t)s].path))
    return;
  LibRow row = d->rows[(size_t)s];
  const MacroMeta *cur = MacroMeta_Find(row.path);
  MacroMeta now; // what this macro uses now (defaults if it has no entry)
  if (cur)
    now = *cur;
  auto chk = [](bool on) { return (UINT)(on ? MF_CHECKED : 0); };
  wchar_t t[96];

  HMENU lp = CreatePopupMenu();
  AppendMenuW(lp, MF_STRING | chk(!now.ovLoops), MS_LOOPS_DEF,
              L(L_MC_OV_DEFAULT));
  AppendMenuW(lp, MF_SEPARATOR, 0, nullptr);
  const struct {
    int id, n;
    const wchar_t *txt;
  } presets[] = {{MS_LOOPS_1, 1, L(L_LOOPS1)},   {MS_LOOPS_2, 2, L(L_LOOPS2)},
                 {MS_LOOPS_3, 3, L(L_LOOPS3)},   {MS_LOOPS_5, 5, L(L_LOOPS5)},
                 {MS_LOOPS_10, 10, L(L_LOOPS10)}, {MS_LOOPS_100, 100, L(L_LOOPS100)}};
  bool preset = false;
  for (const auto &p : presets) {
    bool on = now.ovLoops && !now.continuous && now.loops == p.n;
    preset = preset || on;
    AppendMenuW(lp, MF_STRING | chk(on), p.id, p.txt);
  }
  AppendMenuW(lp, MF_STRING | chk(now.ovLoops && now.continuous),
              MS_LOOPS_CONT, L(L_CONT));
  swprintf_s(t, L"%s (%d)...", L(L_LOOPSCUSTOM), now.loops);
  AppendMenuW(lp, MF_STRING | chk(now.ovLoops && !now.continuous && !preset),
              MS_LOOPS_CUSTOM, t);

  HMENU sp = CreatePopupMenu();
  AppendMenuW(sp, MF_STRING | chk(!now.ovSpeed), MS_SPEED_DEF,
              L(L_MC_OV_DEFAULT));
  AppendMenuW(sp, MF_SEPARATOR, 0, nullptr);
  const struct {
    int id, code;
    const wchar_t *txt;
  } speeds[] = {{MS_SPEED_05, 0, L(L_SPEED05)},
                {MS_SPEED_1, 1, L(L_SPEED1)},
                {MS_SPEED_2, 2, L(L_SPEED2)},
                {MS_SPEED_5, 5, L(L_SPEED5)},
                {MS_SPEED_10, 10, L(L_SPEED10)},
                {MS_SPEED_TURBO, 100, L(L_SPEEDTURBO)},
                {MS_SPEED_MAX, -5, L(L_SPEED_MAX)}};
  for (const auto &p : speeds)
    AppendMenuW(sp, MF_STRING | chk(now.ovSpeed && now.speed == p.code), p.id,
                p.txt);
  swprintf_s(t, L"%s (%.4gx)...", L(L_SPEEDCUSTOM), now.speedCustom);
  AppendMenuW(sp, MF_STRING | chk(now.ovSpeed && now.speed == 999),
              MS_SPEED_CUSTOM, t);

  HMENU m = CreatePopupMenu();
  AppendMenuW(m, MF_POPUP, (UINT_PTR)lp, L(L_LOOPS));
  AppendMenuW(m, MF_POPUP, (UINT_PTR)sp, L(L_SPEED));
  int cmd = LibPopup(d, m, LBT_SETTINGS); // also frees the submenus
  if (!cmd)
    return;

  MacroMeta tmp = now;
  if (cmd == MS_LOOPS_DEF) {
    tmp.ovLoops = 0;
  } else if (cmd >= MS_LOOPS_1 && cmd <= MS_LOOPS_100) {
    for (const auto &p : presets)
      if (p.id == cmd)
        tmp.loops = p.n;
    tmp.ovLoops = 1;
    tmp.continuous = 0;
  } else if (cmd == MS_LOOPS_CONT) {
    tmp.ovLoops = 1;
    tmp.continuous = 1;
  } else if (cmd == MS_LOOPS_CUSTOM) {
    std::wstring out;
    if (!PromptText(d->h, L(L_LOOPS), L(L_Q_LOOPS), std::to_wstring(tmp.loops),
                    true, 5, out))
      return;
    long v = wcstol(out.c_str(), nullptr, 10);
    tmp.loops = (int)(v < 1 ? 1 : (v > 99999 ? 99999 : v));
    tmp.ovLoops = 1;
    tmp.continuous = 0;
  } else if (cmd == MS_SPEED_DEF) {
    tmp.ovSpeed = 0;
  } else if (cmd >= MS_SPEED_05 && cmd <= MS_SPEED_MAX) {
    for (const auto &p : speeds)
      if (p.id == cmd)
        tmp.speed = p.code;
    tmp.ovSpeed = 1;
  } else if (cmd == MS_SPEED_CUSTOM) {
    swprintf_s(t, L"%.4g", tmp.speedCustom);
    std::wstring out;
    if (!PromptText(d->h, L(L_SPEED), L(L_Q_SPEED), t, false, 8, out))
      return;
    double v = wcstod(out.c_str(), nullptr);
    if (!(v > 0.0))
      return;
    tmp.speedCustom = v < 0.01 ? 0.01 : (v > 1000.0 ? 1000.0 : v);
    tmp.speed = 999;
    tmp.ovSpeed = 1;
  } else {
    return;
  }
  MacroMeta *m2 = MacroMeta_Edit(row.path);
  if (!m2)
    return;
  m2->ovLoops = tmp.ovLoops;
  m2->loops = tmp.loops;
  m2->continuous = tmp.continuous;
  m2->ovSpeed = tmp.ovSpeed;
  m2->speed = tmp.speed;
  m2->speedCustom = tmp.speedCustom;
  MacroMeta_Save();
  StatusShow(L(L_ST_SAVED), 2000);
  if (g_hwnd)
    InvalidateRect(g_hwnd, nullptr, FALSE);
  InvalidateRect(d->h, nullptr, FALSE);
}

static void LibBrowse(LibDlg *d) {
  wchar_t path[MAX_PATH] = L"";
  std::wstring dir = Macros_Dir();
  OPENFILENAMEW o = {};
  o.lStructSize = sizeof(o);
  o.hwndOwner = d->h;
  o.lpstrFilter =
      L"AutoOnty (*.onty;*.json)\0*.onty;*.json\0All files\0*.*\0";
  o.lpstrFile = path;
  o.nMaxFile = MAX_PATH;
  o.lpstrInitialDir = dir.c_str();
  o.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  if (GetOpenFileNameW(&o)) {
    if (LoadIntoApp(d->h, path)) {
      d->done = true;
      DestroyWindow(d->h);
    }
  }
}

static void LibRun(LibDlg *d, int b) {
  if (!LibEnabled(d, b))
    return;
  switch (b) {
  case LBT_LOAD:
    LibLoad(d);
    break;
  case LBT_EDIT:
    LibEdit(d);
    break;
  case LBT_RENAME:
    LibRename(d);
    break;
  case LBT_DELETE:
    LibDelete(d);
    break;
  case LBT_HOTKEY:
    LibHotkey(d);
    break;
  case LBT_SETTINGS:
    LibSettings(d);
    break;
  case LBT_SAVE:
    if (Macros_SaveCurrent(d->h))
      LibRebuild(d, g_lastFile);
    break;
  case LBT_BROWSE:
    LibBrowse(d);
    break;
  case LBT_FOLDER:
    ShellExecuteW(d->h, L"open", Macros_Dir().c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
    break;
  }
}

static int LibHit(const POINT &pt) {
  RECT cr = GfxCloseRect(LIB_W);
  if (PtInRect(&cr, pt))
    return 100;
  for (int i = 0; i < LBT_COUNT; i++) {
    RECT r = LibBtn(i);
    if (PtInRect(&r, pt))
      return i;
  }
  return -1;
}

// Search box: Down / Enter jump to the list, Esc clears the text (or closes).
static LRESULT CALLBACK LibSearchProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  WNDPROC old = (WNDPROC)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_KEYDOWN) {
    HWND par = GetParent(h);
    LibDlg *d = (LibDlg *)GetWindowLongPtrW(par, GWLP_USERDATA);
    if (d && (w == VK_DOWN || w == VK_RETURN)) {
      SetFocus(d->lb);
      return 0;
    }
    if (w == VK_ESCAPE) {
      if (GetWindowTextLengthW(h) > 0)
        SetWindowTextW(h, L"");
      else
        PostMessageW(par, LIB_MSG_ESC, 0, 0);
      return 0;
    }
  }
  if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE))
    return 0; // no "ding"
  return CallWindowProcW(old, h, m, w, l);
}

static LRESULT CALLBACK LibraryProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    LibDlg *nd = (LibDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
    nd->h = h;
    nd->lb = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED |
            LBS_NODATA | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT |
            LBS_NOINTEGRALHEIGHT,
        LIB_LX + 1, LIB_LY + 1, LIB_LW - 2, LIB_LH - 2, h,
        (HMENU)(INT_PTR)LIB_LIST_ID, g_hInst, nullptr);
    SendMessageW(nd->lb, LB_SETITEMHEIGHT, 0, EDT_ROW + 2);
    ApplyListTheme(nd->lb);
    {
      RECT sr = LibSearchRect();
      nd->search = CreateWindowExW(
          0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
          sr.left + 6, sr.top + 5, sr.right - sr.left - 12, 18, h,
          (HMENU)(INT_PTR)LIB_SEARCH_ID, g_hInst, nullptr);
      SendMessageW(nd->search, WM_SETFONT, (WPARAM)ThFont(9), TRUE);
      SendMessageW(nd->search, EM_SETLIMITTEXT, 60, 0);
      SendMessageW(nd->search, EM_SETCUEBANNER, TRUE, (LPARAM)L(L_MC_SEARCH));
      WNDPROC old = (WNDPROC)SetWindowLongPtrW(nd->search, GWLP_WNDPROC,
                                               (LONG_PTR)LibSearchProc);
      SetWindowLongPtrW(nd->search, GWLP_USERDATA, (LONG_PTR)old);
    }
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  LibDlg *d = (LibDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
  case WM_MEASUREITEM: {
    MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)l;
    if (mi->CtlType == ODT_LISTBOX) {
      mi->itemHeight = EDT_ROW + 2;
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)l;
    if (di->CtlType == ODT_LISTBOX) {
      LibDrawRow(d, di);
      return TRUE;
    }
    break;
  }
  case WM_CTLCOLORLISTBOX:
    return (LRESULT)FieldBrush();
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w;
    SetTextColor(dc, ThText());
    SetBkColor(dc, FieldCol(false));
    return (LRESULT)FieldBrush();
  }
  case LIB_MSG_ESC:
    d->done = true;
    DestroyWindow(h);
    return 0;
  case WM_VKEYTOITEM: {
    int vk = LOWORD(w);
    if (vk == 'F' && (GetKeyState(VK_CONTROL) & 0x8000) && d->showSearch) {
      SetFocus(d->search);
      return -2;
    }
    if (vk == VK_RETURN) {
      LibRun(d, LBT_LOAD);
      return -2;
    }
    if (vk == VK_DELETE) {
      LibRun(d, LBT_DELETE);
      return -2;
    }
    if (vk == VK_F2) {
      LibRun(d, LBT_RENAME);
      return -2;
    }
    if (vk == VK_ESCAPE) {
      d->done = true;
      DestroyWindow(h);
      return -2;
    }
    return -1;
  }
  case WM_COMMAND:
    if (LOWORD(w) == LIB_LIST_ID) {
      if (HIWORD(w) == LBN_DBLCLK)
        LibRun(d, LBT_LOAD);
      else if (HIWORD(w) == LBN_SELCHANGE)
        InvalidateRect(h, nullptr, FALSE);
    } else if (LOWORD(w) == LIB_SEARCH_ID && HIWORD(w) == EN_CHANGE) {
      wchar_t sb[128] = L"";
      GetWindowTextW(d->search, sb, 128);
      if (d->filter != sb) { // (LibRebuild clears it itself when it hides the box)
        d->filter = sb;
        int cs = LibCur(d);
        LibRebuild(d, cs >= 0 ? d->rows[(size_t)cs].path : std::wstring());
      }
    }
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = GfxCloseRect(LIB_W);
    if (pt.y < 30 && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int nh = LibHit(pt);
    if (nh != d->hover) {
      d->hover = nh;
      InvalidateRect(h, nullptr, FALSE);
    }
    SetCursor(LoadCursorW(nullptr, nh >= 0 ? IDC_HAND : IDC_ARROW));
    if (!d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    d->tracking = false;
    d->hover = -1;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int b = LibHit(pt);
    if (b == 100) {
      d->done = true;
      DestroyWindow(h);
    } else if (b >= 0)
      LibRun(d, b);
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC mem, Gdiplus::Graphics &g, const RECT &rc) {
      GfxChrome(g, mem, rc.right, L(L_MACROS), d->hover == 100 ? 2 : 0,
                CHB_SEP);
      PutText(g, mem, Macros_CurrentLabel(), (REAL)LIB_LX, 34.0f,
              (REAL)(rc.right - 2 * LIB_LX), 24.0f, GdiCol(ThDim()), 0, 9);
      RECT fr = LibListRect(d);
      if (d->showSearch) {
        RECT sr = LibSearchRect();
        Gdiplus::SolidBrush sb(GdiCol(FieldCol(false)));
        g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        g.FillRectangle(&sb, (REAL)sr.left, (REAL)sr.top,
                        (REAL)(sr.right - sr.left), (REAL)(sr.bottom - sr.top));
        FrameRect(g, sr);
      }
      if (d->rows.empty()) {
        Gdiplus::SolidBrush inf(GdiCol(FieldCol(false)));
        g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        g.FillRectangle(&inf, (REAL)fr.left, (REAL)fr.top,
                        (REAL)(fr.right - fr.left), (REAL)(fr.bottom - fr.top));
        PutText(g, mem, d->filter.empty() ? L(L_MC_EMPTY) : L(L_MC_NOMATCH),
                (REAL)fr.left, (REAL)fr.top,
                (REAL)(fr.right - fr.left), (REAL)(fr.bottom - fr.top),
                GdiCol(ThDim()), 1, 9);
      }
      FrameRect(g, fr);
      for (int i = 0; i < LBT_COUNT; i++)
        PaintBtn(g, mem, LibBtn(i), LibBtnText(i), d->hover == i, false,
                 LibEnabled(d, i));
    });
    return 0;
  case WM_DESTROY:
    if (d->owner) {
      EnableWindow(d->owner, TRUE);
      SetForegroundWindow(d->owner);
    }
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

void Macros_ShowLibrary(HWND owner) {
  if (!owner)
    owner = g_hwnd;
  if (g_state.load() != ST_IDLE) {
    StatusShow(L(L_ST_AC_BUSY), 2500);
    return;
  }
  if (g_modal > 0)
    return;
  LibDlg d;
  d.owner = owner;
  d.libDir = Macros_Dir();
  ModalGuard mg;
  int x, y;
  PlaceOver(owner, LIB_W, LIB_H, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyMacros", L"",
                           WS_POPUP | WS_CLIPCHILDREN, x, y, LIB_W, LIB_H,
                           owner, nullptr, g_hInst, &d);
  if (!h) {
    EnableWindow(owner, TRUE);
    return;
  }
  GfxRoundRegion(h);
  LibRebuild(&d, std::wstring());
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  SetFocus(d.lb);
  RunModal(h, d.done);
  if (g_hwnd)
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

void Macros_RegisterClasses() {
  WNDCLASSW wc = {};
  wc.hInstance = g_hInst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpfnWndProc = LibraryProc;
  wc.lpszClassName = L"AutoOntyMacros";
  RegisterClassW(&wc);
  wc.lpfnWndProc = CheckProc;
  wc.lpszClassName = L"AutoOntyEditorOptions";
  RegisterClassW(&wc);
  wc.lpfnWndProc = EditorProc;
  wc.lpszClassName = L"AutoOntyEditor";
  RegisterClassW(&wc);
  wc.lpfnWndProc = PromptProc;
  wc.lpszClassName = L"AutoOntyPrompt";
  RegisterClassW(&wc);
  wc.lpfnWndProc = KeyCapProc;
  wc.lpszClassName = L"AutoOntyKeyCap";
  RegisterClassW(&wc);
}
