#include "core/config.h"
#include "core/json.h"
#include "core/update.h"
#include "engine/autoclick.h"
#include "engine/engine.h"
#include "lang/lang.h"
#include "ui/gfx.h"
#include "ui/macros.h"
#include "ui/msgbox.h"
#include "ui/statusbar.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <uxtheme.h>
#include <vector>
#include <windows.h>
#include <windowsx.h>

#ifndef CSIDL_PROGRAMS
#define CSIDL_PROGRAMS 0x0002
#endif
#ifndef CSIDL_DESKTOPDIRECTORY
#define CSIDL_DESKTOPDIRECTORY 0x0010
#endif
#ifndef CSIDL_LOCALAPPDATA
#define CSIDL_LOCALAPPDATA 0x001c
#endif

#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_UPDATE (WM_APP + 2)
#define WM_APP_PLAYDONE (WM_APP + 3)
#define WM_IN_OK (WM_APP + 20)
#define WM_IN_CANCEL (WM_APP + 21)

enum { HK_ID_REC = 1, HK_ID_PLAY = 2, HK_ID_AC = 3, HK_ID_ACPOS = 4 };

enum {
  IDM_REC = 100,
  IDM_PLAY,
  IDM_OPEN,
  IDM_SAVE,
  IDM_SP025 = 107,
  IDM_SP05 = 108,
  IDM_SP075 = 109,
  IDM_SP1 = 110,
  IDM_SP125 = 111,
  IDM_SP15 = 112,
  IDM_SP2 = 113,
  IDM_SP3 = 114,
  IDM_SP4 = 115,
  IDM_SP5 = 116,
  IDM_SP8 = 117,
  IDM_SP10 = 118,
  IDM_SP50 = 119,
  IDM_SPTURBO = 120,
  IDM_SPMAX = 121,
  IDM_SPCUSTOM = 122,
  IDM_LP1 = 125,
  IDM_LP2,
  IDM_LP3,
  IDM_LP5,
  IDM_LP10,
  IDM_LP20,
  IDM_LP50,
  IDM_LP100,
  IDM_LP500,
  IDM_LP1000,
  IDM_LPCONT,
  IDM_LPCUSTOM,
  IDM_TH0 = 140,
  IDM_TH1,
  IDM_TH2,
  IDM_TRI0 = 143,
  IDM_TRI1,
  IDM_TRI2,
  IDM_UI0 = 146,
  IDM_UI1,
  IDM_UI2,
  IDM_LENG = 150,
  IDM_LENRU,
  IDM_LENIMP,
  IDM_LENTR,
  IDM_HKR0 = 160,
  IDM_HKP0,
  IDM_HKAC,
  IDM_HKACPOS,
  IDM_HKPANIC = 165,
  IDM_PANIC_MOUSE = 168,
  IDM_REC_MOVE = 1910,
  IDM_REC_LEFT,
  IDM_REC_RIGHT,
  IDM_REC_MIDDLE,
  IDM_REC_WHEELUP,
  IDM_REC_WHEELDOWN,
  IDM_SB_SHOW = 175,
  IDM_SB_TOP,
  IDM_SB_BOTTOM,
  IDM_SB_FOLLOW,
  IDM_ASSOC_ONTY = 185,
  IDM_TOPMOST = 190,
  IDM_AUTOMINIMIZE = 191,
  IDM_COPY_CLIPBOARD = 192,
  IDM_PASTE_CLIPBOARD = 193,
  IDM_CREATE_SHORTCUT = 194,
  IDM_INSTALL = 195,
  IDM_UPDATE = 196,
  IDM_ABOUT,
  IDM_EXIT,
  IDM_RECENT_0 = 1700,
  IDM_RECENT_7 = 1707,
  IDM_RECENT_CLEAR = 1708,
  IDM_AC_TOGGLE = 1800,
  IDM_AC_HOURS,
  IDM_AC_MINS,
  IDM_AC_SECS,
  IDM_AC_MS,
  IDM_AC_BTN_L,
  IDM_AC_BTN_R,
  IDM_AC_BTN_M,
  IDM_AC_SINGLE,
  IDM_AC_DOUBLE,
  IDM_AC_UNTIL,
  IDM_AC_COUNT,
  IDM_AC_FOLLOW,
  IDM_AC_FIXED,
  IDM_AC_SETPOS,
  IDM_MACROS = 1900
};

static const int WIN_W = 250, TB_H = 30, TOOL_H = 45, STATUS_H = 24, TILES = 5,
                 TILE_W = WIN_W / TILES;
static const int WIN_H = TB_H + TOOL_H + STATUS_H;

static const wchar_t GLYPH_MACROS = 0xE8FD, GLYPH_REC = 0, GLYPH_STOP = 0xE71A,
                     GLYPH_PLAY = 0xE768;

static const int CMD_BTN[TILES] = {IDM_MACROS, IDM_REC, IDM_PLAY,
                                   IDM_AC_TOGGLE, 0};
// Tile 1 (record dot) and tile 3 (autoclick pointer) are drawn by hand.
static const wchar_t GLYPH_BTN[TILES] = {GLYPH_MACROS, GLYPH_REC, GLYPH_PLAY, 0,
                                         0xE713};

static int g_hoverB = -1, g_pressB = -1, g_hoverT = 0;
static HWND g_abHwnd = nullptr;
static COLORREF g_menuIcon = RGB(210, 210, 210);
static bool g_trayOn = false;
static int g_trayState = 0;
static NOTIFYICONDATAW g_nid = {};
static HICON g_trayIcon = nullptr;
static UINT g_msgTaskbar = 0;
static bool g_closeAfterPlay = false;

static void DoCmd(int id);
static bool InputDialog(const wchar_t *prompt, int init, int minv, int maxv,
                        int &out);
static void ShowAbout();

static RECT TileRect(int i) {
  RECT r = {i * TILE_W, TB_H, (i + 1) * TILE_W, TB_H + TOOL_H};
  return r;
}

static RECT ReSaveBtnRect() {
  RECT r = {0, TB_H + TOOL_H + 1, 23, TB_H + TOOL_H + STATUS_H - 1};
  return r;
}
std::wstring g_lastFile; // shared with ui/macros.cpp
bool g_dirty = false;

static COLORREF RecRed() { return RGB(232, 64, 54); }
static COLORREF PlayBlue() { return RGB(90, 184, 224); }

static Gdiplus::StringFormat &SfL() {
  static Gdiplus::StringFormat f;
  return f;
}

static Gdiplus::StringFormat &SfC() {
  static Gdiplus::StringFormat f;
  f.SetAlignment(Gdiplus::StringAlignmentCenter);
  f.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  return f;
}

static Gdiplus::StringFormat &SfR() {
  static Gdiplus::StringFormat f;
  f.SetAlignment(Gdiplus::StringAlignmentFar);
  f.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  return f;
}

static Gdiplus::StringFormat &SfVC() {
  static Gdiplus::StringFormat f;
  f.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  return f;
}

static void FmtMmSs(wchar_t *b, size_t n, DWORD ms);
static bool InputDialog(const wchar_t *prompt, int init, int minv, int maxv,
                        int &out);
static bool InputDialogFloat(const wchar_t *prompt, double init, double minv,
                             double maxv, double &out);

static std::wstring FileNameOnly(const std::wstring &p) {
  const wchar_t *base = wcsrchr(p.c_str(), L'\\');
  return base ? base + 1 : p;
}

static std::wstring CleanPath(std::wstring p) {
  while (!p.empty() &&
         (p.front() == L' ' || p.front() == L'\"' || p.front() == L'\''))
    p.erase(p.begin());
  while (!p.empty() &&
         (p.back() == L' ' || p.back() == L'\"' || p.back() == L'\''))
    p.pop_back();
  return p;
}

static std::wstring DockStatusText() {
  int st = g_state.load();
  wchar_t b[32];
  if (st == ST_IDLE && g_acRunning.load()) {
    FmtMmSs(b, 32, GetTickCount() - g_acStartTick);
    wchar_t b2[64];
    swprintf_s(b2, L"CLICK %s \u2022 %u", b, g_acClicks.load());
    return b2;
  }
  if (st == ST_REC) {
    FmtMmSs(b, 32, GetTickCount() - g_startTick);
    return std::wstring(L"REC ") + b;
  }
  if (st == ST_PLAY) {
    FmtMmSs(b, 32, GetTickCount() - g_startTick);
    wchar_t b2[64];
    if (g_cfg.continuous)
      swprintf_s(b2, L"PLAY %s \u221E", b);
    else
      swprintf_s(b2, L"PLAY %s (%d/%d)", b, g_playLoop.load() + 1, g_cfg.loops);
    return b2;
  }
  return Macros_CurrentLabel();
}

static std::wstring ActiveStopKeysText() {
  std::vector<std::wstring> keys;
  if (g_cfg.panicVk)
    keys.push_back(FmtHk(g_cfg.panicMods, g_cfg.panicVk));
  if (keys.empty() && g_cfg.playVk) {
    keys.push_back(FmtHk(g_cfg.playMods, g_cfg.playVk));
  }
  if (keys.empty()) {
    if (g_cfg.panicMouseMove)
      keys.push_back(L(L_PANIC_MOUSE));
    else
      keys.push_back(L(L_STOP));
  }
  std::wstring res;
  for (size_t i = 0; i < keys.size(); i++) {
    if (i > 0)
      res += L" / ";
    res += keys[i];
  }
  return res;
}

static std::wstring EngineStatusText() {
  int st = g_state.load();
  wchar_t b[32];
  if (st == ST_IDLE && g_acRunning.load()) {
    FmtMmSs(b, 32, GetTickCount() - g_acStartTick);
    wchar_t b2[64];
    swprintf_s(b2, L"CLICK %s \u2022 %u \u2022 ", b, g_acClicks.load());
    return std::wstring(b2) + L(L_STOP_BIND) + L": " +
           FmtHk(g_cfg.acMods, g_cfg.acVk);
  }
  if (st == ST_REC) {
    FmtMmSs(b, 32, GetTickCount() - g_startTick);
    std::wstring hk = FmtHk(g_cfg.recordMods, g_cfg.recordVk);
    return std::wstring(L"REC ") + b + L" \u2022 " + L(L_STOP_BIND) + L": " +
           hk;
  }
  if (st == ST_PLAY) {
    FmtMmSs(b, 32, GetTickCount() - g_startTick);
    wchar_t b2[128];
    std::wstring stopKey = ActiveStopKeysText();
    if (g_cfg.continuous)
      swprintf_s(b2, L"PLAY %s \u221E \u2022 %s: %s", b, L(L_STOP_BIND),
                 stopKey.c_str());
    else
      swprintf_s(b2, L"PLAY %s (%d/%d) \u2022 %s: %s", b, g_playLoop.load() + 1,
                 g_cfg.loops, L(L_STOP_BIND), stopKey.c_str());
    return b2;
  }
  return Macros_CurrentLabel();
}

static void Paint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(g_hwnd, &ps);
  RECT rc;
  GetClientRect(g_hwnd, &rc);
  HDC mem = CreateCompatibleDC(hdc);
  HBITMAP bm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
  HGDIOBJ old = SelectObject(mem, bm);
  Gdiplus::Graphics g(mem);
  bool acry = g_cfg.theme == THEME_ACRYLIC;
  g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
  Gdiplus::SolidBrush bg(acry ? GfxAcrylicBg() : GdiCol(ThBg()));
  g.FillRectangle(&bg, 0, 0, rc.right, rc.bottom);
  Gdiplus::Pen border(acry ? Gdiplus::Color(160, 56, 56, 56)
                           : GdiCol(ThBorder()));
  g.DrawRectangle(&border, 0, 0, rc.right - 1, rc.bottom - 1);
  int st = g_state.load();
  for (int i = 0; i < TILES; i++) {
    int hs = g_pressB == i ? 2 : g_hoverB == i ? 1 : 0;
    if (!hs)
      continue;
    Gdiplus::SolidBrush fill(acry
                                 ? Gdiplus::Color(hs == 2 ? 90 : 70, 60, 60, 60)
                                 : GdiCol(hs == 2 ? ThHover2() : ThHover()));
    RECT r = TileRect(i);
    g.FillRectangle(&fill, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                    (REAL)(r.bottom - r.top));
  }
  Gdiplus::SolidBrush gborder(acry ? Gdiplus::Color(105, 165, 165, 165)
                                   : GdiCol(ThBorder()));
  for (int i = 1; i < TILES; i++)
    g.FillRectangle(&gborder, (REAL)(i * TILE_W - 1), (REAL)TB_H, 1.0f,
                    (REAL)TOOL_H);
  int sy = TB_H + TOOL_H;
  g.FillRectangle(&gborder, 0.0f, (REAL)TB_H, (REAL)WIN_W, 1.0f);
  g.FillRectangle(&gborder, 0.0f, (REAL)sy, (REAL)WIN_W, 1.0f);
  g.FillRectangle(&gborder, (REAL)(WIN_W - 24), (REAL)sy, 1.0f, (REAL)STATUS_H);
  g.FillRectangle(&gborder, 23.0f, (REAL)sy, 1.0f, (REAL)STATUS_H);
  std::wstring status = DockStatusText();
  Gdiplus::Font f9(mem, ThFont(8));
  Gdiplus::SolidBrush txt(GdiCol(ThText()));
  Gdiplus::SolidBrush dim(GdiCol(ThDim()));
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  for (int i = 0; i < TILES; i++) {
    RECT r = TileRect(i);
    wchar_t gl = GLYPH_BTN[i];
    COLORREF col = ThText();
    if (i == 1) {
      if (st == ST_REC)
        gl = GLYPH_STOP;
      else {
        Gdiplus::SolidBrush rd(GdiCol(RecRed()));
        g.FillEllipse(&rd, (REAL)(r.left + r.right) / 2.0f - 6.5f,
                      (REAL)(r.top + r.bottom) / 2.0f - 6.5f, 13.0f, 13.0f);
        continue;
      }
    } else if (i == 2 && st == ST_PLAY)
      gl = GLYPH_STOP;
    else if (i == 3) {
      if (g_acRunning.load()) {
        gl = GLYPH_STOP;
        col = PlayBlue();
      } else {
        GfxDrawPointer(g, (REAL)(r.left + r.right) / 2.0f,
                       (REAL)(r.top + r.bottom) / 2.0f, 16.0f, col);
        continue;
      }
    }
    DrawGlyph(mem, g, (REAL)(r.left + r.right) / 2.0f,
              (REAL)(r.top + r.bottom) / 2.0f, 16, gl, col);
  }
  RECT rsb = ReSaveBtnRect();
  RECT rbtn = {1, rsb.top, 23, rsb.bottom};
  bool hasRec = !g_events.empty();
  bool saved = hasRec && !g_dirty;
  if (g_hoverT == 6 && hasRec) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    Gdiplus::SolidBrush hb(acry ? Gdiplus::Color(70, 60, 60, 60)
                                : GdiCol(ThHover()));
    g.FillRectangle(&hb, (REAL)rbtn.left, (REAL)rbtn.top,
                    (REAL)(rbtn.right - rbtn.left),
                    (REAL)(rbtn.bottom - rbtn.top));
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  }
  if (!hasRec) {
    Gdiplus::SolidBrush dd(GdiCol(ThDim()));
    g.FillEllipse(&dd, (REAL)(rbtn.left + rbtn.right) / 2.0f - 3,
                  (REAL)(rbtn.top + rbtn.bottom) / 2.0f - 3, 6.0f, 6.0f);
  } else if (saved)
    DrawGlyph(mem, g, (REAL)(rbtn.left + rbtn.right) / 2.0f,
              (REAL)(rbtn.top + rbtn.bottom) / 2.0f, 11, (wchar_t)0xE73E,
              ThText());
  else
    DrawGlyph(mem, g, (REAL)(rbtn.left + rbtn.right) / 2.0f,
              (REAL)(rbtn.top + rbtn.bottom) / 2.0f, 11, (wchar_t)0xE7BA,
              RGB(224, 176, 64));

  Gdiplus::RectF sBounds;
  g.MeasureString(status.c_str(), -1, &f9, Gdiplus::PointF(0, 0), &SfVC(),
                  &sBounds);
  REAL maxW = 118.0f;
  REAL textX = 30.0f;
  if (sBounds.Width > maxW) {
    DWORD now = GetTickCount();
    REAL overflow = sBounds.Width - maxW + 16.0f;
    DWORD period = 4000;
    REAL t = (REAL)(now % period) / (REAL)period;
    REAL offset = 0;
    if (t > 0.25f && t < 0.75f) {
      offset = ((t - 0.25f) / 0.5f) * overflow;
    } else if (t >= 0.75f) {
      offset = overflow;
    }
    textX -= offset;
  }
  Gdiplus::GraphicsState gState = g.Save();
  g.SetClip(Gdiplus::RectF(28.0f, (REAL)sy, maxW + 2.0f, (REAL)STATUS_H));
  Gdiplus::SolidBrush ttxt(GdiCol(
      (st == ST_IDLE && !g_acRunning.load())
          ? ThDim()
          : (st == ST_REC ? RecRed() : PlayBlue())));
  g.DrawString(status.c_str(), -1, &f9,
               Gdiplus::RectF(textX, (REAL)(sy + 1), sBounds.Width + 4.0f,
                              (REAL)STATUS_H),
               &SfVC(), &ttxt);
  g.Restore(gState);

  g.DrawString(APP_VERSION, -1, &f9,
               Gdiplus::RectF(WIN_W - 82, (REAL)(sy + 1), 74, (REAL)STATUS_H),
               &SfR(),
               &dim);
  GfxChrome(g, mem, rc.right, L(L_APPNAME), g_hoverT, CHB_MIN | CHB_PIN);
  BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old);
  DeleteObject(bm);
  DeleteDC(mem);
  EndPaint(g_hwnd, &ps);
}

enum {
  TIP_TILE0 = 200,
  TIP_PIN = 210,
  TIP_MIN = 211,
  TIP_CLOSE = 212,
  TIP_ICON = 213,
  TIP_UPDATE = 214,
  TIP_RESAVE = 215
};

static HWND g_tip = nullptr;
static std::wstring g_tipBuf;

static const wchar_t CURSOR_TIP_CLASS[] = L"AutoOntyCursorTip";
static HWND g_cursorTip = nullptr;
static std::wstring g_cursorTipBuf;
static DWORD g_cursorTipHideTick = 0;
static int g_cursorTipW = 0, g_cursorTipH = 0;

static std::wstring TipFor(UINT id) {
  if (id >= TIP_TILE0 && id < TIP_TILE0 + TILES) {
    int t = id - TIP_TILE0;
    int st = g_state.load();
    switch (t) {
    case 0:
      return L(L_MACROS);
    case 1:
      return (st == ST_REC ? std::wstring(L(L_STOP)) : std::wstring(L(L_REC))) +
             L"  (" + FmtHk(g_cfg.recordMods, g_cfg.recordVk) + L")";
    case 2:
      return (st == ST_PLAY ? std::wstring(L(L_STOP)) + L"  (" +
                                  ActiveStopKeysText() + L")"
                            : std::wstring(L(L_PLAY)) + L"  (" +
                                  FmtHk(g_cfg.playMods, g_cfg.playVk) + L")");
    case 3:
      return (g_acRunning.load() ? std::wstring(L(L_STOP)) + L" " +
                                       L(L_AUTOCLICK)
                                 : std::wstring(L(L_AUTOCLICK))) +
             L"  (" + FmtHk(g_cfg.acMods, g_cfg.acVk) + L")";
    default:
      return L(L_SETTINGS);
    }
  }
  switch (id) {
  case TIP_PIN:
    return L(L_TOPMOST);
  case TIP_MIN:
    return L(L_MIN);
  case TIP_CLOSE:
    return L(L_HIDE);
  case TIP_ICON:
    return std::wstring(L(L_APPNAME)) + L" \u2014 " + L(L_ABOUT);
  case TIP_RESAVE:
    if (g_events.empty())
      return L(L_ST_NOTHING);
    return g_dirty ? std::wstring(L(L_RESAVE)) + L"  (Ctrl+S)"
                   : std::wstring(L(L_SAVED));
  default:
    return std::wstring();
  }
}

static const wchar_t TIP_CLASS[] = L"AutoOntyTip";
static UINT g_tipId = 0;
static bool g_tipShown = false;
static DWORD g_tipDue = 0;
static int g_tipW = 0, g_tipH = 0;

static void TipHideNow() {
  if (!g_tip)
    return;
  g_tipShown = false;
  ShowWindow(g_tip, SW_HIDE);
}

static void TipMeasure(const std::wstring &s) {
  HDC dc = GetDC(g_tip);
  HGDIOBJ o = SelectObject(dc, ThFont(8));
  SIZE sz = {};
  GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
  SelectObject(dc, o);
  ReleaseDC(g_tip, dc);
  g_tipW = sz.cx + 14;
  g_tipH = sz.cy + 6;
}

static void TipSet(UINT id) {
  if (id == g_tipId)
    return;
  g_tipId = id;
  g_tipDue = GetTickCount() + 600;
  if (g_tipShown)
    TipHideNow();
}

static void TipRender(int x, int y) {
  int w = g_tipW, h = g_tipH;
  if (w <= 0 || h <= 0)
    return;
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ ob = SelectObject(dc, dib);
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  g.Clear(Gdiplus::Color(0, 0, 0, 0));
  bool light = g_cfg.theme == THEME_LIGHT;
  Gdiplus::RectF rr(0.5f, 0.5f, (REAL)(w - 1), (REAL)(h - 1));
  REAL d = 8.0f;
  Gdiplus::GraphicsPath path;
  path.AddArc(rr.X, rr.Y, d, d, 180, 90);
  path.AddArc(rr.GetRight() - d, rr.Y, d, d, 270, 90);
  path.AddArc(rr.GetRight() - d, rr.GetBottom() - d, d, d, 0, 90);
  path.AddArc(rr.X, rr.GetBottom() - d, d, d, 90, 90);
  path.CloseFigure();
  Gdiplus::SolidBrush bb(light ? Gdiplus::Color(255, 255, 255, 255)
                               : Gdiplus::Color(255, 40, 40, 40));
  g.FillPath(&bb, &path);
  Gdiplus::Pen pen(light ? Gdiplus::Color(255, 210, 210, 210)
                         : Gdiplus::Color(255, 68, 68, 68),
                   1.0f);
  g.DrawPath(&pen, &path);
  Gdiplus::Font f(dc, ThFont(8));
  Gdiplus::SolidBrush tb(light ? Gdiplus::Color(255, 24, 24, 24)
                               : Gdiplus::Color(255, 230, 230, 230));
  static Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericTypographic());
  sf.SetAlignment(Gdiplus::StringAlignmentCenter);
  sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  g.DrawString(g_tipBuf.c_str(), -1, &f, Gdiplus::RectF(0, 0, (REAL)w, (REAL)h),
               &sf, &tb);
  unsigned *px = (unsigned *)bits;
  for (int i = 0; i < w * h; i++) {
    unsigned p = px[i];
    unsigned a = (p >> 24) & 0xFF;
    if (a == 255 || a == 0)
      continue;
    unsigned b0 = (p & 0xFF) * a / 255;
    unsigned b1 = ((p >> 8) & 0xFF) * a / 255;
    unsigned b2 = ((p >> 16) & 0xFF) * a / 255;
    px[i] = (p & 0xFF000000u) | (b2 << 16) | (b1 << 8) | b0;
  }
  SIZE sz = {w, h};
  POINT pos = {x, y};
  POINT srco = {0, 0};
  BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_tip, screen, &pos, &sz, dc, &srco, 0, &bf, ULW_ALPHA);
  SelectObject(dc, ob);
  DeleteObject(dib);
  DeleteDC(dc);
  ReleaseDC(nullptr, screen);
}

static void CursorTipRender(int x, int y) {
  int w = g_cursorTipW, h = g_cursorTipH;
  if (w <= 0 || h <= 0 || !g_cursorTip)
    return;
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ ob = SelectObject(dc, dib);
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  g.Clear(Gdiplus::Color(0, 0, 0, 0));
  bool light = g_cfg.theme == THEME_LIGHT;
  Gdiplus::RectF rr(0.5f, 0.5f, (REAL)(w - 1), (REAL)(h - 1));
  REAL d = 8.0f;
  Gdiplus::GraphicsPath path;
  path.AddArc(rr.X, rr.Y, d, d, 180, 90);
  path.AddArc(rr.GetRight() - d, rr.Y, d, d, 270, 90);
  path.AddArc(rr.GetRight() - d, rr.GetBottom() - d, d, d, 0, 90);
  path.AddArc(rr.X, rr.GetBottom() - d, d, d, 90, 90);
  path.CloseFigure();
  Gdiplus::SolidBrush bb(light ? Gdiplus::Color(255, 255, 255, 255)
                               : Gdiplus::Color(255, 40, 40, 40));
  g.FillPath(&bb, &path);
  Gdiplus::Pen pen(light ? Gdiplus::Color(255, 210, 210, 210)
                         : Gdiplus::Color(255, 68, 68, 68),
                   1.0f);
  g.DrawPath(&pen, &path);
  Gdiplus::Font f(dc, ThFont(8));
  Gdiplus::SolidBrush tb(light ? Gdiplus::Color(255, 24, 24, 24)
                               : Gdiplus::Color(255, 230, 230, 230));
  static Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericTypographic());
  sf.SetAlignment(Gdiplus::StringAlignmentCenter);
  sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  g.DrawString(g_cursorTipBuf.c_str(), -1, &f,
               Gdiplus::RectF(0, 0, (REAL)w, (REAL)h), &sf, &tb);
  unsigned *px = (unsigned *)bits;
  for (int i = 0; i < w * h; i++) {
    unsigned p = px[i];
    unsigned a = (p >> 24) & 0xFF;
    if (a == 255 || a == 0)
      continue;
    unsigned b0 = (p & 0xFF) * a / 255;
    unsigned b1 = ((p >> 8) & 0xFF) * a / 255;
    unsigned b2 = ((p >> 16) & 0xFF) * a / 255;
    px[i] = (p & 0xFF000000u) | (b2 << 16) | (b1 << 8) | b0;
  }
  SIZE sz = {w, h};
  POINT pos = {x, y};
  POINT srco = {0, 0};
  BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_cursorTip, screen, &pos, &sz, dc, &srco, 0, &bf,
                      ULW_ALPHA);
  SelectObject(dc, ob);
  DeleteObject(dib);
  DeleteDC(dc);
  ReleaseDC(nullptr, screen);
}

static void CursorTipShow(const std::wstring &msg) {
  if (!g_cursorTip)
    return;
  g_cursorTipBuf = msg;
  HDC dc = GetDC(g_cursorTip);
  HGDIOBJ o = SelectObject(dc, ThFont(8));
  SIZE sz = {};
  GetTextExtentPoint32W(dc, msg.c_str(), (int)msg.size(), &sz);
  SelectObject(dc, o);
  ReleaseDC(g_cursorTip, dc);
  g_cursorTipW = sz.cx + 14;
  g_cursorTipH = sz.cy + 6;
  POINT pt = {};
  GetCursorPos(&pt);
  int x = pt.x + 12, y = pt.y + 22;
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  if (x + g_cursorTipW > mi.rcWork.right)
    x = pt.x - g_cursorTipW - 8;
  if (y + g_cursorTipH > mi.rcWork.bottom)
    y = pt.y - g_cursorTipH - 8;
  if (x < mi.rcWork.left)
    x = mi.rcWork.left + 4;
  if (y < mi.rcWork.top)
    y = mi.rcWork.top + 4;
  CursorTipRender(x, y);
  SetWindowPos(g_cursorTip, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  g_cursorTipHideTick = GetTickCount() + 1800;
}

static void CursorTipHide() {
  if (g_cursorTip)
    ShowWindow(g_cursorTip, SW_HIDE);
  g_cursorTipHideTick = 0;
}

static void TipPoll() {
  if (g_cursorTipHideTick && GetTickCount() > g_cursorTipHideTick) {
    CursorTipHide();
  }
  if (!g_tip)
    return;
  if (!g_tipId) {
    if (g_tipShown)
      TipHideNow();
    return;
  }
  if (GetTickCount() < g_tipDue)
    return;
  std::wstring s = TipFor(g_tipId);
  if (s.empty()) {
    if (g_tipShown)
      TipHideNow();
    return;
  }
  if (s != g_tipBuf) {
    g_tipBuf = s;
    TipMeasure(s);
  }
  POINT cp;
  GetCursorPos(&cp);
  TipRender(cp.x + 12, cp.y + 22);
  SetWindowPos(g_tip, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  if (!g_tipShown) {
    ShowWindow(g_tip, SW_SHOWNOACTIVATE);
    g_tipShown = true;
  }
}

static LRESULT CALLBACK TipProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  return DefWindowProcW(h, m, w, l);
}

static void TipInit(HWND) {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = TipProc;
  wc.hInstance = g_hInst;
  wc.lpszClassName = TIP_CLASS;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  g_tip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                              WS_EX_TRANSPARENT | WS_EX_LAYERED,
                          TIP_CLASS, L"", WS_POPUP, 0, 0, 10, 10, nullptr,
                          nullptr, g_hInst, nullptr);

  WNDCLASSW wc2 = {};
  wc2.lpfnWndProc = TipProc;
  wc2.hInstance = g_hInst;
  wc2.lpszClassName = CURSOR_TIP_CLASS;
  wc2.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&wc2);
  g_cursorTip =
      CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                          WS_EX_TRANSPARENT | WS_EX_LAYERED,
                      CURSOR_TIP_CLASS, L"", WS_POPUP, 0, 0, 10, 10, nullptr,
                      nullptr, g_hInst, nullptr);
}

static void AddMI(HMENU m, UINT id, const wchar_t *txt, wchar_t glyph) {
  AppendMenuW(m, MF_STRING, id, txt);
  GfxMenuGlyph(m, id, glyph, g_menuIcon);
}

static void AddCheck(HMENU m, UINT id, const wchar_t *txt, bool checked) {
  AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0), id, txt);
  if (checked) {
    GfxMenuGlyph(m, id, (wchar_t)0xE73E, g_menuIcon);
  }
}

static void AddCheckWithIcon(HMENU m, UINT id, const wchar_t *txt, bool checked,
                             wchar_t uncheckedGlyph) {
  AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0), id, txt);
  GfxMenuGlyph(m, id, checked ? (wchar_t)0xE73E : uncheckedGlyph, g_menuIcon);
}

static COLORREF MenuIconColor() {
  return GfxMenuTheme() == 2 ? RGB(210, 210, 210) : RGB(60, 60, 60);
}

static void IconLast(HMENU m, wchar_t glyph) {
  MENUITEMINFOW mii = {sizeof(mii)};
  mii.fMask = MIIM_BITMAP;
  mii.hbmpItem = GfxGlyphBitmap(glyph, g_menuIcon);
  SetMenuItemInfoW(m, GetMenuItemCount(m) - 1, TRUE, &mii);
}

static bool CopyMacroToClipboard() {
  std::string s;
  if (!Engine_ToJson(s))
    return false;
  std::wstring ws = json::ToWide(s);
  if (!OpenClipboard(g_hwnd))
    return false;
  EmptyClipboard();
  size_t bytes = (ws.size() + 1) * sizeof(wchar_t);
  HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!hg) {
    CloseClipboard();
    return false;
  }
  memcpy(GlobalLock(hg), ws.c_str(), bytes);
  GlobalUnlock(hg);
  SetClipboardData(CF_UNICODETEXT, hg);
  CloseClipboard();
  return true;
}

static bool PasteMacroFromClipboard() {
  if (!OpenClipboard(g_hwnd))
    return false;
  HANDLE hg = GetClipboardData(CF_UNICODETEXT);
  if (!hg) {
    CloseClipboard();
    return false;
  }
  const wchar_t *wstr = (const wchar_t *)GlobalLock(hg);
  if (!wstr) {
    CloseClipboard();
    return false;
  }
  std::string jsonStr = json::ToUtf8(wstr);
  GlobalUnlock(hg);
  CloseClipboard();
  if (!Engine_FromJson(jsonStr))
    return false;
  g_lastFile.clear();
  g_dirty = true;
  ConfigSave();
  return true;
}

static bool CreateDesktopShortcut(const std::wstring &macroPath) {
  if (macroPath.empty())
    return false;
  CoInitialize(nullptr);
  IShellLinkW *psl = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void **)&psl);
  if (SUCCEEDED(hr)) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    psl->SetPath(exePath);
    std::wstring args = L"\"" + macroPath + L"\"";
    psl->SetArguments(args.c_str());
    wchar_t dir[MAX_PATH];
    lstrcpynW(dir, exePath, MAX_PATH);
    wchar_t *lastBs = wcsrchr(dir, L'\\');
    if (lastBs)
      *lastBs = 0;
    psl->SetWorkingDirectory(dir);
    psl->SetIconLocation(exePath, 0);

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
    if (SUCCEEDED(hr)) {
      wchar_t desktop[MAX_PATH];
      SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
      std::wstring baseName = FileNameOnly(macroPath);
      size_t dot = baseName.find_last_of(L'.');
      if (dot != std::wstring::npos)
        baseName = baseName.substr(0, dot);
      std::wstring lnkPath =
          std::wstring(desktop) + L"\\" + baseName + L" (AutoOnty).lnk";
      hr = ppf->Save(lnkPath.c_str(), TRUE);
      ppf->Release();
    }
    psl->Release();
  }
  CoUninitialize();
  return SUCCEEDED(hr);
}

static bool InstallToLocalApp() {
  wchar_t localApp[MAX_PATH];
  if (FAILED(
          SHGetFolderPathW(nullptr, CSIDL_LOCALAPPDATA, nullptr, 0, localApp)))
    return false;
  std::wstring targetDir = std::wstring(localApp) + L"\\AutoOnty";
  CreateDirectoryW(targetDir.c_str(), nullptr);
  std::wstring targetExe = targetDir + L"\\AutoOnty.exe";

  wchar_t curExe[MAX_PATH];
  GetModuleFileNameW(nullptr, curExe, MAX_PATH);

  if (_wcsicmp(curExe, targetExe.c_str()) != 0) {
    if (!CopyFileW(curExe, targetExe.c_str(), FALSE))
      return false;
  }

  RegisterFileAssociation(true);

  CoInitialize(nullptr);

  wchar_t programs[MAX_PATH];
  if (SUCCEEDED(
          SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programs))) {
    IShellLinkW *psl1 = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                   (void **)&psl1))) {
      psl1->SetPath(targetExe.c_str());
      psl1->SetWorkingDirectory(targetDir.c_str());
      psl1->SetIconLocation(targetExe.c_str(), 0);
      IPersistFile *ppf1 = nullptr;
      if (SUCCEEDED(psl1->QueryInterface(IID_IPersistFile, (void **)&ppf1))) {
        std::wstring startLnk = std::wstring(programs) + L"\\AutoOnty.lnk";
        ppf1->Save(startLnk.c_str(), TRUE);
        ppf1->Release();
      }
      psl1->Release();
    }
  }

  wchar_t desktop[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0,
                                 desktop))) {
    IShellLinkW *psl2 = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                   (void **)&psl2))) {
      psl2->SetPath(targetExe.c_str());
      psl2->SetWorkingDirectory(targetDir.c_str());
      psl2->SetIconLocation(targetExe.c_str(), 0);
      IPersistFile *ppf2 = nullptr;
      if (SUCCEEDED(psl2->QueryInterface(IID_IPersistFile, (void **)&ppf2))) {
        std::wstring deskLnk = std::wstring(desktop) + L"\\AutoOnty.lnk";
        ppf2->Save(deskLnk.c_str(), TRUE);
        ppf2->Release();
      }
      psl2->Release();
    }
  }

  CoUninitialize();
  return true;
}

// Menu -> "Autoclick settings": mirrors the layout of classic auto clickers
// (click interval, click options, click repeat) plus the cursor/fixed mode.
static void BuildAutoclickMenu(HMENU root) {
  wchar_t t[128];
  HMENU ac = CreatePopupMenu();

  HMENU iv = CreatePopupMenu();
  swprintf_s(t, L"%s: %d...", L(L_AC_HOURS), g_cfg.acHours);
  AppendMenuW(iv, MF_STRING, IDM_AC_HOURS, t);
  swprintf_s(t, L"%s: %d...", L(L_AC_MINS), g_cfg.acMins);
  AppendMenuW(iv, MF_STRING, IDM_AC_MINS, t);
  swprintf_s(t, L"%s: %d...", L(L_AC_SECS), g_cfg.acSecs);
  AppendMenuW(iv, MF_STRING, IDM_AC_SECS, t);
  swprintf_s(t, L"%s: %d...", L(L_AC_MS), g_cfg.acMs);
  AppendMenuW(iv, MF_STRING, IDM_AC_MS, t);
  AppendMenuW(ac, MF_POPUP, (UINT_PTR)iv, L(L_AC_INTERVAL));
  IconLast(ac, 0xE916);

  HMENU bt = CreatePopupMenu();
  AddCheck(bt, IDM_AC_BTN_L, L(L_AC_BTN_LEFT), g_cfg.acButton == 0);
  AddCheck(bt, IDM_AC_BTN_M, L(L_AC_BTN_MIDDLE), g_cfg.acButton == 2);
  AddCheck(bt, IDM_AC_BTN_R, L(L_AC_BTN_RIGHT), g_cfg.acButton == 1);
  AppendMenuW(ac, MF_POPUP, (UINT_PTR)bt, L(L_AC_BUTTON));

  HMENU ty = CreatePopupMenu();
  AddCheck(ty, IDM_AC_SINGLE, L(L_AC_SINGLE), g_cfg.acDouble == 0);
  AddCheck(ty, IDM_AC_DOUBLE, L(L_AC_DOUBLE), g_cfg.acDouble != 0);
  AppendMenuW(ac, MF_POPUP, (UINT_PTR)ty, L(L_AC_TYPE));

  HMENU rp = CreatePopupMenu();
  AddCheck(rp, IDM_AC_UNTIL, L(L_AC_REPEAT_UNTIL), g_cfg.acRepeatUntilStop != 0);
  swprintf_s(t, L"%s (%d)...", L(L_AC_REPEAT_COUNT), g_cfg.acRepeatCount);
  AddCheck(rp, IDM_AC_COUNT, t, g_cfg.acRepeatUntilStop == 0);
  AppendMenuW(ac, MF_POPUP, (UINT_PTR)rp, L(L_AC_REPEAT));
  AppendMenuW(ac, MF_SEPARATOR, 0, nullptr);

  HMENU ps = CreatePopupMenu();
  AddCheck(ps, IDM_AC_FOLLOW, L(L_AC_FOLLOW), g_cfg.acMode == 0);
  AddCheck(ps, IDM_AC_FIXED, L(L_AC_FIXED), g_cfg.acMode == 1);
  AppendMenuW(ps, MF_SEPARATOR, 0, nullptr);
  if (g_cfg.acHasPos)
    swprintf_s(t, L"%s: %d, %d", L(L_AC_SAVED_POS), g_cfg.acX, g_cfg.acY);
  else
    swprintf_s(t, L"%s: %s", L(L_AC_SAVED_POS), L(L_AC_NOT_SET));
  AppendMenuW(ps, MF_STRING | MF_GRAYED, 0, t);
  AppendMenuW(ac, MF_POPUP, (UINT_PTR)ps, L(L_AC_POSITION));

  AppendMenuW(root, MF_POPUP, (UINT_PTR)ac, L(L_AC_SETTINGS));
  MENUITEMINFOW mii = {sizeof(mii)};
  mii.fMask = MIIM_BITMAP;
  mii.hbmpItem = GfxPointerBitmap(g_menuIcon);
  SetMenuItemInfoW(root, GetMenuItemCount(root) - 1, TRUE, &mii);
}

static void BuildMenu(HMENU root) {
  int st = g_state.load();

  std::wstring recHk = FmtHk(g_cfg.recordMods, g_cfg.recordVk);
  std::wstring playHk = (st == ST_PLAY) ? ActiveStopKeysText()
                                        : FmtHk(g_cfg.playMods, g_cfg.playVk);
  std::wstring recText = std::wstring(st == ST_REC ? L(L_STOP) : L(L_REC)) +
                         (recHk.empty() ? L"" : (L"\t" + recHk));
  std::wstring playText = std::wstring(st == ST_PLAY ? L(L_STOP) : L(L_PLAY)) +
                          (playHk.empty() ? L"" : (L"\t" + playHk));

  AppendMenuW(root, MF_STRING, IDM_REC, recText.c_str());
  GfxMenuIcon(root, IDM_REC,
              st == ST_REC ? GfxGlyphBitmap(GLYPH_STOP, g_menuIcon)
                           : GfxDotBitmap(RecRed()));
  AppendMenuW(root, MF_STRING, IDM_PLAY, playText.c_str());
  GfxMenuGlyph(root, IDM_PLAY, st == ST_PLAY ? GLYPH_STOP : GLYPH_PLAY,
               g_menuIcon);
  {
    bool acOn = g_acRunning.load();
    std::wstring acHk = FmtHk(g_cfg.acMods, g_cfg.acVk);
    std::wstring acText =
        (acOn ? std::wstring(L(L_STOP)) + L" " + L(L_AUTOCLICK)
              : std::wstring(L(L_AUTOCLICK))) +
        (acHk.empty() ? L"" : (L"\t" + acHk));
    AppendMenuW(root, MF_STRING, IDM_AC_TOGGLE, acText.c_str());
    GfxMenuIcon(root, IDM_AC_TOGGLE,
                acOn ? GfxGlyphBitmap(GLYPH_STOP, g_menuIcon)
                     : GfxPointerBitmap(g_menuIcon));
  }
  AppendMenuW(root, MF_SEPARATOR, 0, nullptr);

  AddMI(root, IDM_MACROS, L(L_MACROS), 0xE8FD);
  std::wstring openText = std::wstring(L(L_OPEN)) + L"\tCtrl+O";
  std::wstring saveText = std::wstring(L(L_SAVE)) + L"\tCtrl+S";
  AddMI(root, IDM_OPEN, openText.c_str(), 0xE8E5);
  AddMI(root, IDM_SAVE, saveText.c_str(), 0xE74E);

  HMENU clipM = CreatePopupMenu();
  std::wstring copyText = std::wstring(L(L_COPY_CLIPBOARD)) + L"\tCtrl+C";
  std::wstring pasteText = std::wstring(L(L_PASTE_CLIPBOARD)) + L"\tCtrl+V";
  AddMI(clipM, IDM_COPY_CLIPBOARD, copyText.c_str(), 0xE8C8);
  AddMI(clipM, IDM_PASTE_CLIPBOARD, pasteText.c_str(), 0xE77F);
  AppendMenuW(clipM, MF_SEPARATOR, 0, nullptr);
  if (!g_lastFile.empty())
    AddMI(clipM, IDM_CREATE_SHORTCUT, L(L_CREATE_SHORTCUT), 0xE71B);
  else
    AppendMenuW(clipM, MF_STRING | MF_GRAYED, IDM_CREATE_SHORTCUT,
                L(L_CREATE_SHORTCUT));
  AppendMenuW(root, MF_POPUP, (UINT_PTR)clipM, L(L_CLIPBOARD));
  IconLast(root, 0xE77F);
  AppendMenuW(root, MF_SEPARATOR, 0, nullptr);

  HMENU sp = CreatePopupMenu();
  AddCheck(sp, IDM_SP05, L(L_SPEED05), g_cfg.speed == 0);
  AddCheck(sp, IDM_SP1, L(L_SPEED1), g_cfg.speed == 1);
  AddCheck(sp, IDM_SP2, L(L_SPEED2), g_cfg.speed == 2);
  AddCheck(sp, IDM_SP5, L(L_SPEED5), g_cfg.speed == 5);
  AddCheck(sp, IDM_SP10, L(L_SPEED10), g_cfg.speed == 10);
  AddCheck(sp, IDM_SPTURBO, L(L_SPEEDTURBO), g_cfg.speed == 100);
  AddCheck(sp, IDM_SPMAX, L(L_SPEED_MAX), g_cfg.speed == -5);
  wchar_t spCustStr[64];
  swprintf_s(spCustStr, L"%s (%.4gx)...", L(L_SPEEDCUSTOM), g_cfg.speedCustom);
  AddCheck(sp, IDM_SPCUSTOM, spCustStr, g_cfg.speed == 999);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)sp, L(L_SPEED));
  IconLast(root, 0xE916);

  HMENU lp = CreatePopupMenu();
  bool customLoop = !g_cfg.continuous && g_cfg.loops != 1 && g_cfg.loops != 2 &&
                    g_cfg.loops != 3 && g_cfg.loops != 5 && g_cfg.loops != 10 &&
                    g_cfg.loops != 100;
  AddCheck(lp, IDM_LP1, L(L_LOOPS1), !g_cfg.continuous && g_cfg.loops == 1);
  AddCheck(lp, IDM_LP2, L(L_LOOPS2), !g_cfg.continuous && g_cfg.loops == 2);
  AddCheck(lp, IDM_LP3, L(L_LOOPS3), !g_cfg.continuous && g_cfg.loops == 3);
  AddCheck(lp, IDM_LP5, L(L_LOOPS5), !g_cfg.continuous && g_cfg.loops == 5);
  AddCheck(lp, IDM_LP10, L(L_LOOPS10), !g_cfg.continuous && g_cfg.loops == 10);
  AddCheck(lp, IDM_LP100, L(L_LOOPS100),
           !g_cfg.continuous && g_cfg.loops == 100);
  AddCheck(lp, IDM_LPCONT, L(L_CONT), g_cfg.continuous != 0);
  wchar_t lpCustStr[64];
  swprintf_s(lpCustStr, L"%s (%d)...", L(L_LOOPSCUSTOM), g_cfg.loops);
  AddCheck(lp, IDM_LPCUSTOM, lpCustStr, customLoop);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)lp, L(L_LOOPS));
  IconLast(root, 0xE74D);
  BuildAutoclickMenu(root);
  AppendMenuW(root, MF_SEPARATOR, 0, nullptr);

  AddCheckWithIcon(root, IDM_TOPMOST, L(L_TOPMOST), g_cfg.topmost != 0, 0xE718);

  HMENU hk = CreatePopupMenu();
  std::wstring recItem = std::wstring(L(L_HK_REC)) + L":   " +
                         FmtHk(g_cfg.recordMods, g_cfg.recordVk);
  std::wstring playItem = std::wstring(L(L_HK_PLAY)) + L":   " +
                          FmtHk(g_cfg.playMods, g_cfg.playVk);
  AddMI(hk, IDM_HKR0, recItem.c_str(), 0xE70F);
  AddMI(hk, IDM_HKP0, playItem.c_str(), 0xE70F);
  std::wstring acItem = std::wstring(L(L_AUTOCLICK)) + L":   " +
                        FmtHk(g_cfg.acMods, g_cfg.acVk);
  std::wstring acPosItem = std::wstring(L(L_HK_AC_POS)) + L":   " +
                           FmtHk(g_cfg.acPosMods, g_cfg.acPosVk);
  AddMI(hk, IDM_HKAC, acItem.c_str(), 0xE70F);
  AddMI(hk, IDM_HKACPOS, acPosItem.c_str(), 0xE70F);
  AppendMenuW(hk, MF_SEPARATOR, 0, nullptr);
  std::wstring panicItem = std::wstring(L(L_PANIC_KEYS)) + L":   " +
                           FmtHk(g_cfg.panicMods, g_cfg.panicVk);
  AddMI(hk, IDM_HKPANIC, panicItem.c_str(), 0xE70F);
  AddCheck(hk, IDM_PANIC_MOUSE, L(L_PANIC_MOUSE), g_cfg.panicMouseMove != 0);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)hk, L(L_HOTKEYS));
  IconLast(root, 0xE765);

  HMENU rc = CreatePopupMenu();
  AddCheck(rc, IDM_REC_MOVE, L(L_REC_MOVE), g_cfg.recMouseMove != 0);
  AddCheck(rc, IDM_REC_LEFT, L(L_REC_LEFT), g_cfg.recLeft != 0);
  AddCheck(rc, IDM_REC_RIGHT, L(L_REC_RIGHT), g_cfg.recRight != 0);
  AddCheck(rc, IDM_REC_MIDDLE, L(L_REC_MIDDLE), g_cfg.recMiddle != 0);
  AddCheck(rc, IDM_REC_WHEELUP, L(L_REC_WHEELUP), g_cfg.recWheelUp != 0);
  AddCheck(rc, IDM_REC_WHEELDOWN, L(L_REC_WHEELDOWN), g_cfg.recWheelDown != 0);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)rc, L(L_REC_INPUTS));
  IconLast(root, 0xE7C8);

  HMENU th = CreatePopupMenu();
  AppendMenuW(th, MF_STRING | MF_GRAYED, 0, L(L_INTERFACE_THEME));
  AddCheck(th, IDM_TH0, L(L_TH_DARK), g_cfg.theme == 0);
  AddCheck(th, IDM_TH1, L(L_TH_LIGHT), g_cfg.theme == 1);
  AddCheck(th, IDM_TH2, L(L_TH_ACRYLIC), g_cfg.theme == 2);
  AppendMenuW(th, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(th, MF_STRING | MF_GRAYED, 0, L(L_TRAY_ICON));
  AddCheck(th, IDM_TRI0, L(L_TRAY_AUTO), g_cfg.trayIcon == 0);
  AddCheck(th, IDM_TRI1, L(L_TH_DARK), g_cfg.trayIcon == 1);
  AddCheck(th, IDM_TRI2, L(L_TH_LIGHT), g_cfg.trayIcon == 2);
  AppendMenuW(th, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(th, MF_STRING | MF_GRAYED, 0, L(L_UI_ICON));
  AddCheck(th, IDM_UI0, L(L_TRAY_AUTO), g_cfg.uiIcon == 0);
  AddCheck(th, IDM_UI1, L(L_TH_DARK), g_cfg.uiIcon == 1);
  AddCheck(th, IDM_UI2, L(L_TH_LIGHT), g_cfg.uiIcon == 2);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)th, L(L_THEME));
  IconLast(root, 0xE727);

  HMENU sb = CreatePopupMenu();
  AddCheck(sb, IDM_SB_SHOW, L(L_SB_ENABLE), g_cfg.statusbarShow != 0);
  AppendMenuW(sb, MF_SEPARATOR, 0, nullptr);
  AddCheck(sb, IDM_SB_TOP, L(L_SB_TOP), g_cfg.statusbarTop != 0);
  AddCheck(sb, IDM_SB_BOTTOM, L(L_SB_BOTTOM), g_cfg.statusbarTop == 0);
  AppendMenuW(sb, MF_SEPARATOR, 0, nullptr);
  AddCheck(sb, IDM_SB_FOLLOW, L(L_SB_FOLLOW_MONITOR),
           g_cfg.statusbarFollowCursor != 0);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)sb, L(L_STATUSBAR));
  IconLast(root, 0xEC42);

  AddCheckWithIcon(root, IDM_AUTOMINIMIZE, L(L_AUTOMINIMIZE),
                   g_cfg.autoMinimize != 0, 0xE921);
  bool isAssoc = IsFileAssociationRegistered();
  AddCheckWithIcon(root, IDM_ASSOC_ONTY, L(L_ASSOC_ONTY), isAssoc, 0xE71B);

  HMENU lg = CreatePopupMenu();
  AddCheck(lg, IDM_LENG, L(L_LANG_EN), g_cfg.lang == 1);
  AddCheck(lg, IDM_LENRU, L(L_LANG_RU), g_cfg.lang == 2);
  AppendMenuW(lg, MF_SEPARATOR, 0, nullptr);
  AddMI(lg, IDM_LENIMP, L(L_LANG_IMPORT), 0xE896);
  AddMI(lg, IDM_LENTR, L(L_LANG_TRANSLATE), 0xE8A1);
  AppendMenuW(root, MF_POPUP, (UINT_PTR)lg, L(L_LANG));
  IconLast(root, 0xE774);
  AppendMenuW(root, MF_SEPARATOR, 0, nullptr);

  {
    const wchar_t *upTxt = L(L_UPDATE);
    switch (Update_State()) {
    case UR_FOUND:
      upTxt = L(L_UPDATE_AVAIL);
      break;
    case UR_CHECKING:
      upTxt = L(L_UPDATE);
      break;
    case UR_LATEST:
      upTxt = L(L_ST_UPTODATE);
      break;
    case UR_FAIL:
      upTxt = L(L_ST_UPDATEFAIL);
      break;
    }
    AddMI(root, IDM_UPDATE, upTxt, 0xE777);
  }
  AddMI(root, IDM_ABOUT, L(L_ABOUT), 0xE946);
  AddMI(root, IDM_EXIT, L(L_EXIT), 0xE711);
}

static void TrackMenu() {
  GfxMenuTheme();
  g_menuIcon = MenuIconColor();
  HMENU m = CreatePopupMenu();
  BuildMenu(m);
  POINT p;
  GetCursorPos(&p);
  SetForegroundWindow(g_hwnd);
  TrackPopupMenu(m, TPM_RIGHTBUTTON, p.x, p.y, 0, g_hwnd, nullptr);
  DestroyMenu(m);
}

static HICON TrayIconMake() {
  bool light = g_cfg.theme == THEME_LIGHT;
  if (g_cfg.trayIcon == 1)
    light = false;
  else if (g_cfg.trayIcon == 2)
    light = true;
  int id = g_trayState == ST_REC    ? (light ? 5 : 3)
           : g_trayState == ST_PLAY ? (light ? 7 : 6)
                                    : (light ? 4 : 2);
  return LoadAppIconId(id, GetSystemMetrics(SM_CXSMICON));
}

static void TrayAdd() {
  g_nid = {};
  g_nid.cbSize = sizeof(g_nid);
  g_nid.hWnd = g_hwnd;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  g_nid.uCallbackMessage = WM_APP_TRAY;
  lstrcpynW(g_nid.szTip, L"AutoOnty", 64);
  if (g_trayIcon)
    DestroyIcon(g_trayIcon);
  g_trayIcon = TrayIconMake();
  g_nid.hIcon = g_trayIcon;
  g_trayOn = Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
}

static void TrayRemove() {
  if (g_trayOn)
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
  g_trayOn = false;
}

static void TrayIconRefresh() {
  if (g_trayIcon)
    DestroyIcon(g_trayIcon);
  g_trayIcon = TrayIconMake();
  g_nid.hIcon = g_trayIcon;
  if (g_trayOn)
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void TraySync() {
  int st = g_state.load();
  if (st != g_trayState) {
    g_trayState = st;
    TrayIconRefresh();
  }
}

static void TrayBalloon(const wchar_t *title, const wchar_t *text) {
  if (!g_trayOn)
    return;
  UINT f = g_nid.uFlags;
  g_nid.uFlags = NIF_INFO;
  lstrcpynW(g_nid.szInfoTitle, title, 64);
  lstrcpynW(g_nid.szInfo, text, 256);
  g_nid.dwInfoFlags = NIIF_INFO;
  Shell_NotifyIconW(NIM_MODIFY, &g_nid);
  g_nid.uFlags = f;
}

static void FmtMmSs(wchar_t *b, size_t n, DWORD ms) {
  swprintf_s(b, n, L"%02d:%02d", (int)(ms / 60000), (int)(ms / 1000 % 60));
}

static void StatusTick() {
  std::wstring s = EngineStatusText();
  int st = g_state.load();
  if (st == ST_REC || st == ST_PLAY || g_acRunning.load()) {
    if (StatusIsPersistent())
      StatusUpdate(s.c_str());
    else if (!StatusVisible())
      StatusPersistent(s.c_str());
    InvalidateRect(g_hwnd, nullptr, FALSE);
  } else {
    if (StatusIsPersistent() && StatusVisible())
      StatusHide();
    InvalidateRect(g_hwnd, nullptr, FALSE);
  }
  int trayState = st;
  if (trayState != g_trayState) {
    g_trayState = trayState;
    TrayIconRefresh();
  }
}

static UINT HkSysMod(int m) {
  return ((m & HK_CTRL) ? MOD_CONTROL : 0) | ((m & HK_SHIFT) ? MOD_SHIFT : 0) |
         ((m & HK_ALT) ? MOD_ALT : 0) | ((m & HK_WIN) ? MOD_WIN : 0) |
         MOD_NOREPEAT;
}

static void HotkeyRegisterAll(HWND hwnd) {
  const struct {
    int id, mods, vk;
  } hk[] = {{HK_ID_REC, g_cfg.recordMods, g_cfg.recordVk},
            {HK_ID_PLAY, g_cfg.playMods, g_cfg.playVk},
            {HK_ID_AC, g_cfg.acMods, g_cfg.acVk},
            {HK_ID_ACPOS, g_cfg.acPosMods, g_cfg.acPosVk}};
  for (const auto &h : hk)
    UnregisterHotKey(hwnd, h.id);
  for (const auto &h : hk) {
    if (!h.vk)
      continue;
    UINT mod = HkSysMod(h.mods);
    if (!RegisterHotKey(hwnd, h.id, mod, h.vk)) {
      RegisterHotKey(hwnd, h.id, mod & ~MOD_NOREPEAT, h.vk);
    }
  }
}

static void DoCmd(int id) {
  // While the macro library/editor is open, don't let hotkeys or the tray menu
  // record, play or swap the macro underneath it.
  if (Macros_IsModalOpen() &&
      (id == IDM_REC || id == IDM_PLAY || id == IDM_AC_TOGGLE ||
       id == IDM_OPEN || id == IDM_SAVE || id == IDM_MACROS ||
       id == IDM_PASTE_CLIPBOARD || id == IDM_AC_SETPOS))
    return;
  switch (id) {
  case IDM_MACROS:
    if (!IsWindowVisible(g_hwnd))
      ShowWindow(g_hwnd, SW_SHOW);
    Macros_ShowLibrary(g_hwnd);
    break;
  case IDM_REC:
    if (g_state.load() == ST_REC) {
      Engine_RecordStop();
      g_lastFile.clear();
      g_dirty = !g_events.empty();
      if (g_cfg.autoMinimize) {
        ShowWindow(g_hwnd, SW_SHOW);
        SetForegroundWindow(g_hwnd);
      }
    } else if (g_state.load() == ST_PLAY)
      Engine_PlayStop();
    else {
      Autoclick_StopAndWait(1000);
      if (Engine_Record()) {
        g_lastFile.clear();
        g_dirty = true;
        if (g_cfg.autoMinimize)
          ShowWindow(g_hwnd, SW_HIDE);
      }
    }
    break;
  case IDM_PLAY:
    if (g_state.load() == ST_PLAY)
      Engine_PlayStop();
    else if (g_state.load() == ST_REC)
      Engine_RecordStop();
    else {
      Autoclick_StopAndWait(1000);
      if (Engine_Play()) {
        if (g_cfg.autoMinimize)
          ShowWindow(g_hwnd, SW_HIDE);
      }
    }
    break;
  case IDM_OPEN: {
    if (g_state.load() != ST_IDLE)
      break;
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    o.lpstrFilter =
        L"AutoOnty (*.onty;*.json)\0*.onty;*.json\0All files\0*.*\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    std::wstring openDir = Macros_Dir();
    o.lpstrInitialDir = openDir.c_str();
    o.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&o)) {
      if (Engine_Load(path)) {
        g_lastFile = path;
        g_dirty = false;
        RecentAdd(path);
        StatusShow(L(L_ST_LOADED), 2500);
      } else {
        StatusShow(L(L_ST_INVALID), 2500);
        UiMessage(g_hwnd, L(L_ST_INVALID), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
      }
    }
    break;
  }
  case IDM_SAVE:
    if (g_state.load() != ST_IDLE)
      break;
    Macros_SaveCurrent(g_hwnd);
    break;
  case IDM_COPY_CLIPBOARD:
    if (CopyMacroToClipboard())
      StatusShow(L(L_CLIPBOARD_COPIED), 2500);
    else
      StatusShow(L(L_ST_NOTHING), 2500);
    break;
  case IDM_PASTE_CLIPBOARD:
    if (PasteMacroFromClipboard())
      StatusShow(L(L_CLIPBOARD_LOADED), 2500);
    else
      StatusShow(L(L_CLIPBOARD_EMPTY), 2500);
    break;
  case IDM_CREATE_SHORTCUT:
    if (!g_lastFile.empty()) {
      if (CreateDesktopShortcut(g_lastFile))
        StatusShow(L(L_SHORTCUT_SUCCESS), 2500);
      else
        StatusShow(L(L_SHORTCUT_FAIL), 2500);
    } else {
      StatusShow(L(L_ST_NOTHING), 2500);
    }
    break;
  case IDM_SP025:
    g_cfg.speed = -1;
    ConfigSave();
    break;
  case IDM_SP05:
    g_cfg.speed = 0;
    ConfigSave();
    break;
  case IDM_SP075:
    g_cfg.speed = -2;
    ConfigSave();
    break;
  case IDM_SP1:
    g_cfg.speed = 1;
    ConfigSave();
    break;
  case IDM_SP125:
    g_cfg.speed = -3;
    ConfigSave();
    break;
  case IDM_SP15:
    g_cfg.speed = -4;
    ConfigSave();
    break;
  case IDM_SP2:
    g_cfg.speed = 2;
    ConfigSave();
    break;
  case IDM_SP3:
    g_cfg.speed = 3;
    ConfigSave();
    break;
  case IDM_SP4:
    g_cfg.speed = 4;
    ConfigSave();
    break;
  case IDM_SP5:
    g_cfg.speed = 5;
    ConfigSave();
    break;
  case IDM_SP8:
    g_cfg.speed = 8;
    ConfigSave();
    break;
  case IDM_SP10:
    g_cfg.speed = 10;
    ConfigSave();
    break;
  case IDM_SP50:
    g_cfg.speed = 50;
    ConfigSave();
    break;
  case IDM_SPTURBO:
    g_cfg.speed = 100;
    ConfigSave();
    break;
  case IDM_SPMAX:
    g_cfg.speed = -5;
    ConfigSave();
    break;
  case IDM_SPCUSTOM: {
    double v = g_cfg.speedCustom;
    if (InputDialogFloat(L(L_Q_SPEED), g_cfg.speedCustom, 0.01, 1000.0, v)) {
      g_cfg.speedCustom = v;
      g_cfg.speed = 999;
      ConfigSave();
    }
    break;
  }
  case IDM_LP1:
    g_cfg.loops = 1;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP2:
    g_cfg.loops = 2;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP3:
    g_cfg.loops = 3;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP5:
    g_cfg.loops = 5;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP10:
    g_cfg.loops = 10;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP20:
    g_cfg.loops = 20;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP50:
    g_cfg.loops = 50;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP100:
    g_cfg.loops = 100;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP500:
    g_cfg.loops = 500;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LP1000:
    g_cfg.loops = 1000;
    g_cfg.continuous = 0;
    ConfigSave();
    break;
  case IDM_LPCONT:
    g_cfg.continuous = !g_cfg.continuous;
    ConfigSave();
    break;
  case IDM_LPCUSTOM: {
    int v = g_cfg.loops;
    if (InputDialog(L(L_Q_LOOPS), g_cfg.loops, 1, 99999, v)) {
      g_cfg.loops = v;
      g_cfg.continuous = 0;
      ConfigSave();
    }
    break;
  }
  case IDM_REC_MOVE:
    g_cfg.recMouseMove = !g_cfg.recMouseMove;
    ConfigSave();
    break;
  case IDM_REC_LEFT:
    g_cfg.recLeft = !g_cfg.recLeft;
    ConfigSave();
    break;
  case IDM_REC_RIGHT:
    g_cfg.recRight = !g_cfg.recRight;
    ConfigSave();
    break;
  case IDM_REC_MIDDLE:
    g_cfg.recMiddle = !g_cfg.recMiddle;
    ConfigSave();
    break;
  case IDM_REC_WHEELUP:
    g_cfg.recWheelUp = !g_cfg.recWheelUp;
    ConfigSave();
    break;
  case IDM_REC_WHEELDOWN:
    g_cfg.recWheelDown = !g_cfg.recWheelDown;
    ConfigSave();
    break;
  case IDM_PANIC_MOUSE:
    g_cfg.panicMouseMove = !g_cfg.panicMouseMove;
    ConfigSave();
    break;
  case IDM_SB_SHOW:
    g_cfg.statusbarShow = !g_cfg.statusbarShow;
    if (!g_cfg.statusbarShow)
      StatusHide();
    ConfigSave();
    break;
  case IDM_SB_TOP:
    g_cfg.statusbarTop = 1;
    ConfigSave();
    break;
  case IDM_SB_BOTTOM:
    g_cfg.statusbarTop = 0;
    ConfigSave();
    break;
  case IDM_SB_FOLLOW:
    g_cfg.statusbarFollowCursor = !g_cfg.statusbarFollowCursor;
    ConfigSave();
    break;
  case IDM_ASSOC_ONTY: {
    bool reg = IsFileAssociationRegistered();
    RegisterFileAssociation(!reg);
    StatusShow(!reg ? L(L_ASSOC_SUCCESS) : L(L_ASSOC_REMOVED), 2500);
    break;
  }
  case IDM_TH0:
  case IDM_TH1:
  case IDM_TH2:
    g_cfg.theme = id - IDM_TH0;
    ApplyWindowMaterial(g_hwnd);
    if (IsWindow(g_abHwnd)) {
      ApplyWindowMaterial(g_abHwnd);
      InvalidateRect(g_abHwnd, nullptr, TRUE);
    }
    SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 16));
    SendMessageW(g_hwnd, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 32));
    TrayIconRefresh();
    ConfigSave();
    break;
  case IDM_TRI0:
  case IDM_TRI1:
  case IDM_TRI2:
    g_cfg.trayIcon = id - IDM_TRI0;
    ConfigSave();
    TrayIconRefresh();
    break;
  case IDM_UI0:
  case IDM_UI1:
  case IDM_UI2:
    g_cfg.uiIcon = id - IDM_UI0;
    ConfigSave();
    SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 16));
    SendMessageW(g_hwnd, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 32));
    if (IsWindow(g_abHwnd))
      InvalidateRect(g_abHwnd, nullptr, FALSE);
    break;
  case IDM_LENG:
    g_cfg.lang = 1;
    LangInit();
    ConfigSave();
    break;
  case IDM_LENRU:
    g_cfg.lang = 2;
    LangInit();
    ConfigSave();
    break;
  case IDM_LENIMP: {
    if (g_state.load() != ST_IDLE)
      break;
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW o = {};
    o.lStructSize = sizeof(o);
    o.hwndOwner = g_hwnd;
    o.lpstrFilter = L"JSON (*.json)\0*.json\0All files\0*.*\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&o)) {
      std::wstring name;
      if (LangImport(path, name)) {
        g_cfg.lang = 0;
        ConfigSave();
        StatusShow(L(L_ST_TRANSLATED), 2500);
        InvalidateRect(g_hwnd, nullptr, TRUE);
        if (IsWindow(g_abHwnd))
          InvalidateRect(g_abHwnd, nullptr, TRUE);
      } else
        StatusShow(L(L_ST_INVALID), 3000);
    }
    break;
  }
  case IDM_LENTR:
    ShellExecuteW(g_hwnd, L"open", URL_ISSUE, nullptr, nullptr, SW_SHOWNORMAL);
    break;
  case IDM_HKR0:
  case IDM_HKP0:
  case IDM_HKAC:
  case IDM_HKACPOS:
  case IDM_HKPANIC: {
    // 0 record, 1 play, 2 autoclick, 3 click position, 4 panic stop
    int slot = (id == IDM_HKPANIC) ? 4 : id - IDM_HKR0;
    int *mods[5] = {&g_cfg.recordMods, &g_cfg.playMods, &g_cfg.acMods,
                    &g_cfg.acPosMods, &g_cfg.panicMods};
    int *vks[5] = {&g_cfg.recordVk, &g_cfg.playVk, &g_cfg.acVk,
                   &g_cfg.acPosVk, &g_cfg.panicVk};
    const wchar_t *titles[5] = {L(L_HK_REC), L(L_HK_PLAY), L(L_AUTOCLICK),
                                L(L_HK_AC_POS), L(L_PANIC_KEYS)};
    int mo = 0, vk = 0;
    if (UiHotkeyCapture(g_hwnd, titles[slot], mo, vk)) {
      bool clash = false;
      for (int i = 0; i < 5; i++)
        if (i != slot && mo == *mods[i] && vk == *vks[i])
          clash = true;
      if (clash) {
        UiMessage(g_hwnd, L(L_ST_CONFLICT), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
        break;
      }
      *mods[slot] = mo;
      *vks[slot] = vk;
      ConfigSave();
      HotkeyRegisterAll(g_hwnd);
    }
    break;
  }
  case IDM_AC_TOGGLE:
    if (g_acRunning.load()) {
      Autoclick_Stop();
      break;
    }
    if (g_state.load() != ST_IDLE) {
      StatusShow(L(L_ST_AC_BUSY), 2500);
      break;
    }
    if (g_cfg.acMode == 1 && !g_cfg.acHasPos) {
      std::wstring m = std::wstring(L(L_ST_AC_NOPOS)) + L" (" +
                       FmtHk(g_cfg.acPosMods, g_cfg.acPosVk) + L")";
      StatusShow(m.c_str(), 3500);
      break;
    }
    if (Autoclick_Start()) {
      if (g_cfg.autoMinimize)
        ShowWindow(g_hwnd, SW_HIDE);
    }
    break;
  case IDM_AC_SETPOS:
    if (Autoclick_SavePosition()) {
      wchar_t t[96];
      swprintf_s(t, L"%s: %d, %d", L(L_ST_AC_POS_SAVED), g_cfg.acX, g_cfg.acY);
      StatusShow(t, 2500);
    }
    break;
  case IDM_AC_HOURS:
  case IDM_AC_MINS:
  case IDM_AC_SECS:
  case IDM_AC_MS: {
    int *field = &g_cfg.acHours;
    const wchar_t *name = L(L_AC_HOURS);
    int maxv = 99;
    if (id == IDM_AC_MINS) {
      field = &g_cfg.acMins;
      name = L(L_AC_MINS);
      maxv = 59;
    } else if (id == IDM_AC_SECS) {
      field = &g_cfg.acSecs;
      name = L(L_AC_SECS);
      maxv = 59;
    } else if (id == IDM_AC_MS) {
      field = &g_cfg.acMs;
      name = L(L_AC_MS);
      maxv = 999;
    }
    std::wstring prompt = std::wstring(name) + L" (0-" + std::to_wstring(maxv) + L")";
    int v = *field;
    if (InputDialog(prompt.c_str(), *field, 0, maxv, v)) {
      *field = v;
      if (g_cfg.acHours == 0 && g_cfg.acMins == 0 && g_cfg.acSecs == 0 &&
          g_cfg.acMs < 1)
        g_cfg.acMs = 1; // an interval of zero would peg the CPU
      ConfigSave();
    }
    break;
  }
  case IDM_AC_BTN_L:
  case IDM_AC_BTN_R:
  case IDM_AC_BTN_M:
    g_cfg.acButton = id - IDM_AC_BTN_L;
    ConfigSave();
    break;
  case IDM_AC_SINGLE:
  case IDM_AC_DOUBLE:
    g_cfg.acDouble = id == IDM_AC_DOUBLE ? 1 : 0;
    ConfigSave();
    break;
  case IDM_AC_UNTIL:
    g_cfg.acRepeatUntilStop = 1;
    ConfigSave();
    break;
  case IDM_AC_COUNT: {
    int v = g_cfg.acRepeatCount;
    if (InputDialog(L(L_Q_AC_REPEAT), g_cfg.acRepeatCount, 1, 999999, v)) {
      g_cfg.acRepeatCount = v;
      g_cfg.acRepeatUntilStop = 0;
      ConfigSave();
    }
    break;
  }
  case IDM_AC_FOLLOW:
    g_cfg.acMode = 0;
    ConfigSave();
    break;
  case IDM_AC_FIXED:
    g_cfg.acMode = 1;
    ConfigSave();
    if (!g_cfg.acHasPos) {
      std::wstring m = std::wstring(L(L_ST_AC_NOPOS)) + L" (" +
                       FmtHk(g_cfg.acPosMods, g_cfg.acPosVk) + L")";
      StatusShow(m.c_str(), 3500);
    }
    break;
  case IDM_AUTOMINIMIZE:
    g_cfg.autoMinimize = !g_cfg.autoMinimize;
    ConfigSave();
    break;
  case IDM_TOPMOST:
    g_cfg.topmost = !g_cfg.topmost;
    SetWindowPos(g_hwnd, g_cfg.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0,
                 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ConfigSave();
    break;
  case IDM_UPDATE:
    if (Update_Found())
      ShellExecuteW(g_hwnd, L"open", URL_RELEASES, nullptr, nullptr,
                    SW_SHOWNORMAL);
    else
      Update_ManualCheck();
    break;
  case IDM_ABOUT:
    ShowAbout();
    break;
  case IDM_EXIT:
    ConfigSave();
    DestroyWindow(g_hwnd);
    break;
  default:
    break;
  }
  TraySync();
  InvalidateRect(g_hwnd, nullptr, FALSE);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
  case WM_CREATE:
    DragAcceptFiles(hwnd, TRUE);
    ApplyWindowMaterial(hwnd);
    TryRoundCorners(hwnd);
    SetTimer(hwnd, 1, 200, nullptr);
    SetTimer(hwnd, 3, 120, nullptr);
    SetTimer(hwnd, 14, 10, nullptr);
    HotkeyRegisterAll(hwnd);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 16));
    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadAppIconId(GfxUiIconId(), 32));
    TipInit(hwnd);
    if (g_cfg.topmost)
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return 0;
  case WM_PAINT:
    Paint();
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(hwnd, &pt);
    RECT rmin = GfxMinRect(WIN_W), rclose = GfxCloseRect(WIN_W),
         rpin = GfxPinRect(WIN_W);
    RECT ric = {0, 0, 32, TB_H};
    if (PtInRect(&rmin, pt) || PtInRect(&rclose, pt) || PtInRect(&rpin, pt) ||
        PtInRect(&ric, pt))
      return HTCLIENT;
    if (pt.y < TB_H)
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    int nb = -1, nt = 0;
    for (int i = 0; i < TILES; i++) {
      RECT r = TileRect(i);
      if (PtInRect(&r, pt))
        nb = i;
    }
    RECT rmin = GfxMinRect(WIN_W), rclose = GfxCloseRect(WIN_W),
         rpin = GfxPinRect(WIN_W);
    RECT ric = {0, 0, 30, TB_H}, rsb = ReSaveBtnRect();
    if (PtInRect(&rmin, pt))
      nt = 1;
    if (PtInRect(&rclose, pt))
      nt = 2;
    if (PtInRect(&rpin, pt))
      nt = 3;
    if (PtInRect(&ric, pt))
      nt = 4;
    if (PtInRect(&rsb, pt) && g_state.load() == ST_IDLE && !g_events.empty())
      nt = 6;
    UINT tid = 0;
    switch (nt) {
    case 1:
      tid = TIP_MIN;
      break;
    case 2:
      tid = TIP_CLOSE;
      break;
    case 3:
      tid = TIP_PIN;
      break;
    case 4:
      tid = TIP_ICON;
      break;
    case 6:
      tid = TIP_RESAVE;
      break;
    default:
      if (nb >= 0)
        tid = TIP_TILE0 + nb;
      break;
    }
    TipSet(tid);
    if (nb != g_hoverB || nt != g_hoverT) {
      g_hoverB = nb;
      g_hoverT = nt;
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    SetCursor(nt >= 4 ? LoadCursorW(nullptr, IDC_HAND)
                      : LoadCursorW(nullptr, IDC_ARROW));
    TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&t);
    return 0;
  }
  case WM_MOUSELEAVE:
    g_hoverB = -1;
    g_hoverT = 0;
    TipSet(0);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  case WM_LBUTTONDOWN:
    g_pressB = g_hoverB;
    if (g_hoverT == 1)
      ShowWindow(hwnd, SW_MINIMIZE);
    else if (g_hoverT == 2)
      ShowWindow(hwnd, SW_HIDE);
    else if (g_hoverT == 3) {
      g_cfg.topmost = !g_cfg.topmost;
      SetWindowPos(hwnd, g_cfg.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0,
                   0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      ConfigSave();
    } else if (g_hoverT == 4)
      ShowAbout();
    else if (g_hoverT == 6) {
      if (g_state.load() == ST_IDLE && !g_events.empty()) {
        if (!g_dirty) {
          StatusShow(L(L_SAVED), 2000);
        } else if (!g_lastFile.empty()) {
          if (Engine_Save(g_lastFile.c_str())) {
            g_dirty = false;
            StatusShow(L(L_ST_SAVED), 2500);
          } else
            StatusShow(L(L_SAVEFAIL), 3000);
        } else
          DoCmd(IDM_SAVE);
      }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP:
    if (g_pressB >= 0 && g_hoverB == g_pressB) {
      int cmd = CMD_BTN[g_pressB];
      g_pressB = -1;
      if (cmd)
        DoCmd(cmd);
      else
        TrackMenu();
      return 0;
    }
    g_pressB = -1;
    return 0;
  case WM_RBUTTONUP:
    TrackMenu();
    return 0;
  case WM_COMMAND:
    DoCmd(LOWORD(w));
    return 0;
  case WM_TIMER:
    if (w == 1)
      StatusTick();
    else if (w == 3)
      TipPoll();
    else if (w == 14)
      Engine_FlushMoves();
    return 0;
  case WM_HOTKEY:
    if (Macros_IsModalOpen())
      return 0;
    if (w == HK_ID_REC)
      DoCmd(IDM_REC);
    else if (w == HK_ID_PLAY)
      DoCmd(IDM_PLAY);
    else if (w == HK_ID_AC)
      DoCmd(IDM_AC_TOGGLE);
    else if (w == HK_ID_ACPOS)
      DoCmd(IDM_AC_SETPOS);
    return 0;
  case WM_INPUT:
    Engine_RawInput(l);
    return DefWindowProcW(hwnd, msg, w, l);
  case WM_APP_TRAY:
    if (l == WM_LBUTTONUP) {
      if (IsWindowVisible(hwnd))
        ShowWindow(hwnd, SW_HIDE);
      else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
      }
    } else if (l == WM_RBUTTONUP)
      TrackMenu();
    return 0;
  case WM_APP_UPDATE:
    if (l == 1)
      TrayBalloon(L(L_UPDATE_AVAIL), L"github.com/Myzoxxsekolah/AutoOnty/releases");
    else if (w) {
      if (l == 0)
        StatusShow(L(L_ST_UPTODATE), 3000);
      else
        StatusShow(L(L_ST_UPDATEFAIL), 3000);
    }
    if (IsWindow(g_abHwnd))
      InvalidateRect(g_abHwnd, nullptr, FALSE);
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  case WM_APP_PLAYDONE:
    TraySync();
    StatusHide();
    CursorTipHide();
    if (g_cfg.autoMinimize) {
      ShowWindow(hwnd, SW_SHOW);
      SetForegroundWindow(hwnd);
    }
    if (g_playStopped.load())
      StatusShow(L(L_ST_PLAYSTOP), 2500);
    InvalidateRect(hwnd, nullptr, FALSE);
    if (g_closeAfterPlay) {
      DestroyWindow(hwnd);
      return 0;
    }
    return 0;
  case WM_APP_AUTOCLICKDONE:
    // If a recording/playback took over (it stops autoclick first), leave the
    // status bar and window alone - they belong to the macro now.
    if (g_state.load() == ST_IDLE) {
      StatusHide();
      if (g_cfg.autoMinimize) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
      }
      std::wstring m = std::wstring(L(L_ST_AC_STOP)) + L" (" +
                       std::to_wstring(g_acClicks.load()) + L")";
      StatusShow(m.c_str(), 2500);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  case WM_APP + 5:
    if (g_state.load() == ST_PLAY) {
      std::wstring tip = std::wstring(L(L_ST_PLAYING)) + L" \u2022 " +
                         L(L_STOP_BIND) + L": " + ActiveStopKeysText();
      CursorTipShow(tip);
    }
    return 0;
  case WM_KEYDOWN:
    if (GetKeyState(VK_CONTROL) & 0x8000) {
      if (w == 'C' || w == 'c') {
        DoCmd(IDM_COPY_CLIPBOARD);
        return 0;
      }
      if (w == 'V' || w == 'v') {
        DoCmd(IDM_PASTE_CLIPBOARD);
        return 0;
      }
      if (w == 'S' || w == 's') {
        if (g_state.load() == ST_IDLE && !g_events.empty()) {
          if (g_dirty && !g_lastFile.empty()) {
            if (Engine_Save(g_lastFile.c_str())) {
              g_dirty = false;
              StatusShow(L(L_ST_SAVED), 2500);
            } else
              StatusShow(L(L_SAVEFAIL), 3000);
            InvalidateRect(hwnd, nullptr, FALSE);
          } else if (g_dirty)
            DoCmd(IDM_SAVE);
        }
        return 0;
      }
      if (w == 'O' || w == 'o') {
        DoCmd(IDM_OPEN);
        return 0;
      }
    }
    return DefWindowProcW(hwnd, msg, w, l);
  case WM_COPYDATA: {
    COPYDATASTRUCT *cds = (COPYDATASTRUCT *)l;
    if (cds && cds->lpData && !Macros_IsModalOpen()) {
      if (cds->dwData == 1) {
        std::wstring p = CleanPath((const wchar_t *)cds->lpData);
        if (Engine_Load(p.c_str())) {
          g_lastFile = p;
          g_dirty = false;
          RecentAdd(p);
          ConfigSave();
          StatusShow(L(L_ST_LOADED), 2500);
          InvalidateRect(hwnd, nullptr, FALSE);
        } else {
          StatusShow(L(L_ST_INVALID), 2500);
          UiMessage(hwnd, L(L_ST_INVALID), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
        }
      } else if (cds->dwData == 2) {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW((const wchar_t *)cds->lpData, &argc);
        if (argv) {
          std::wstring f;
          bool play = false, rec = false;
          for (int i = 1; i < argc; i++) {
            std::wstring arg = argv[i];
            if (arg == L"--play" || arg == L"-p")
              play = true;
            else if (arg == L"--rec" || arg == L"-r")
              rec = true;
            else if ((arg == L"--loop" || arg == L"-l") && i + 1 < argc) {
              int n = _wtoi(argv[++i]);
              if (n == 0)
                g_cfg.continuous = 1;
              else {
                g_cfg.loops = n;
                g_cfg.continuous = 0;
              }
            } else if ((arg == L"--speed" || arg == L"-s") && i + 1 < argc) {
              double sp = _wtof(argv[++i]);
              if (sp == 0.25)
                g_cfg.speed = -1;
              else if (sp == 0.5)
                g_cfg.speed = 0;
              else if (sp == 0.75)
                g_cfg.speed = -2;
              else if (sp == 1.0)
                g_cfg.speed = 1;
              else if (sp == 1.25)
                g_cfg.speed = -3;
              else if (sp == 1.5)
                g_cfg.speed = -4;
              else if (sp == 0.0)
                g_cfg.speed = -5;
              else if (sp == 100.0)
                g_cfg.speed = 100;
              else if (sp >= 2 && sp <= 50 && (double)(int)sp == sp)
                g_cfg.speed = (int)sp;
              else {
                g_cfg.speed = 999;
                g_cfg.speedCustom =
                    sp < 0.01 ? 0.01 : (sp > 1000.0 ? 1000.0 : sp);
              }
            } else if (arg == L"--close-after" || arg == L"-c")
              g_closeAfterPlay = true;
            else if (arg == L"--file" || arg == L"-f") {
              if (i + 1 < argc)
                f = CleanPath(argv[++i]);
            } else if (!arg.empty() && arg[0] != L'-') {
              if (f.empty())
                f = CleanPath(arg);
            }
          }
          LocalFree(argv);
          if (!f.empty()) {
            if (Engine_Load(f.c_str())) {
              g_lastFile = f;
              g_dirty = false;
              RecentAdd(f);
              ConfigSave();
              StatusShow(L(L_ST_LOADED), 2500);
            } else {
              StatusShow(L(L_ST_INVALID), 2500);
              UiMessage(hwnd, L(L_ST_INVALID), L(L_APPNAME), UI_OK,
                        UI_ICON_ERROR);
            }
          }
          if (play)
            DoCmd(IDM_PLAY);
          else if (rec)
            DoCmd(IDM_REC);
          InvalidateRect(hwnd, nullptr, FALSE);
        }
      }
    }
    return TRUE;
  }
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)w;
    wchar_t path[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
      std::wstring p = CleanPath(path);
      if (Engine_Load(p.c_str())) {
        g_lastFile = p;
        g_dirty = false;
        RecentAdd(p);
        ConfigSave();
        StatusShow(L(L_ST_LOADED), 2500);
        InvalidateRect(hwnd, nullptr, FALSE);
      } else {
        StatusShow(L(L_ST_INVALID), 2500);
        UiMessage(hwnd, L(L_ST_INVALID), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
      }
    }
    DragFinish(hDrop);
    return 0;
  }
  case WM_CLOSE:
    ShowWindow(hwnd, SW_HIDE);
    return 0;
  case WM_QUERYENDSESSION:
    ConfigSave();
    return TRUE;
  case WM_ENDSESSION:
    if (w) {
      Engine_Shutdown();
      Autoclick_StopAndWait(1500);
      ConfigSave();
      TrayRemove();
      DestroyWindow(hwnd);
    }
    return 0;
  case WM_DESTROY:
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2);
    Engine_Shutdown();
    Autoclick_StopAndWait(1500);
    if (!IsIconic(hwnd)) {
      RECT r;
      GetWindowRect(hwnd, &r);
      g_cfg.x = r.left;
      g_cfg.y = r.top;
    }
    ConfigSave();
    TrayRemove();
    if (g_trayIcon) {
      DestroyIcon(g_trayIcon);
      g_trayIcon = nullptr;
    }
    PostQuitMessage(0);
    return 0;
  default:
    if (msg == g_msgTaskbar && g_msgTaskbar) {
      TrayAdd();
      return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
  }
}

struct InDlg {
  HWND h = nullptr, edit = nullptr;
  const wchar_t *prompt = nullptr;
  int *outInt = nullptr;
  double *outDouble = nullptr;
  double minv = 0, maxv = 0;
  bool isFloat = false;
  bool ok = false, done = false, tracking = false, focus = false;
  int hover = 0;
};

static InDlg *g_in = nullptr;

static const int IN_W = 280, IN_H = 114;
static RECT InCaRect() {
  RECT r = {0, 84, IN_W / 2, IN_H};
  return r;
}
static RECT InOkRect() {
  RECT r = {IN_W / 2, 84, IN_W, IN_H};
  return r;
}
static RECT InBannerRect() {
  RECT r = {0, 30, IN_W, 56};
  return r;
}
static RECT InInputRow() {
  RECT r = {0, 56, IN_W, 84};
  return r;
}

static COLORREF InFieldColor(bool focus) {
  if (g_cfg.theme == THEME_LIGHT)
    return focus ? RGB(225, 225, 225) : RGB(238, 238, 238);
  if (g_cfg.theme == THEME_ACRYLIC)
    return focus ? RGB(34, 34, 34) : RGB(21, 21, 21);
  return focus ? RGB(46, 46, 46) : RGB(37, 37, 37);
}

static LRESULT CALLBACK InputEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  WNDPROC old = (WNDPROC)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) {
    PostMessageW(GetParent(h), w == VK_RETURN ? WM_IN_OK : WM_IN_CANCEL, 0, 0);
    return 0;
  }
  return CallWindowProcW(old, h, m, w, l);
}

static LRESULT CALLBACK InputProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  InDlg *d = g_in;
  switch (m) {
  case WM_CREATE: {
    CREATESTRUCTW *cs = (CREATESTRUCTW *)l;
    d = (InDlg *)cs->lpCreateParams;
    d->h = h;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT |
                  (d->isFloat ? 0 : ES_NUMBER);
    d->edit = CreateWindowExW(0, L"EDIT", L"", style, 14, 60, IN_W - 28, 20, h,
                              (HMENU)1, g_hInst, nullptr);
    SendMessageW(d->edit, WM_SETFONT, (WPARAM)ThFont(10), TRUE);
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(d->edit, GWLP_WNDPROC,
                                             (LONG_PTR)InputEditProc);
    SetWindowLongPtrW(d->edit, GWLP_USERDATA, (LONG_PTR)old);
    SetFocus(d->edit);
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w;
    SetTextColor(dc, ThText());
    COLORREF c = InFieldColor(d->focus);
    SetBkColor(dc, c);
    static COLORREF bc = -1;
    static HBRUSH br = nullptr;
    if (!br || c != bc) {
      if (br)
        DeleteObject(br);
      br = CreateSolidBrush(c);
      bc = c;
    }
    return (LRESULT)br;
  }
  case WM_IN_OK: {
    wchar_t b[64];
    GetWindowTextW(d->edit, b, 64);
    for (int i = 0; b[i]; i++)
      if (b[i] == L',')
        b[i] = L'.';
    if (d->isFloat) {
      double v = wcstod(b, nullptr);
      if (v < d->minv)
        v = d->minv;
      if (v > d->maxv)
        v = d->maxv;
      if (d->outDouble)
        *d->outDouble = v;
    } else {
      int v = wcstol(b, nullptr, 10);
      if ((double)v < d->minv)
        v = (int)d->minv;
      if ((double)v > d->maxv)
        v = (int)d->maxv;
      if (d->outInt)
        *d->outInt = v;
    }
    d->ok = true;
    d->done = true;
    DestroyWindow(h);
    return 0;
  }
  case WM_IN_CANCEL:
    d->done = true;
    DestroyWindow(h);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = GfxCloseRect(IN_W);
    if (pt.y < TB_H && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bm);
    Gdiplus::Graphics g(mem);
    bool acry = g_cfg.theme == THEME_ACRYLIC;
    bool light = g_cfg.theme == THEME_LIGHT;
    Gdiplus::SolidBrush bg(acry ? GfxAcrylicBg() : GdiCol(ThBg()));
    g.FillRectangle(&bg, 0, 0, rc.right, rc.bottom);
    Gdiplus::Pen pen(acry ? Gdiplus::Color(180, 56, 56, 56)
                          : GdiCol(ThBorder()));
    g.DrawRectangle(&pen, 0, 0, rc.right - 1, rc.bottom - 1);
    GfxChrome(g, mem, rc.right, L(L_SETTINGS), d->hover == 3 ? 2 : 0, CHB_SEP);
    Gdiplus::SolidBrush txt(GdiCol(ThText()));
    Gdiplus::SolidBrush gsep(GdiCol(ThBorder()));

    RECT rb = InBannerRect();
    Gdiplus::SolidBrush banf(acry ? Gdiplus::Color(40, 50, 50, 50)
                                  : (light ? Gdiplus::Color(245, 245, 245)
                                           : Gdiplus::Color(60, 45, 45, 45)));
    g.FillRectangle(&banf, (REAL)rb.left, (REAL)rb.top,
                    (REAL)(rb.right - rb.left), (REAL)(rb.bottom - rb.top));
    g.FillRectangle(&gsep, (REAL)rb.left, (REAL)rb.top,
                    (REAL)(rb.right - rb.left), 1.0f);
    g.FillRectangle(&gsep, (REAL)rb.left, (REAL)rb.top, 1.0f,
                    (REAL)(rb.bottom - rb.top));
    g.FillRectangle(&gsep, (REAL)(rb.right - 1), (REAL)rb.top, 1.0f,
                    (REAL)(rb.bottom - rb.top));
    {
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
      DrawGlyph(mem, g, 16.0f, 43.0f, 11, (wchar_t)0xE946, ThAccent());
      Gdiplus::Font f(mem, ThFont(9));
      static Gdiplus::StringFormat sfLV;
      sfLV.SetAlignment(Gdiplus::StringAlignmentNear);
      sfLV.SetLineAlignment(Gdiplus::StringAlignmentCenter);
      Gdiplus::SolidBrush dimBrush(GdiCol(ThDim()));
      g.DrawString(d->prompt, -1, &f,
                   Gdiplus::RectF(30.0f, (REAL)rb.top, (REAL)(rb.right - 38),
                                  (REAL)(rb.bottom - rb.top)),
                   &sfLV, &dimBrush);
    }

    RECT ri = InInputRow();
    Gdiplus::SolidBrush inf(GdiCol(InFieldColor(d->focus)));
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
    g.FillRectangle(&inf, (REAL)ri.left, (REAL)ri.top,
                    (REAL)(ri.right - ri.left), (REAL)(ri.bottom - ri.top));
    g.FillRectangle(&gsep, (REAL)ri.left, (REAL)ri.top,
                    (REAL)(ri.right - ri.left), 1.0f);
    g.FillRectangle(&gsep, (REAL)ri.left, (REAL)ri.top, 1.0f,
                    (REAL)(ri.bottom - ri.top));
    g.FillRectangle(&gsep, (REAL)(ri.right - 1), (REAL)ri.top, 1.0f,
                    (REAL)(ri.bottom - ri.top));

    RECT ro = InOkRect(), rca = InCaRect();
    Gdiplus::SolidBrush okf(
        acry ? (d->hover == 1 ? Gdiplus::Color(140, 80, 160, 224)
                              : Gdiplus::Color(100, 70, 140, 200))
             : (light ? (d->hover == 1 ? Gdiplus::Color(255, 90, 170, 220)
                                       : Gdiplus::Color(255, 80, 155, 210))
                      : (d->hover == 1 ? Gdiplus::Color(255, 70, 145, 205)
                                       : Gdiplus::Color(255, 55, 125, 185))));
    Gdiplus::SolidBrush caf(
        acry ? (d->hover == 2 ? Gdiplus::Color(70, 70, 70, 70)
                              : Gdiplus::Color(40, 50, 50, 50))
             : (light ? (d->hover == 2 ? Gdiplus::Color(230, 220, 220, 220)
                                       : Gdiplus::Color(255, 245, 245, 245))
                      : (d->hover == 2 ? Gdiplus::Color(255, 60, 60, 60)
                                       : Gdiplus::Color(255, 42, 42, 42))));
    g.FillRectangle(&okf, (REAL)ro.left, (REAL)ro.top,
                    (REAL)(ro.right - ro.left), (REAL)(ro.bottom - ro.top));
    g.FillRectangle(&caf, (REAL)rca.left, (REAL)rca.top,
                    (REAL)(rca.right - rca.left), (REAL)(rca.bottom - rca.top));
    g.FillRectangle(&gsep, (REAL)rca.left, (REAL)rca.top,
                    (REAL)(ro.right - rca.left), 1.0f);
    g.FillRectangle(&gsep, (REAL)(ro.left), (REAL)rca.top, 1.0f,
                    (REAL)(rca.bottom - rca.top));
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    Gdiplus::Font fb(mem, ThFont(10, false));
    Gdiplus::SolidBrush okText(GdiCol(RGB(240, 245, 250)));
    g.DrawString(L(L_OK), -1, &fb,
                 Gdiplus::RectF((REAL)ro.left, (REAL)ro.top,
                                (REAL)(ro.right - ro.left),
                                (REAL)(ro.bottom - ro.top)),
                 &SfC(), &okText);
    g.DrawString(L(L_CANCEL), -1, &fb,
                 Gdiplus::RectF((REAL)rca.left, (REAL)rca.top,
                                (REAL)(rca.right - rca.left),
                                (REAL)(rca.bottom - rca.top)),
                 &SfC(), &txt);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
    EndPaint(h, &ps);
    return 0;
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
    RECT ro = InOkRect(), rca = InCaRect(), rcl = GfxCloseRect(IN_W);
    int nh = PtInRect(&ro, pt)    ? 1
             : PtInRect(&rca, pt) ? 2
             : PtInRect(&rcl, pt) ? 3
                                  : 0;
    if (nh != d->hover) {
      d->hover = nh;
      InvalidateRect(h, nullptr, FALSE);
    }
    if (!d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    d->hover = 0;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP:
    if (d->hover == 1)
      PostMessageW(h, WM_IN_OK, 0, 0);
    else if (d->hover == 2 || d->hover == 3)
      PostMessageW(h, WM_IN_CANCEL, 0, 0);
    return 0;
  case WM_DESTROY:
    EnableWindow(g_hwnd, TRUE);
    SetForegroundWindow(g_hwnd);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static bool InputDialog(const wchar_t *prompt, int init, int minv, int maxv,
                        int &out) {
  InDlg d;
  d.prompt = prompt;
  d.outInt = &out;
  d.minv = (double)minv;
  d.maxv = (double)maxv;
  d.isFloat = false;
  g_in = &d;
  RECT rw;
  GetWindowRect(g_hwnd, &rw);
  int x = rw.left + ((rw.right - rw.left) - IN_W) / 2;
  int y = rw.top + ((rw.bottom - rw.top) - IN_H) / 2;
  EnableWindow(g_hwnd, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyInput", L"", WS_POPUP, x,
                           y, IN_W, IN_H, g_hwnd, nullptr, g_hInst, &d);
  GfxRoundRegion(h);
  wchar_t b[32];
  swprintf_s(b, L"%d", init);
  SetWindowTextW(d.edit, b);
  SendMessageW(d.edit, EM_SETSEL, 0, -1);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  MSG m;
  while (!d.done && IsWindow(h) && GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  g_in = nullptr;
  return d.ok;
}

static bool InputDialogFloat(const wchar_t *prompt, double init, double minv,
                             double maxv, double &out) {
  InDlg d;
  d.prompt = prompt;
  d.outDouble = &out;
  d.minv = minv;
  d.maxv = maxv;
  d.isFloat = true;
  g_in = &d;
  RECT rw;
  GetWindowRect(g_hwnd, &rw);
  int x = rw.left + ((rw.right - rw.left) - IN_W) / 2;
  int y = rw.top + ((rw.bottom - rw.top) - IN_H) / 2;
  EnableWindow(g_hwnd, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyInput", L"", WS_POPUP, x,
                           y, IN_W, IN_H, g_hwnd, nullptr, g_hInst, &d);
  GfxRoundRegion(h);
  wchar_t b[32];
  swprintf_s(b, L"%.4g", init);
  SetWindowTextW(d.edit, b);
  SendMessageW(d.edit, EM_SETSEL, 0, -1);
  ShowWindow(h, SW_SHOW);
  SetForegroundWindow(h);
  MSG m;
  while (!d.done && IsWindow(h) && GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  g_in = nullptr;
  return d.ok;
}

struct AbDlg {
  HWND h = nullptr;
  bool done = false, tracking = false;
  int hover = 0;
};

static AbDlg *g_ab = nullptr;

static const int AB_W = 280, AB_H = 228;
static RECT AbCloseRect() {
  RECT r = {AB_W - 30, 0, AB_W, 30};
  return r;
}
static RECT AbUpdRect() {
  RECT r = {0, AB_H - 58, AB_W, AB_H - 32};
  return r;
}
static RECT AbGhRect() {
  RECT r = {0, AB_H - 32, AB_W / 2, AB_H};
  return r;
}
static RECT AbLiRect() {
  RECT r = {AB_W / 2, AB_H - 32, AB_W, AB_H};
  return r;
}

static std::vector<std::wstring> WrapText(HDC dc, const std::wstring &s,
                                          int maxW) {
  std::vector<std::wstring> out;
  std::wstring line;
  size_t i = 0;
  while (i < s.size()) {
    size_t j = s.find(L' ', i);
    if (j == std::wstring::npos)
      j = s.size();
    std::wstring word = s.substr(i, j - i);
    std::wstring test = line.empty() ? word : line + L" " + word;
    SIZE sz;
    GetTextExtentPoint32W(dc, test.c_str(), (int)test.size(), &sz);
    if (sz.cx > maxW && !line.empty()) {
      out.push_back(line);
      line = word;
    } else
      line = test;
    i = j + 1;
  }
  if (!line.empty())
    out.push_back(line);
  return out;
}

static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  AbDlg *d = g_ab;
  switch (m) {
  case WM_CREATE: {
    CREATESTRUCTW *cs = (CREATESTRUCTW *)l;
    d = (AbDlg *)cs->lpCreateParams;
    g_ab = d;
    if (d)
      d->h = h;
    g_abHwnd = h;
    ApplyWindowMaterial(h);
    TryRoundCorners(h);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;
  case WM_NCHITTEST: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &pt);
    RECT cr = AbCloseRect();
    if (pt.y < 72 && !PtInRect(&cr, pt))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bm);
    Gdiplus::Graphics g(mem);
    bool acry = g_cfg.theme == THEME_ACRYLIC;
    bool light = g_cfg.theme == THEME_LIGHT;
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
    Gdiplus::SolidBrush bg(acry ? GfxAcrylicBg() : GdiCol(ThBg()));
    g.FillRectangle(&bg, 0, 0, rc.right, rc.bottom);
    Gdiplus::SolidBrush header(acry ? Gdiplus::Color(100, 10, 10, 10)
                                    : GdiCol(ThBg2()));
    g.FillRectangle(&header, 0, 0, AB_W, 72);
    Gdiplus::SolidBrush accent(GdiCol(ThAccent()));
    g.FillRectangle(&accent, 0, 72, AB_W, 1);
    Gdiplus::SolidBrush bb(GdiCol(ThBorder()));
    Gdiplus::Pen borderPen(GdiCol(ThBorder()));
    g.DrawRectangle(&borderPen, 0, 0, rc.right - 1, rc.bottom - 1);
    RECT rcl = AbCloseRect();
    int hover = d ? d->hover : 0;
    if (hover == 3) {
      Gdiplus::SolidBrush red(Gdiplus::Color(255, 196, 43, 28));
      g.FillRectangle(&red, (REAL)rcl.left, (REAL)rcl.top,
                      (REAL)(rcl.right - rcl.left),
                      (REAL)(rcl.bottom - rcl.top));
    }
    {
      int cx = (rcl.left + rcl.right) / 2;
      REAL cy = 14.5f, s = 5.0f;
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      Gdiplus::Pen xp(GdiCol(hover == 3 ? RGB(255, 255, 255) : ThText()), 1.0f);
      g.DrawLine(&xp, (REAL)cx - s, cy - s, (REAL)cx + s, cy + s);
      g.DrawLine(&xp, (REAL)cx + s, cy - s, (REAL)cx - s, cy + s);
      g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    }

    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    Gdiplus::Font f18b(mem, ThFont(18, true));
    Gdiplus::Font f10(mem, ThFont(10));
    Gdiplus::Font f9(mem, ThFont(9));
    Gdiplus::Font f9b(mem, ThFont(9, true));
    Gdiplus::SolidBrush txt(GdiCol(ThText()));
    Gdiplus::SolidBrush dim(GdiCol(ThDim()));
    Gdiplus::SolidBrush ac(GdiCol(ThAccent()));
    HICON ic = LoadAppIconId(GfxUiIconId(), 36);
    if (ic) {
      DrawIconEx(mem, 18, 18, ic, 36, 36, 0, nullptr, DI_NORMAL);
      DestroyIcon(ic);
    }
    g.DrawString(L(L_APPNAME), -1, &f18b, Gdiplus::PointF(64, 13), &txt);
    g.DrawString(APP_VERSION, -1, &f9, Gdiplus::PointF(65, 38), &dim);

    std::wstring desc = L(L_ABOUT_DESC);
    SelectObject(mem, ThFont(10));
    std::vector<std::wstring> lines = WrapText(mem, desc, AB_W - 36);
    int dy = 84;
    for (const auto &ln : lines) {
      g.DrawString(ln.c_str(), -1, &f10, Gdiplus::PointF(18, (REAL)dy), &txt);
      dy += 15;
    }

    std::wstring meta = std::wstring(L(L_DEVELOPED)) + L"\n" + L(L_LIC_LINE);
    g.DrawString(meta.c_str(), -1, &f9,
                 Gdiplus::RectF(18.0f, (REAL)dy + 6, (REAL)(AB_W - 36), 30.0f),
                 &SfL(), &dim);

    RECT ru = AbUpdRect();
    if (hover == 4) {
      g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
      Gdiplus::SolidBrush hb(Gdiplus::Color(35, 90, 90, 90));
      g.FillRectangle(&hb, (REAL)ru.left, (REAL)ru.top,
                      (REAL)(ru.right - ru.left), (REAL)(ru.bottom - ru.top));
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    }
    g.FillRectangle(&bb, (REAL)ru.left, (REAL)ru.top,
                    (REAL)(ru.right - ru.left), 1.0f);
    g.FillRectangle(&bb, (REAL)ru.left, (REAL)(ru.bottom - 1),
                    (REAL)(ru.right - ru.left), 1.0f);
    const wchar_t *upTxt = L(L_UPDATE);
    int ust = Update_State();
    if (ust == UR_LATEST)
      upTxt = L(L_ST_UPTODATE);
    else if (ust == UR_FOUND)
      upTxt = L(L_UPDATE_AVAIL);
    else if (ust == UR_FAIL)
      upTxt = L(L_ST_UPDATEFAIL);
    Gdiplus::SolidBrush ufb(GdiCol(ust == UR_FOUND ? RecRed()
                                   : (ust == UR_LATEST || ust == UR_CHECKING)
                                       ? ThAccent()
                                       : ThDim()));
    g.DrawString(upTxt, -1, &f9,
                 Gdiplus::RectF((REAL)ru.left, (REAL)ru.top,
                                (REAL)(ru.right - ru.left),
                                (REAL)(ru.bottom - ru.top)),
                 &SfC(), &ufb);
    RECT rg = AbGhRect(), rl2 = AbLiRect();
    Gdiplus::Color ghc = light
                             ? GdiCol(hover == 1 ? ThHover2() : ThBg2())
                             : Gdiplus::Color(hover == 1 ? 70 : 50, 60, 60, 60);
    Gdiplus::Color lic = light
                             ? GdiCol(hover == 2 ? ThHover2() : ThBg2())
                             : Gdiplus::Color(hover == 2 ? 70 : 50, 60, 60, 60);
    Gdiplus::SolidBrush ghf(ghc);
    Gdiplus::SolidBrush lif(lic);
    g.FillRectangle(&ghf, (REAL)rg.left, (REAL)rg.top,
                    (REAL)(rg.right - rg.left), (REAL)(rg.bottom - rg.top));
    g.FillRectangle(&lif, (REAL)rl2.left, (REAL)rl2.top,
                    (REAL)(rl2.right - rl2.left), (REAL)(rl2.bottom - rl2.top));
    g.FillRectangle(&bb, (REAL)rg.left, (REAL)rg.top, 1.0f,
                    (REAL)(rg.bottom - rg.top));
    g.FillRectangle(&bb, (REAL)(rg.right - 1), (REAL)rg.top, 1.0f,
                    (REAL)(rg.bottom - rg.top));
    g.FillRectangle(&bb, (REAL)(rl2.right - 1), (REAL)rl2.top, 1.0f,
                    (REAL)(rl2.bottom - rl2.top));
    g.FillRectangle(&bb, (REAL)rg.left, (REAL)rg.bottom - 1,
                    (REAL)(rl2.right - rg.left), 1.0f);
    static Gdiplus::StringFormat sfA;
    sfA.SetAlignment(Gdiplus::StringAlignmentCenter);
    sfA.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    COLORREF btnTxt = light ? ThText() : RGB(255, 255, 255);
    Gdiplus::SolidBrush btb(GdiCol(btnTxt));
    g.DrawString(L(L_GITHUB), -1, &f9b,
                 Gdiplus::RectF((REAL)rg.left, (REAL)rg.top,
                                (REAL)(rg.right - rg.left),
                                (REAL)(rg.bottom - rg.top)),
                 &sfA, &btb);
    g.DrawString(L(L_LICENSE), -1, &f9b,
                 Gdiplus::RectF((REAL)rl2.left, (REAL)rl2.top,
                                (REAL)(rl2.right - rl2.left),
                                (REAL)(rl2.bottom - rl2.top)),
                 &sfA, &btb);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    RECT rg = AbGhRect(), rl = AbLiRect(), rcl = AbCloseRect(),
         ru = AbUpdRect();
    int nh = PtInRect(&rg, pt)    ? 1
             : PtInRect(&rl, pt)  ? 2
             : PtInRect(&rcl, pt) ? 3
             : PtInRect(&ru, pt)  ? 4
                                  : 0;
    if (d && nh != d->hover) {
      d->hover = nh;
      InvalidateRect(h, nullptr, FALSE);
    }
    SetCursor(nh ? LoadCursorW(nullptr, IDC_HAND)
                 : LoadCursorW(nullptr, IDC_ARROW));
    if (d && !d->tracking) {
      d->tracking = true;
      TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
    }
    return 0;
  }
  case WM_MOUSELEAVE:
    if (d)
      d->hover = 0;
    InvalidateRect(h, nullptr, FALSE);
    return 0;
  case WM_LBUTTONUP:
    if (!d)
      return 0;
    if (d->hover == 1)
      ShellExecuteW(h, L"open", URL_GITHUB, nullptr, nullptr, SW_SHOWNORMAL);
    else if (d->hover == 2)
      ShellExecuteW(h, L"open", URL_LICENSE, nullptr, nullptr, SW_SHOWNORMAL);
    else if (d->hover == 3) {
      d->done = true;
      DestroyWindow(h);
    } else if (d->hover == 4) {
      if (Update_State() == UR_FOUND)
        ShellExecuteW(h, L"open", URL_RELEASES, nullptr, nullptr,
                      SW_SHOWNORMAL);
      else
        Update_ManualCheck();
    }
    return 0;
  case WM_DESTROY:
    g_abHwnd = nullptr;
    g_ab = nullptr;
    EnableWindow(g_hwnd, TRUE);
    SetForegroundWindow(g_hwnd);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static void ShowAbout() {
  if (IsWindow(g_abHwnd)) {
    ShowWindow(g_abHwnd, SW_SHOW);
    SetForegroundWindow(g_abHwnd);
    return;
  }
  static AbDlg s_ab;
  s_ab = AbDlg();
  g_ab = &s_ab;
  RECT pr;
  GetWindowRect(g_hwnd, &pr);
  int x = pr.left + (pr.right - pr.left - AB_W) / 2;
  int y = pr.top + (pr.bottom - pr.top - AB_H) / 2;
  g_abHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"AutoOntyAbout",
                             L"About AutoOnty", WS_POPUP | WS_VISIBLE, x, y,
                             AB_W, AB_H, g_hwnd, nullptr, g_hInst, &s_ab);
}

static void RegisterClasses() {
  WNDCLASSW wc = {};
  wc.hInstance = g_hInst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpfnWndProc = MainProc;
  wc.lpszClassName = L"AutoOntyMain";
  RegisterClassW(&wc);
  wc.lpfnWndProc = InputProc;
  wc.lpszClassName = L"AutoOntyInput";
  RegisterClassW(&wc);
  wc.lpfnWndProc = AboutProc;
  wc.lpszClassName = L"AutoOntyAbout";
  RegisterClassW(&wc);
  Macros_RegisterClasses();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
  g_hInst = hInst;
  {
    typedef BOOL(WINAPI * SetProcessDpiAwarenessContext_fn)(HANDLE);
    auto fn = (SetProcessDpiAwarenessContext_fn)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (fn)
      fn((HANDLE)-5);
    else
      SetProcessDPIAware();
  }

  struct CliArgs {
    std::wstring file;
    bool play = false;
    bool rec = false;
    bool min = false;
    bool closeAfter = false;
    int loop = -1;
    double speed = -100.0;
  } cli;

  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv) {
    for (int i = 1; i < argc; i++) {
      std::wstring arg = argv[i];
      if (arg == L"--install" || arg == L"-i") {
        InstallToLocalApp();
        LocalFree(argv);
        return 0;
      } else if (arg == L"--play" || arg == L"-p")
        cli.play = true;
      else if (arg == L"--rec" || arg == L"-r")
        cli.rec = true;
      else if (arg == L"--min" || arg == L"-m" || arg == L"--hide")
        cli.min = true;
      else if (arg == L"--close-after" || arg == L"-c")
        cli.closeAfter = true;
      else if ((arg == L"--loop" || arg == L"-l") && i + 1 < argc) {
        cli.loop = _wtoi(argv[++i]);
      } else if ((arg == L"--speed" || arg == L"-s") && i + 1 < argc) {
        wchar_t *endp = nullptr;
        cli.speed = wcstod(argv[++i], &endp);
      } else if (arg == L"--file" || arg == L"-f") {
        if (i + 1 < argc)
          cli.file = CleanPath(argv[++i]);
      } else if (!arg.empty() && arg[0] != L'-') {
        if (cli.file.empty())
          cli.file = CleanPath(arg);
      }
    }
    LocalFree(argv);
  }

  HANDLE mutex = CreateMutexW(nullptr, FALSE, L"AutoOnty.SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND prev = FindWindowW(L"AutoOntyMain", nullptr);
    if (prev) {
      std::wstring cmdLine = GetCommandLineW();
      COPYDATASTRUCT cds = {};
      cds.dwData = 2;
      cds.cbData = (DWORD)((cmdLine.size() + 1) * sizeof(wchar_t));
      cds.lpData = (PVOID)cmdLine.c_str();
      SendMessageW(prev, WM_COPYDATA, 0, (LPARAM)&cds);
      if (!cli.min) {
        ShowWindow(prev, SW_SHOW);
        SetForegroundWindow(prev);
      }
    }
    CloseHandle(mutex);
    return 0;
  }

  ConfigLoad();
  LangInit();
  GfxInit();
  g_msgTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
  RegisterClasses();
  int x = g_cfg.x, y = g_cfg.y;
  int vx = GetSystemMetrics(SM_XVIRTUALSCREEN),
      vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN),
      vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (x < vx - WIN_W + 20 || x > vx + vw - 20 || y < vy || y > vy + vh - 40) {
    x = vx + (vw - WIN_W) / 2;
    y = vy + (vh - WIN_H) / 3;
  }
  g_hwnd = CreateWindowExW(0, L"AutoOntyMain", L"AutoOnty",
                           WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, x, y, WIN_W,
                           WIN_H, nullptr, nullptr, hInst, nullptr);

  if (!cli.file.empty()) {
    if (Engine_Load(cli.file.c_str())) {
      g_lastFile = cli.file;
      g_dirty = false;
      RecentAdd(cli.file);
      StatusShow(L(L_ST_LOADED), 2500);
      InvalidateRect(g_hwnd, nullptr, FALSE);
    } else {
      StatusShow(L(L_ST_INVALID), 2500);
      UiMessage(g_hwnd, L(L_ST_INVALID), L(L_APPNAME), UI_OK, UI_ICON_ERROR);
    }
  }

  if (cli.closeAfter)
    g_closeAfterPlay = true;
  if (cli.loop >= 0) {
    if (cli.loop == 0)
      g_cfg.continuous = 1;
    else {
      g_cfg.loops = cli.loop;
      g_cfg.continuous = 0;
    }
  }
  if (cli.speed > -90.0) {
    if (cli.speed == 0.25)
      g_cfg.speed = -1;
    else if (cli.speed == 0.5)
      g_cfg.speed = 0;
    else if (cli.speed == 0.75)
      g_cfg.speed = -2;
    else if (cli.speed == 1.0)
      g_cfg.speed = 1;
    else if (cli.speed == 1.25)
      g_cfg.speed = -3;
    else if (cli.speed == 1.5)
      g_cfg.speed = -4;
    else if (cli.speed == 0.0)
      g_cfg.speed = -5;
    else if (cli.speed == 100.0)
      g_cfg.speed = 100;
    else if (cli.speed >= 2 && cli.speed <= 50 &&
             (double)(int)cli.speed == cli.speed)
      g_cfg.speed = (int)cli.speed;
    else {
      g_cfg.speed = 999;
      g_cfg.speedCustom =
          cli.speed < 0.01 ? 0.01 : (cli.speed > 1000.0 ? 1000.0 : cli.speed);
    }
  }

  if (!cli.min)
    ShowWindow(g_hwnd, SW_SHOW);
  else
    ShowWindow(g_hwnd, SW_HIDE);

  Engine_InitRawInput();
  TrayAdd();
  Update_AutoCheck();

  if (cli.play)
    DoCmd(IDM_PLAY);
  else if (cli.rec)
    DoCmd(IDM_REC);

  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  GfxShutdown();
  CloseHandle(mutex);
  return 0;
}
