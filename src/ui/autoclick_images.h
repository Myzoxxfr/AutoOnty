#pragma once

struct AutoclickImagesDialog {
  HWND h = nullptr, list = nullptr;
  std::vector<MAc> images;
  unsigned long long selectedMask = 0;
  bool done = false, selecting = false, ok = false;
};
static RECT AcImageButton(int id) {
  if (id == IDOK) return {466, 444, 606, 478};
  if (id == IDCANCEL) return {14, 444, 144, 478};
  return {14 + (id - 10) * 198, 390, 202 + (id - 10) * 198, 424};
}
static void AcImagesReload(AutoclickImagesDialog *d, int selected) {
  SendMessageW(d->list, LB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < d->images.size(); ++i) {
    const auto &o = d->images[i].imageOptions;
    auto label = std::to_wstring(i + 1) + L", " + std::to_wstring(o.interval) +
                 L" ms, " + std::to_wstring(o.clicks) + L" " + L(L_AC_REPEAT_COUNT) +
                 L", " + ImageTimeoutText(o.autoDuration) + L" s";
    SendMessageW(d->list, LB_ADDSTRING, 0, (LPARAM)label.c_str());
  }
  if (d->selecting) {
    for (size_t i = 0; i < d->images.size(); ++i)
      SendMessageW(d->list, LB_SETSEL, (d->selectedMask & (1ULL << i)) != 0, i);
  } else if (!d->images.empty())
    SendMessageW(d->list, LB_SETCURSEL,
                 (std::min)(selected, (int)d->images.size() - 1), 0);
  EnableWindow(GetDlgItem(d->h, 11), !d->images.empty());
  EnableWindow(GetDlgItem(d->h, 12), !d->images.empty());
  InvalidateRect(d->h, nullptr, FALSE);
}
static LRESULT CALLBACK AcImagesProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto d = (AutoclickImagesDialog *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_NCCREATE) {
    d = (AutoclickImagesDialog *)((CREATESTRUCTW *)l)->lpCreateParams;
    d->h = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
  }
  if (!d) return DefWindowProcW(h, m, w, l);
  auto selection = [&] { return (int)SendMessageW(d->list, LB_GETCURSEL, 0, 0); };
  auto edit = [&](bool add) {
    int index = selection();
    if (!add && (index < 0 || index >= (int)d->images.size())) return;
    if (add && d->images.size() >= 64) {
      UiMessage(h, L(L_AC_IMAGES_LIMIT), L(L_IMAGE), UI_OK, UI_ICON_WARN);
      return;
    }
    MAc value;
    if (add) {
      value.type = A_IMAGE;
      value.imageTimeout = 0;
      value.imageOptions.action = IA_AUTOCLICK;
      value.imageOptions.clicks = 0;
      value.imageOptions.interval = Autoclick_IntervalMs();
      value.imageOptions.standalone = true;
    } else value = d->images[index];
    if (!EditImage(h, value, add, false, nullptr, true)) return;
    if (add) { index = (int)d->images.size(); d->images.push_back(value); }
    else d->images[index] = value;
    AcImagesReload(d, index);
  };
  switch (m) {
  case WM_CREATE:
    ApplyWindowMaterial(h);
    GfxRoundRegion(h);
    d->list = CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY |
        LBS_HASSTRINGS | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT |
        (d->selecting ? LBS_MULTIPLESEL : 0),
        14, 80, 592, 296, h, (HMENU)3, g_hInst, nullptr);
    if (!d->list) return -1;
    SendMessageW(d->list, WM_SETFONT, (WPARAM)ThFont(10), TRUE);
    SendMessageW(d->list, LB_SETITEMHEIGHT, 0, 66);
    ApplyListTheme(d->list);
    for (int id : {10, 11, 12, IDOK, IDCANCEL}) {
      if (d->selecting && id >= 10) continue;
      auto r = AcImageButton(id);
      int labels[] = {L_ADD_IMAGE, L_IMAGE_SETTINGS, L_MC_DELETE};
      auto button = CreateWindowW(L"BUTTON",
          L(id == IDOK ? (d->selecting ? L_OK : L_SAVE)
                       : id == IDCANCEL ? L_CANCEL : labels[id - 10]),
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
          r.left, r.top, r.right - r.left, r.bottom - r.top,
          h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
      if (!button) return -1;
    }
    AcImagesReload(d, 0);
    return 0;
  case WM_MEASUREITEM:
    ((MEASUREITEMSTRUCT *)l)->itemHeight = 66;
    return TRUE;
  case WM_DRAWITEM: {
    auto item = (DRAWITEMSTRUCT *)l;
    if (item->CtlType == ODT_BUTTON) {
      Gdiplus::Graphics g(item->hDC);
      wchar_t text[128]{};
      GetWindowTextW(item->hwndItem, text, 128);
      PaintBtn(g, item->hDC, item->rcItem, text,
               (item->itemState & (ODS_SELECTED | ODS_FOCUS)) != 0,
               item->CtlID == IDOK, !(item->itemState & ODS_DISABLED));
      if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
      return TRUE;
    }
    if (item->itemID >= d->images.size()) return TRUE;
    Gdiplus::Graphics g(item->hDC);
    auto r = item->rcItem;
    Gdiplus::SolidBrush bg(GdiCol(item->itemState & ODS_SELECTED ? ThHover() : FieldCol(false)));
    g.FillRectangle(&bg, (REAL)r.left, (REAL)r.top,
                    (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    const auto &a = d->images[item->itemID];
    const auto &p = *a.image;
    int inset = d->selecting ? 26 : 0;
    if (d->selecting) {
      Gdiplus::Pen border(GdiCol(ThDim()));
      g.DrawRectangle(&border, (int)r.left + 5, (int)r.top + 25, 14, 14);
      if (SendMessageW(d->list, LB_GETSEL, item->itemID, 0) > 0) {
        Gdiplus::Pen tick(GdiCol(ThAccent()), 2);
        g.DrawLine(&tick, (int)r.left + 8, (int)r.top + 32, (int)r.left + 11, (int)r.top + 36);
        g.DrawLine(&tick, (int)r.left + 11, (int)r.top + 36, (int)r.left + 17, (int)r.top + 28);
      }
    }
    double scale = (std::min)(52.0 / p.w, 50.0 / p.h);
    int width = (int)(p.w * scale), height = (int)(p.h * scale);
    Gdiplus::Bitmap bitmap(p.w, p.h, p.w * 4, PixelFormat32bppRGB, (BYTE *)p.rgb.data());
    g.DrawImage(&bitmap, (int)r.left + 8 + inset, (int)r.top + 8, width, height);
    const auto &o = a.imageOptions;
    auto title = L"#" + std::to_wstring(item->itemID + 1) + L"  " +
        std::to_wstring(p.w) + L" x " + std::to_wstring(p.h) + L"  |  " +
        std::to_wstring(a.sensitivity) + L"%";
    auto detail = std::to_wstring(o.interval) + L" ms  |  " +
        std::to_wstring(o.clicks) + L" " + L(L_AC_REPEAT_COUNT) + L"  |  " +
        ImageTimeoutText(o.autoDuration) + L" s";
    PutText(g, item->hDC, title, (REAL)r.left + 72 + inset, (REAL)r.top + 5,
            (REAL)(r.right - r.left - 80 - inset), 25, GdiCol(ThText()));
    PutText(g, item->hDC, detail, (REAL)r.left + 72 + inset, (REAL)r.top + 31,
            (REAL)(r.right - r.left - 80 - inset), 25, GdiCol(ThDim()), 0, 9);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &r);
    return TRUE;
  }
  case WM_CTLCOLORLISTBOX:
    SetTextColor((HDC)w, ThText()); SetBkColor((HDC)w, FieldCol(false));
    return (LRESULT)FieldBrush();
  case WM_COMMAND: {
    int id = LOWORD(w);
    if (id == 3 && HIWORD(w) == LBN_SELCHANGE) InvalidateRect(d->list, nullptr, FALSE);
    if (id == 3 && HIWORD(w) == LBN_DBLCLK && !d->selecting) edit(false);
    if (id == 10) edit(true);
    if (id == 11) edit(false);
    if (id == 12 && selection() >= 0) {
      int index = selection();
      d->images.erase(d->images.begin() + index);
      auto lower = d->selectedMask & ((1ULL << index) - 1);
      auto upper = index == 63 ? 0 : (d->selectedMask >> (index + 1)) << index;
      d->selectedMask = lower | upper;
      AcImagesReload(d, index);
    }
    if (id == IDOK) {
      if (d->selecting) {
        unsigned long long chosen = 0;
        for (size_t i = 0; i < d->images.size(); ++i)
          if (SendMessageW(d->list, LB_GETSEL, i, 0) > 0) chosen |= 1ULL << i;
        if (!chosen) {
          UiMessage(h, L(L_AC_IMAGES_SELECT_EMPTY), L(L_IMAGE), UI_OK, UI_ICON_WARN);
          return 0;
        }
        d->selectedMask = chosen;
      } else {
        if (!Autoclick_SaveImages(d->images)) {
          UiMessage(h, L(L_SAVEFAIL), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
          return 0;
        }
        g_cfg.acImageMask = d->selectedMask;
        if (!Autoclick_HasSelectedImages()) g_cfg.acImagesEnabled = 0;
        ConfigSave();
      }
      d->ok = true;
      SendMessageW(h, WM_CLOSE, 0, 0);
    }
    if (id == IDCANCEL) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  }
  case WM_NCHITTEST: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    ScreenToClient(h, &p);
    auto close = GfxCloseRect(620);
    return p.y < 30 && !PtInRect(&close, p) ? HTCAPTION : HTCLIENT;
  }
  case WM_LBUTTONUP: {
    POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    auto close = GfxCloseRect(620);
    if (PtInRect(&close, p)) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  }
  case WM_KEYDOWN:
    if (w == VK_ESCAPE) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  case DM_GETDEFID:
    return MAKELONG(IDOK, DC_HASDEFID);
  case WM_PAINT:
    PaintBuffered(h, [&](HDC dc, Gdiplus::Graphics &g, const RECT &) {
      GfxChrome(g, dc, 620, L(d->selecting ? L_AC_IMAGES_SELECT : L_AC_IMAGES), 0, CHB_SEP);
      PutText(g, dc, L(d->selecting ? L_AC_IMAGES_SELECT_HINT : L_AC_IMAGES_HINT),
              14, 40, 592, 30, GdiCol(ThDim()), 0, 9);
    });
    return 0;
  case WM_CLOSE:
    d->done = true; DestroyWindow(h); return 0;
  case WM_DESTROY:
    d->done = true; return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static bool ShowAutoclickImages(HWND owner, bool selecting) {
  if (g_state.load() != ST_IDLE || g_acRunning.load()) return false;
  ModalGuard guard;
  if (!Autoclick_LoadImages()) {
    UiMessage(owner, L(L_AC_IMAGES_LOAD_ERROR), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
    return false;
  }
  if (selecting && g_acImages.empty()) {
    UiMessage(owner, L(L_AC_IMAGES_EMPTY), L(L_IMAGE), UI_OK, UI_ICON_WARN);
    return false;
  }
  AutoclickImagesDialog d;
  d.images = g_acImages;
  d.selecting = selecting;
  d.selectedMask = g_cfg.acImageMask &
      (d.images.size() == 64 ? ~0ULL : (1ULL << d.images.size()) - 1);
  WNDCLASSW wc{};
  wc.lpfnWndProc = AcImagesProc;
  wc.hInstance = g_hInst;
  wc.lpszClassName = L"AutoOntyAutoclickImages";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  int x, y;
  PlaceOver(owner, 620, 492, x, y);
  EnableWindow(owner, FALSE);
  HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP | WS_CLIPCHILDREN,
                           x, y, 620, 492, owner, nullptr, g_hInst, &d);
  if (h) {
    ShowWindow(h, SW_SHOW); SetForegroundWindow(h); SetFocus(d.list);
    MSG message;
    while (!d.done && IsWindow(h)) {
      BOOL result = GetMessageW(&message, nullptr, 0, 0);
      if (result <= 0) {
        if (!result) PostQuitMessage((int)message.wParam);
        DestroyWindow(h);
        break;
      }
      if (!IsDialogMessageW(h, &message)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
  }
  EnableWindow(owner, TRUE); SetForegroundWindow(owner);
  if (selecting && d.ok) g_cfg.acImageMask = d.selectedMask;
  return d.ok;
}
void Macros_ShowAutoclickImages(HWND owner) { ShowAutoclickImages(owner, false); }
bool Macros_SelectAutoclickImages(HWND owner) { return ShowAutoclickImages(owner, true); }
