#pragma once
#include "core/json.h"
enum ImageLibraryMode { IL_AUTOCLICK, IL_MACRO, IL_PICK_MACRO };
struct ImageLibraryChoice {
  bool accepted = false, create = false, start = false;
  MAc image;
};
struct ImageLibraryDialog {
  ImagePendingFiles pending;
  HWND h = nullptr, list = nullptr;
  std::vector<ImageLibraryEntry> entries;
  std::vector<std::wstring> deletedImages;
  unsigned long long selectedMask = 0;
  ImageLibraryMode mode = IL_AUTOCLICK;
  bool done = false;
  ImageLibraryChoice result;
};
static int LibraryImagePrimary(ImageLibraryMode mode) { return mode == IL_MACRO ? 15 : IDOK; }
static RECT LibraryImageButton(int id, ImageLibraryMode mode) {
  if (mode == IL_MACRO) {
    if (id == 15) return {470, 444, 606, 478};
    if (id == IDOK) return {166, 444, 308, 478};
  }
  if (id >= 10 && id <= 12) return {14 + (id - 10) * 198, 390, 202 + (id - 10) * 198, 424};
  if (id == IDCANCEL) return {14, 444, 156, 478};
  if (id == 15) return {166, 444, 308, 478};
  if (id == 14) return {318, 444, 460, 478};
  return {470, 444, 606, 478};
}
static int LibraryImageSelection(const ImageLibraryDialog *d) {
  return (int)SendMessageW(d->list, d->mode == IL_AUTOCLICK ? LB_GETCARETINDEX : LB_GETCURSEL, 0, 0);
}
static void LibraryImageReload(ImageLibraryDialog *d, int selected) {
  SendMessageW(d->list, LB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < d->entries.size(); ++i) {
    const auto &a = d->mode == IL_AUTOCLICK ? d->entries[i].autoclick : d->entries[i].macro;
    auto text = std::to_wstring(i + 1) + L", " + std::to_wstring(a.sensitivity) + L"%";
    SendMessageW(d->list, LB_ADDSTRING, 0, (LPARAM)text.c_str());
  }
  if (!d->entries.empty()) {
    selected = (std::clamp)(selected, 0, (int)d->entries.size() - 1);
    if (d->mode == IL_AUTOCLICK) {
      for (size_t i = 0; i < d->entries.size(); ++i)
        SendMessageW(d->list, LB_SETSEL, (d->selectedMask & (1ULL << i)) != 0, i);
      SendMessageW(d->list, LB_SETCARETINDEX, selected, FALSE);
    } else SendMessageW(d->list, LB_SETCURSEL, selected, 0);
  }
  for (int id : {11, 12, 14}) EnableWindow(GetDlgItem(d->h, id), !d->entries.empty());
  EnableWindow(GetDlgItem(d->h, IDOK), d->mode == IL_AUTOCLICK || !d->entries.empty());
  InvalidateRect(d->h, nullptr, FALSE);
}
static bool LibraryImagesUsedBySavedMacro(const std::vector<std::wstring> &names) {
  if (names.empty()) return false;
  auto directory = ConfigDirectory();
  if (directory.empty()) return false;
  WIN32_FIND_DATAW file{};
  HANDLE search = FindFirstFileW((directory + L"*.onty").c_str(), &file);
  if (search == INVALID_HANDLE_VALUE) return false;
  bool used = false;
  do {
    if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    std::string content;
    if (!json::ReadTextFile((directory + file.cFileName).c_str(), content)) continue;
    for (const auto &name : names) {
      auto filename = json::ToUtf8(name.substr(name.find_last_of(L"/\\") + 1));
      if (content.find(filename) != std::string::npos) { used = true; break; }
    }
    if (used) break;
  } while (FindNextFileW(search, &file));
  FindClose(search);
  return used;
}
static bool LibraryImageSave(ImageLibraryDialog *d) {
  std::vector<std::wstring> deleting;
  for (const auto &name : d->deletedImages) {
    bool stillListed = std::any_of(d->entries.begin(), d->entries.end(), [&](const auto &entry) {
      return image::Name(*entry.macro.image) == name;
    });
    if (stillListed) continue;
    auto references = [&](const auto &self, const MAc &action, unsigned depth) -> bool {
      if (depth > 8) return true;
      if (action.type == A_IMAGE && action.image && image::Name(*action.image) == name) return true;
      if (action.imageMacro)
        for (const auto &child : *action.imageMacro) if (self(self, child, depth + 1)) return true;
      return false;
    };
    for (const auto &entry : d->entries)
      if (references(references, entry.macro, 0) || references(references, entry.autoclick, 0)) {
        UiMessage(d->h, L(L_IMAGE_DELETE_IN_USE), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
        return false;
      }
    deleting.push_back(name);
  }
  if (LibraryImagesUsedBySavedMacro(deleting) &&
      UiMessage(d->h, L(L_IMAGE_DELETE_USED), L(L_IMAGE), UI_YESNO, UI_ICON_WARN) != IDYES) return false;
  if (!ImageLibrary_Save(d->entries)) {
    UiMessage(d->h, L(L_SAVEFAIL), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
    return false;
  }
  for (const auto &entry : d->entries) d->pending.Keep(*entry.macro.image);
  d->selectedMask &= d->entries.size() == 64 ? ~0ULL : (1ULL << d->entries.size()) - 1;
  g_cfg.acImageMask = d->selectedMask;
  if (d->mode == IL_AUTOCLICK) g_cfg.acImagesEnabled = d->selectedMask != 0;
  else if (!d->selectedMask) g_cfg.acImagesEnabled = 0;
  ConfigSave();
  auto directory = ConfigDirectory();
  d->deletedImages.clear();
  for (const auto &name : deleting) {
    if (directory.empty() || !image::SafeName(name)) {
      d->deletedImages.push_back(name);
      continue;
    }
    if (!DeleteFileW((directory + name).c_str())) {
      DWORD error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) d->deletedImages.push_back(name);
    }
  }
  if (!d->deletedImages.empty()) {
    UiMessage(d->h, L(L_IMAGE_DELETE_FAILED), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
    return false;
  }
  return true;
}
static LRESULT CALLBACK ImageLibraryProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  auto d = (ImageLibraryDialog *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_NCCREATE) {
    d = (ImageLibraryDialog *)((CREATESTRUCTW *)l)->lpCreateParams;
    d->h = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
  }
  if (!d) return DefWindowProcW(h, m, w, l);
  bool macro = d->mode != IL_AUTOCLICK;
  switch (m) {
  case WM_CREATE:
    ApplyWindowMaterial(h); GfxRoundRegion(h);
    d->list = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
        LBS_NOTIFY | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT |
        (macro ? 0 : LBS_MULTIPLESEL), 14, 80, 592, 296, h, (HMENU)3, g_hInst, nullptr);
    if (!d->list) return -1;
    SendMessageW(d->list, WM_SETFONT, (WPARAM)ThFont(10), TRUE);
    SendMessageW(d->list, LB_SETITEMHEIGHT, 0, 66);
    ApplyListTheme(d->list);
    for (int id : {10, 11, 12, IDCANCEL, 15, 14, IDOK}) {
      if (id == 15 && !macro) continue;
      if (id == 14 && d->mode != IL_MACRO) continue;
      int labels[] = {L_ADD_IMAGE, L_IMAGE_SETTINGS, L_MC_DELETE};
      int label = id == IDCANCEL ? L_CANCEL : id == 15 ? L_DONE : id == 14 ? L_IMAGE_START :
          id == IDOK ? (d->mode == IL_AUTOCLICK ? L_DONE : d->mode == IL_PICK_MACRO ? L_USE_IMAGE : L_IMAGE_SAVE_ONLY) : labels[id - 10];
      RECT r = LibraryImageButton(id, d->mode);
      if (!CreateWindowW(L"BUTTON", L(label), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
          r.left, r.top, r.right - r.left, r.bottom - r.top, h, (HMENU)(INT_PTR)id, g_hInst, nullptr)) return -1;
    }
    {
      int initial = 0;
      if (!macro && d->selectedMask)
        while (initial < (int)d->entries.size() && !(d->selectedMask & (1ULL << initial))) ++initial;
      LibraryImageReload(d, initial);
    }
    return 0;
  case WM_MEASUREITEM:
    ((MEASUREITEMSTRUCT *)l)->itemHeight = 66;
    return TRUE;
  case WM_DRAWITEM: {
    auto item = (DRAWITEMSTRUCT *)l;
    Gdiplus::Graphics graphics(item->hDC);
    if (item->CtlType == ODT_BUTTON) {
      wchar_t text[128]{}; GetWindowTextW(item->hwndItem, text, 128);
      PaintBtn(graphics, item->hDC, item->rcItem, text, (item->itemState & (ODS_SELECTED | ODS_FOCUS)) != 0,
               item->CtlID == LibraryImagePrimary(d->mode), !(item->itemState & ODS_DISABLED));
      return TRUE;
    }
    if (item->itemID >= d->entries.size()) return TRUE;
    const auto &a = macro ? d->entries[item->itemID].macro : d->entries[item->itemID].autoclick;
    const auto &pixels = *a.image;
    auto r = item->rcItem;
    bool active = macro ? (item->itemState & ODS_SELECTED) != 0 : LibraryImageSelection(d) == (int)item->itemID;
    Gdiplus::SolidBrush background(GdiCol(active ? ThHover() : FieldCol(false)));
    graphics.FillRectangle(&background, (REAL)r.left, (REAL)r.top, (REAL)(r.right-r.left), (REAL)(r.bottom-r.top));
    bool selected = macro ? SendMessageW(d->list, LB_GETCURSEL, 0, 0) == item->itemID :
                           (d->selectedMask & (1ULL << item->itemID)) != 0;
    Gdiplus::Pen border(GdiCol(ThDim()));
    graphics.DrawRectangle(&border, (int)r.left+5, (int)r.top+25, 14, 14);
    if (selected) {
      Gdiplus::Pen tick(GdiCol(ThAccent()), 2);
      graphics.DrawLine(&tick, (int)r.left+8, (int)r.top+32, (int)r.left+11, (int)r.top+36);
      graphics.DrawLine(&tick, (int)r.left+11, (int)r.top+36, (int)r.left+17, (int)r.top+28);
    }
    double scale = (std::min)(52.0/pixels.w, 50.0/pixels.h);
    Gdiplus::Bitmap bitmap(pixels.w, pixels.h, pixels.w*4, PixelFormat32bppRGB, (BYTE *)pixels.rgb.data());
    graphics.DrawImage(&bitmap, (int)r.left+34, (int)r.top+8, (int)(pixels.w*scale), (int)(pixels.h*scale));
    auto title = L"#" + std::to_wstring(item->itemID+1) + L"  " + std::to_wstring(pixels.w) + L" x " +
                 std::to_wstring(pixels.h) + L"  |  " + std::to_wstring(a.sensitivity) + L"%";
    auto detail = macro ? std::wstring(L(a.imageOptions.action == IA_RUN_MACRO ? L_IMAGE_RUN :
        a.imageOptions.action == IA_AUTOCLICK ? L_IMAGE_AUTOCLICK : a.imageOptions.action == IA_CLICK ? L_IMAGE_CLICK : L_IMAGE_CONTINUE)) :
        std::to_wstring(a.imageOptions.interval) + L" ms  |  " + std::to_wstring(a.imageOptions.clicks) + L" " +
        L(L_AC_REPEAT_COUNT) + L"  |  " + ImageTimeoutText(a.imageOptions.autoDuration) + L" s";
    PutText(graphics, item->hDC, title, (REAL)r.left+98, (REAL)r.top+5, (REAL)(r.right-r.left-106), 25, GdiCol(ThText()));
    PutText(graphics, item->hDC, detail, (REAL)r.left+98, (REAL)r.top+31, (REAL)(r.right-r.left-106), 25, GdiCol(ThDim()), 0, 9);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &r);
    return TRUE;
  }
  case WM_CTLCOLORLISTBOX:
    SetTextColor((HDC)w, ThText()); SetBkColor((HDC)w, FieldCol(false));
    return (LRESULT)FieldBrush();
  case WM_COMMAND: {
    int id = LOWORD(w), index = LibraryImageSelection(d);
    if (id == 3 && HIWORD(w) == LBN_SELCHANGE) {
      if (!macro) {
        d->selectedMask = 0;
        for (size_t i=0; i<d->entries.size(); ++i)
          if (SendMessageW(d->list, LB_GETSEL, i, 0)>0) d->selectedMask |= 1ULL << i;
      }
      InvalidateRect(d->list, nullptr, FALSE);
    }
    if (id == 10) {
      if (d->entries.size() >= 64) { UiMessage(h, L(L_AC_IMAGES_LIMIT), L(L_IMAGE), UI_OK, UI_ICON_WARN); return 0; }
      auto pixels = ChooseImage(h, &d->pending);
      if (!pixels) return 0;
      auto name = image::Name(*pixels);
      auto found = std::find_if(d->entries.begin(), d->entries.end(), [&](const auto &entry) {
        return image::Name(*entry.macro.image) == name;
      });
      if (found == d->entries.end()) { d->entries.push_back(ImageLibrary_New(pixels)); index = (int)d->entries.size()-1; }
      else index = (int)(found-d->entries.begin());
      if (!macro) d->selectedMask |= 1ULL << index;
      LibraryImageReload(d, index);
    }
    if ((id == 11 || (id == 3 && HIWORD(w) == LBN_DBLCLK)) && index >= 0 && index < (int)d->entries.size()) {
      auto &entry = d->entries[index];
      MAc value = macro ? entry.macro : entry.autoclick;
      if (EditImage(h, value, false, false, nullptr, !macro, &d->pending)) {
        (macro ? entry.macro : entry.autoclick) = value;
        entry.macro.image = entry.autoclick.image = value.image;
        entry.macro.imageFile = entry.autoclick.imageFile = image::Name(*value.image);
        LibraryImageReload(d, index);
      }
    }
    if (id == 12 && index >= 0 && index < (int)d->entries.size()) {
      d->deletedImages.push_back(image::Name(*d->entries[index].macro.image));
      d->entries.erase(d->entries.begin()+index);
      auto lower = d->selectedMask & ((1ULL << index)-1);
      auto upper = index == 63 ? 0 : (d->selectedMask >> (index+1)) << index;
      d->selectedMask = lower | upper;
      LibraryImageReload(d, index);
    }
    if (id == IDOK || id == 14 || id == 15) {
      if (macro && id != 15 && (index < 0 || index >= (int)d->entries.size())) return 0;
      if (macro && id != 15 && !ImageMacroReady(d->entries[index].macro)) {
        UiMessage(h, L(L_IMAGE_MACRO_ERROR), L(L_IMAGE), UI_OK, UI_ICON_ERROR);
        return 0;
      }
      if (!LibraryImageSave(d)) return 0;
      d->result.accepted = true;
      if (macro && id != 15) {
        d->result.image = d->entries[index].macro;
        d->result.create = true;
        d->result.start = id == 14;
      }
      SendMessageW(h, WM_CLOSE, 0, 0);
    }
    if (id == IDCANCEL) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  }
  case WM_NCHITTEST: {
    POINT point{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(h, &point);
    auto close = GfxCloseRect(620);
    return point.y < 30 && !PtInRect(&close, point) ? HTCAPTION : HTCLIENT;
  }
  case WM_LBUTTONUP: {
    POINT point{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    auto close = GfxCloseRect(620);
    if (PtInRect(&close, point)) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  }
  case WM_KEYDOWN:
    if (w == VK_ESCAPE) SendMessageW(h, WM_CLOSE, 0, 0);
    return 0;
  case DM_GETDEFID: return MAKELONG(LibraryImagePrimary(d->mode), DC_HASDEFID);
  case WM_PAINT:
    PaintBuffered(h, [&](HDC dc, Gdiplus::Graphics &graphics, const RECT &) {
      GfxChrome(graphics, dc, 620, L(macro ? L_IMAGE_MACRO : L_AC_IMAGES), 0, CHB_SEP);
      PutText(graphics, dc, L(L_IMAGE_LIBRARY_HINT), 14, 40, 592, 35, GdiCol(ThDim()), 0, 9);
    });
    return 0;
  case WM_CLOSE: d->done = true; DestroyWindow(h); return 0;
  case WM_DESTROY: d->done = true; return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static ImageLibraryChoice ShowImageLibrary(HWND owner, ImageLibraryMode mode) {
  if (g_state.load() != ST_IDLE || g_acRunning.load()) return {};
  ModalGuard guard;
  if (!ImageLibrary_Load()) { UiMessage(owner, L(L_AC_IMAGES_LOAD_ERROR), L(L_IMAGE), UI_OK, UI_ICON_ERROR); return {}; }
  ImageLibraryDialog d;
  d.entries = g_imageLibrary; d.mode = mode;
  d.selectedMask = g_cfg.acImageMask & (d.entries.size()==64 ? ~0ULL : (1ULL << d.entries.size())-1);
  WNDCLASSW wc{}; wc.lpfnWndProc=ImageLibraryProc; wc.hInstance=g_hInst;
  wc.lpszClassName=L"AutoOntyImageLibrary"; wc.hCursor=LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  int x,y; PlaceOver(owner,620,492,x,y);
  EnableWindow(owner,FALSE);
  HWND h=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP|WS_CLIPCHILDREN,x,y,620,492,owner,nullptr,g_hInst,&d);
  if(h) { ShowWindow(h,SW_SHOW); SetForegroundWindow(h); SetFocus(d.list); RunModal(h,d.done); }
  EnableWindow(owner,TRUE); SetForegroundWindow(owner);
  return d.result;
}
void Macros_ShowAutoclickImages(HWND owner) { ShowImageLibrary(owner, IL_AUTOCLICK); }

static std::vector<MAc> ImageMacroSteps(MAc value) {
  if (!ImageMacroReady(value)) return {};
  std::vector<MAc> events;
  if (value.imageOptions.action == IA_RUN_MACRO && value.imageMacro) {
    events = *value.imageMacro;
    value.imageOptions.action = IA_WAIT;
    value.imageOptions.repeat = IR_ONCE;
    value.imageOptions.onFound = IO_CONTINUE;
    value.imageOptions.onTimeout = IO_STOP;
  }
  value.imageMacro.reset();
  value.imageMacroName.clear();
  events.insert(events.begin(), value);
  size_t count = 0;
  for (const auto &event : events) {
    count += EdActionCount(event);
    if (count > MAX_EDIT_EVENTS) return {};
  }
  return events;
}
