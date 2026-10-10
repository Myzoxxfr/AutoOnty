#include "ui/macros.h"
#include "core/config.h"
#include "engine/engine.h"
#include "lang/lang.h"
#include "ui/gfx.h"
#include "ui/msgbox.h"
#include "ui/statusbar.h"
#include <algorithm>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <vector>
#include <windows.h>
#include <windowsx.h>

#ifndef CSIDL_PERSONAL
#define CSIDL_PERSONAL 0x0005
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
  bool ok = false, done = false, tracking = false, hoverClose = false;
};

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
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
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
      PutText(g, mem, L(L_ED_PRESS_KEY), 0, 42.0f, (REAL)rc.right, 24.0f,
              GdiCol(ThDim()), 1, 10);
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

static bool CaptureKey(HWND owner, const wchar_t *title, int &vk, bool &ext) {
  KeyDlg d;
  d.title = title;
  d.owner = owner;
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
  RunModal(h, d.done);
  if (d.ok) {
    vk = d.vk;
    ext = d.ext;
  }
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
  EB_OK,
  EB_CANCEL,
  EB_COUNT
};

struct EdItem {
  bool isDelay = false;
  DWORD ms = 0; // when isDelay
  MAc a;        // when !isDelay (a.delay is always 0 inside the editor)
};

struct EdDlg {
  HWND h = nullptr, lb = nullptr, owner = nullptr;
  std::vector<EdItem> it;
  bool changed = false, ok = false, done = false, tracking = false;
  int hover = -1; // EB_* index, or 100 for the close button
};

static RECT EdBtn(int i) {
  int x = EDT_W - 126;
  if (i == EB_OK)
    return {x, EDT_H - 74, x + 116, EDT_H - 46};
  if (i == EB_CANCEL)
    return {x, EDT_H - 40, x + 116, EDT_H - 12};
  int y = EDT_LY + i * 34;
  return {x, y, x + 116, y + 28};
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
  case EB_OK:
    return L(L_OK);
  default:
    return L(L_CANCEL);
  }
}

static bool IsKeyEv(const MAc &a) { return a.type == A_KD || a.type == A_KU; }
static bool IsKeyIt(const EdItem &e) { return !e.isDelay && IsKeyEv(e.a); }

static std::vector<EdItem> EdFromEvents(const std::vector<MAc> &ev) {
  std::vector<EdItem> out;
  out.reserve(ev.size() * 2);
  for (const MAc &a : ev) {
    if (a.delay > 0) {
      EdItem d;
      d.isDelay = true;
      d.ms = a.delay;
      out.push_back(d);
    }
    EdItem e;
    e.a = a;
    e.a.delay = 0;
    out.push_back(std::move(e));
  }
  return out;
}

static std::vector<MAc> EdToEvents(const std::vector<EdItem> &items) {
  std::vector<MAc> out;
  out.reserve(items.size());
  long long pending = 0;
  for (const EdItem &e : items) {
    if (e.isDelay) {
      pending += e.ms;
      continue;
    }
    MAc a = e.a;
    a.delay = (DWORD)(pending > MAX_DELAY_MS ? MAX_DELAY_MS : pending);
    pending = 0;
    out.push_back(std::move(a));
  }
  return out; // trailing Delay rows have nothing to wait for and are dropped
}

static size_t EdEventCount(const EdDlg *d) {
  size_t n = 0;
  for (const EdItem &e : d->it)
    if (!e.isDelay)
      n++;
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
  std::vector<int> sel;
  switch (b) {
  case EB_ADD:
  case EB_ADDDELAY:
    return (int)d->it.size() + 3 <= MAX_EDIT_EVENTS * 2;
  case EB_KEY:
    sel = EdSel(d);
    return !sel.empty() && sel[0] < (int)d->it.size() &&
           IsKeyIt(d->it[(size_t)sel[0]]);
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

// Inserts: delay(100) key down, delay(50) key up - a normal tap.
static void EdAddKey(EdDlg *d) {
  if (!EdEnabled(d, EB_ADD))
    return;
  int vk = 0;
  bool ext = false;
  if (!CaptureKey(d->h, L(L_ED_ADD), vk, ext))
    return;
  std::vector<int> sel = EdSel(d);
  int pos = sel.empty() ? (int)d->it.size() : sel.back() + 1;
  if (pos > (int)d->it.size())
    pos = (int)d->it.size();
  std::vector<EdItem> add;
  add.push_back(MakeDelay(100));
  add.push_back(MakeKey(A_KD, vk, ext));
  add.push_back(MakeDelay(50));
  add.push_back(MakeKey(A_KU, vk, ext));
  d->it.insert(d->it.begin() + pos, add.begin(), add.end());
  d->changed = true;
  EdReload(d, {pos + 1, pos + 3}, EdTopFor(d, pos + 3));
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
  d->it.insert(d->it.begin() + pos, MakeDelay(v));
  d->changed = true;
  EdReload(d, {pos}, EdTopFor(d, pos));
}

static void EdChangeKey(EdDlg *d) {
  std::vector<int> sel = EdSel(d);
  if (sel.empty() || sel[0] >= (int)d->it.size() ||
      !IsKeyIt(d->it[(size_t)sel[0]]))
    return;
  int i = sel[0];
  int vk = 0;
  bool ext = false;
  if (!CaptureKey(d->h, L(L_ED_CHANGE), vk, ext))
    return;
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
  if (!moved)
    return;
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
  if (apply) {
    if (EdEventCount(d) == 0) {
      StatusShow(L(L_ST_NOTHING), 2500);
      return;
    }
    if (d->changed) {
      g_events = EdToEvents(d->it);
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

static void EdRunButton(EdDlg *d, int b) {
  if (!EdEnabled(d, b))
    return;
  switch (b) {
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
  case EB_OK:
    EdFinish(d, true);
    break;
  case EB_CANCEL:
    EdFinish(d, false);
    break;
  }
}

static int EdHit(const POINT &pt) {
  RECT cr = GfxCloseRect(EDT_W);
  if (PtInRect(&cr, pt))
    return 100;
  for (int i = 0; i < EB_COUNT; i++) {
    RECT r = EdBtn(i);
    if (PtInRect(&r, pt))
      return i;
  }
  return -1;
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
      det = KeyLabel(a.vk);
      break;
    case A_KU:
      act = L(L_ED_KU);
      det = KeyLabel(a.vk);
      break;
    case A_MD:
    case A_MU: {
      act = a.type == A_MD ? L(L_ED_MD) : L(L_ED_MU);
      switch (a.btn) {
      case 0:
        det = L(L_AC_BTN_LEFT);
        break;
      case 1:
        det = L(L_AC_BTN_RIGHT);
        break;
      case 2:
        det = L(L_AC_BTN_MIDDLE);
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
      det = a.wheel > 0 ? L"\u2191" : L"\u2193";
      break;
    }
  }
  Gdiplus::Color txt = sel ? Gdiplus::Color(240, 245, 250) : GdiCol(ThText());
  Gdiplus::Color dim = sel ? Gdiplus::Color(215, 225, 235) : GdiCol(ThDim());
  Gdiplus::Color actCol = (e.isDelay || !IsKeyEv(a)) ? dim : txt;
  REAL y = (REAL)r.top, h = (REAL)(r.bottom - r.top);
  swprintf_s(b, L"%d", idx + 1);
  PutText(g, dc, b, (REAL)r.left + 4, y, 38, h, dim, 2, 8);
  PutText(g, dc, act, (REAL)r.left + 50, y, 110, h, actCol, 0, 9);
  PutText(g, dc, det, (REAL)r.left + 164, y, (REAL)(r.right - r.left - 170), h,
          txt, 0, 9, IsKeyIt(e) || e.isDelay);
}

static LRESULT CALLBACK EditorProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CREATE) {
    EdDlg *nd = (EdDlg *)((CREATESTRUCTW *)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
    nd->h = h;
    nd->lb = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED |
            LBS_NODATA | LBS_EXTENDEDSEL | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT |
            LBS_NOINTEGRALHEIGHT,
        EDT_LX + 1, EDT_LY + 1, EDT_LW - 2, EDT_LH - 2, h,
        (HMENU)(INT_PTR)EDT_LIST_ID, g_hInst, nullptr);
    SendMessageW(nd->lb, LB_SETITEMHEIGHT, 0, EDT_ROW);
    SendMessageW(nd->lb, LB_SETCOUNT, (WPARAM)nd->it.size(), 0);
    ApplyListTheme(nd->lb);
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  EdDlg *d = (EdDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!d)
    return DefWindowProcW(h, m, w, l);
  switch (m) {
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
    if (pt.y < 30 && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int nh = EdHit(pt);
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
    int b = EdHit(pt);
    if (b == 100)
      EdFinish(d, false);
    else if (b >= 0)
      EdRunButton(d, b);
    return 0;
  }
  case WM_PAINT:
    PaintBuffered(h, [&](HDC mem, Gdiplus::Graphics &g, const RECT &rc) {
      std::wstring title = L(L_ED_TITLE);
      if (!g_lastFile.empty())
        title += L" \u2014 " + Macros_Stem(g_lastFile);
      GfxChrome(g, mem, rc.right, title.c_str(), d->hover == 100 ? 2 : 0,
                CHB_SEP);
      RECT fr = {EDT_LX, EDT_LY, EDT_LX + EDT_LW, EDT_LY + EDT_LH};
      FrameRect(g, fr);
      for (int i = 0; i < EB_COUNT; i++)
        PaintBtn(g, mem, EdBtn(i), EdBtnText(i), d->hover == i, i == EB_OK,
                 EdEnabled(d, i));
      PutText(g, mem, L(L_ED_HINT), (REAL)EDT_LX, (REAL)(EDT_LY + EDT_LH + 4),
              (REAL)EDT_LW, 22.0f, GdiCol(ThDim()), 0, 8);
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

// Edits the current macro (g_events). Saves straight to its file if it has
// one, otherwise leaves it marked as unsaved.
static void RunEditor(HWND owner) {
  if (g_events.empty()) {
    StatusShow(L(L_ST_NOTHING), 2500);
    return;
  }
  EdDlg d;
  d.it = EdFromEvents(g_events);
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

// ---------------------------------------------------------------------------
// Macro library window
// ---------------------------------------------------------------------------

static const int LIB_W = 360, LIB_H = 330, LIB_LIST_ID = 201;
static const int LIB_LX = 10, LIB_LY = 64, LIB_LW = 210, LIB_LH = 256;

enum {
  LBT_LOAD,
  LBT_EDIT,
  LBT_RENAME,
  LBT_DELETE,
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
  HWND h = nullptr, lb = nullptr, owner = nullptr;
  std::vector<LibRow> rows;
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

static bool LibEnabled(LibDlg *d, int b) {
  int s = LibCur(d);
  switch (b) {
  case LBT_LOAD:
  case LBT_RENAME:
  case LBT_DELETE:
    return s >= 0 && !d->rows[(size_t)s].path.empty();
  case LBT_EDIT:
    return s >= 0;
  case LBT_SAVE:
    return !g_events.empty();
  default:
    return true;
  }
}

static void LibRebuild(LibDlg *d, const std::wstring &keepPath) {
  d->rows.clear();
  std::vector<std::wstring> files = ListMacros();
  bool curFound = false;
  for (const auto &p : files) {
    LibRow r;
    r.path = p;
    r.name = Macros_Stem(p);
    r.current = SamePath(p, g_lastFile);
    curFound = curFound || r.current;
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
    d->rows.insert(d->rows.begin(), r);
  }
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
  PutText(g, dc, row.name, (REAL)r.left + 30, (REAL)r.top,
          (REAL)(r.right - r.left - 36), (REAL)(r.bottom - r.top), txt, 0, 9,
          row.current);
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
  case WM_VKEYTOITEM: {
    int vk = LOWORD(w);
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
      RECT fr = {LIB_LX, LIB_LY, LIB_LX + LIB_LW, LIB_LY + LIB_LH};
      if (d->rows.empty()) {
        Gdiplus::SolidBrush inf(GdiCol(FieldCol(false)));
        g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        g.FillRectangle(&inf, (REAL)fr.left, (REAL)fr.top,
                        (REAL)(fr.right - fr.left), (REAL)(fr.bottom - fr.top));
        PutText(g, mem, L(L_MC_EMPTY), (REAL)fr.left, (REAL)fr.top,
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
