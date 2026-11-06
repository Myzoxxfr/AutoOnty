#include <vector>
#include <windows.h>
static UINT snipSent = 6;
static std::vector<INPUT> snipCleanup;
static UINT WINAPI CaptureSnipInput(UINT count, LPINPUT input, int) {
  if (count == 6) {
    snipCleanup.clear();
    return snipSent;
  }
  snipCleanup.assign(input, input + count);
  return count;
}
#define SendInput CaptureSnipInput
#include "../src/ui/macros.cpp"
#undef SendInput
#include "storage_paths.h"
#define SHGetFolderPathW TestDocumentsFolder
#include "../src/core/config.cpp"
#undef SHGetFolderPathW
#include <cassert>
#include <cstdio>
std::wstring g_lastFile;
bool g_dirty = false;

// Answer only modal dialogs owned by this isolated editor; never inject input.
static HWND modalOwner;
static const wchar_t *modalValue = L"0";
static bool modalCancel = false;
static bool answerImagePicker = false;
static unsigned long long modalImageMask = 0;
static int modalMouseButton = -1, modalAnswers = 0;
static UINT_PTR modalTimer = 0;
static DWORD modalStarted;
static BOOL CALLBACK AnswerModal(HWND h, LPARAM) {
  if (GetWindow(h, GW_OWNER) != modalOwner)
    return TRUE;
  wchar_t cls[64]{};
  GetClassNameW(h, cls, 64);
  if (wcscmp(cls, L"AutoOntyPrompt") == 0) {
    auto *prompt = (PromptDlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
    assert(prompt && prompt->edit);
    SetWindowTextW(prompt->edit, modalValue);
    PostMessageW(h, modalCancel ? PM_CANCEL : PM_OK, 0, 0);
  } else if (wcscmp(cls, L"AutoOntyKeyCap") == 0 && modalMouseButton >= 0) {
    assert(g_keyMouseCapture && g_keyMouseCapture->h == h);
    const UINT downs[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN,
                          WM_XBUTTONDOWN, WM_XBUTTONDOWN};
    const UINT ups[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP,
                        WM_XBUTTONUP};
    MSLLHOOKSTRUCT mouse{};
    mouse.pt = {-1234, 567};
    mouse.mouseData =
        modalMouseButton == 3 ? (XBUTTON1 << 16) : (XBUTTON2 << 16);
    assert(KeyMouseProc(HC_ACTION, downs[modalMouseButton], (LPARAM)&mouse) ==
           1);
    mouse.pt = {300,
                400}; // Release elsewhere must not change the capture position.
    assert(KeyMouseProc(HC_ACTION, ups[modalMouseButton], (LPARAM)&mouse) == 1);
  } else if (answerImagePicker && wcscmp(cls, L"AutoOntyAutoclickImages") == 0) {
    auto *picker = (AutoclickImagesDialog *)GetWindowLongPtrW(h, GWLP_USERDATA);
    assert(picker && picker->selecting && !picker->done);
    for (size_t i = 0; i < picker->images.size(); ++i)
      SendMessageW(picker->list, LB_SETSEL, (modalImageMask & (1ULL << i)) != 0, i);
    PostMessageW(h, WM_COMMAND, modalCancel ? IDCANCEL : IDOK, 0);
  } else {
    return TRUE;
  }
  ++modalAnswers;
  KillTimer(nullptr, modalTimer);
  modalTimer = 0;
  return FALSE;
}
static void CALLBACK AnswerModalTimer(HWND, UINT, UINT_PTR, DWORD) {
  assert(GetTickCount() - modalStarted < 5000);
  EnumThreadWindows(GetCurrentThreadId(), AnswerModal, 0);
}
static void PrepareModal(HWND owner, const wchar_t *value, bool cancel = false,
                         int mouseButton = -1) {
  modalOwner = owner;
  modalValue = value;
  modalCancel = cancel;
  modalMouseButton = mouseButton;
  modalAnswers = 0;
  modalStarted = GetTickCount();
  modalTimer = SetTimer(nullptr, 0, 10, AnswerModalTimer);
  assert(modalTimer);
}

int main() {
  _set_error_mode(_OUT_TO_STDERR);
  setvbuf(stdout, nullptr, _IONBF, 0);
  g_hInst = GetModuleHandleW(nullptr);
  g_cfg.lang = 1;
  g_cfg.theme = 0;
  g_cfg.statusbarShow = 0; // Tests never display the status overlay.
  g_cfg.speed = 1;
  g_cfg.speedCustom = 1;
  g_cfg.loops = 1;
  g_cfg.edKeys = g_cfg.edMoves = g_cfg.edLeft = 1;
  INITCOMMONCONTROLSEX common = {sizeof(common), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&common);
  LangInit();
  GfxInit();
  Macros_RegisterClasses();
  EdDlg d;
  d.it = {MakeKey(A_KD, 'A', false), MakeDelay(100),
          MakeKey(A_KU, 'A', false), MakeDelay(200),
          MakeKey(A_KD, 'B', false), MakeDelay(300),
          MakeKey(A_KU, 'B', false)};
  // A hidden, isolated editor exercises native list selection and layout
  // without interacting with the user's running application or macro files.
  HWND editor =
      CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyEditor",
                      L"AutoOnty editor tests", WS_POPUP | WS_CLIPCHILDREN, 500,
                      350, EDT_W, EDT_H, nullptr, nullptr, g_hInst, &d);
  assert(editor && d.lb);
  g_hwnd = editor;
  d.prefs.exclude.keys['A'] = true;
  assert(EdKeyText(d.it[0].a, d.prefs).find(L(L_EXCLUDED)) !=
         std::wstring::npos);
  assert(EdKeyText(d.it[2].a, d.prefs).find(L(L_EXCLUDED)) !=
         std::wstring::npos);
  d.prefs.exclude = {};
  assert(EdKeyText(d.it[0].a, d.prefs).find(L(L_EXCLUDED)) ==
         std::wstring::npos);

  RECT add = EdBtn(&d, EB_ADD), del = EdBtn(&d, EB_DEL);
  POINT oldAdd{add.left, add.top}, oldDel{del.left, del.top};
  ClientToScreen(editor, &oldAdd);
  ClientToScreen(editor, &oldDel);
  EdToggleMode(&d);
  assert(d.editMode);
  add = EdBtn(&d, EB_ADD);
  del = EdBtn(&d, EB_DEL);
  POINT newAdd{add.left, add.top}, newDel{del.left, del.top};
  ClientToScreen(editor, &newAdd);
  ClientToScreen(editor, &newDel);
  assert(oldAdd.x == newAdd.x && oldAdd.y == newAdd.y);
  assert(oldDel.x == newDel.x && oldDel.y == newDel.y);
  RECT list;
  GetWindowRect(d.lb, &list);
  assert(list.top == 350 + EDT_LY + 1);
  // All panel controls fit, with no overlap or collision with the list.
  for (int i = 0; i < EP_COUNT; ++i) {
    RECT r = EdPanelRect(i);
    assert(r.left >= 0 && r.right <= EDT_W && r.bottom <= EDT_LY + ED_PANEL_H);
    for (int k = i + 1; k < EP_COUNT; ++k) {
      RECT other = EdPanelRect(k), overlap;
      assert(!IntersectRect(&overlap, &r, &other));
    }
  }
  SendMessageW(d.lb, LB_SETSEL, TRUE, 0);
  SendMessageW(d.lb, LB_SETSEL, TRUE, 2);
  assert((EdSel(&d) == std::vector<int>{0, 2}));
  SendMessageW(d.lb, LB_SETSEL, FALSE, 0); // Ctrl-click removal semantic.
  assert((EdSel(&d) == std::vector<int>{2}));
  d.dragStart = {10, 5};
  d.dragPoint = {200, 75};
  d.dragStartY = 5;
  EdDragUpdate(&d);
  assert((EdSel(&d) == std::vector<int>{0, 1, 2, 3}));
  d.dragCtrl = true;
  d.dragBase = {0, 6};
  EdDragUpdate(&d);
  assert((EdSel(&d) == std::vector<int>{1, 2, 3, 6}));
  // Bulk edits and undo/redo restore rows and selection as one operation.
  EdPush(&d);
  macroedit::UniversalDelay(d.it, EdSel(&d), 42);
  EdUndo(&d);
  assert(d.it[1].ms == 100 && d.it[3].ms == 200);
  assert((EdSel(&d) == std::vector<int>{1, 2, 3, 6}));
  EdRedo(&d);
  assert(d.it[1].ms == 42 && d.it[3].ms == 42);
  // Exercise the actual universal-delay command with actions selected, not
  // delays.
  EdReload(&d, {2, 6}, 0);
  PrepareModal(editor, L"87");
  EdUniversal(&d);
  assert(modalAnswers == 1 && d.it[1].ms == 87 && d.it[5].ms == 87);
  assert(d.it[3].ms == 42);
  EdUndo(&d);
  assert(d.it[1].ms == 42 && d.it[5].ms == 42);
  EdRedo(&d);
  assert(d.it[1].ms == 87 && d.it[5].ms == 87);
  EdReload(&d, {}, 0);
  PrepareModal(editor, L"25");
  EdUniversal(&d);
  assert(d.it[1].ms == 25 && d.it[3].ms == 25 && d.it[5].ms == 25);

  // Add key accepts all five physical mouse buttons through the capture hook.
  d.prefs.customVisibility = true;
  d.prefs.visibility = d.visibilityMask = EV_ALL;
  for (int button = 0; button < 5; ++button) {
    size_t before = d.it.size();
    EdReload(&d, {}, 0);
    PrepareModal(editor, L"", false, button);
    EdAddKey(&d);
    assert(modalAnswers == 1 && !g_keyMouseCapture &&
           d.it.size() == before + 4);
    const auto &press = d.it[before + 1].a;
    const auto &release = d.it[before + 3].a;
    assert(press.type == A_MD && release.type == A_MU);
    assert(press.btn == button && release.btn == button);
    assert(press.x == -1234 && press.y == 567 && release.x == -1234 &&
           release.y == 567);
    EdUndo(&d);
    assert(d.it.size() == before);
    EdRedo(&d);
    assert(d.it.size() == before + 4);
  }
  // A filtered-out mouse button is stored without forcing its rows visible.
  d.prefs.visibility = d.visibilityMask = EV_KEYS;
  macroedit::Reproject(d.it, d.tail, d.dragBase, EV_KEYS);
  EdReload(&d, {}, 0);
  size_t eventCount = EdToEvents(d.it, d.tail).size();
  PrepareModal(editor, L"", false, 4);
  EdAddKey(&d);
  auto withHiddenClick = EdToEvents(d.it, d.tail);
  assert(withHiddenClick.size() == eventCount + 2);
  assert(withHiddenClick.back().type == A_MU &&
         withHiddenClick.back().btn == 4);
  for (const auto &item : d.it)
    assert(item.isDelay || item.a.type == A_KD || item.a.type == A_KU);

  // Keyboard-only capture dialogs must not consume mouse input.
  KeyDlg keyboardOnly;
  MSLLHOOKSTRUCT mouse{};
  assert(!CaptureMouseButton(&keyboardOnly, WM_LBUTTONDOWN, mouse));
  keyboardOnly.allowMouse = true;
  mouse.flags = LLMHF_INJECTED;
  assert(!CaptureMouseButton(&keyboardOnly, WM_LBUTTONDOWN, mouse));
  auto keyTap = EdInputTap('Z', true, MakeKey(A_KD, 'Z', true).a);
  assert(keyTap[1].a.type == A_KD && keyTap[3].a.type == A_KU &&
         keyTap[1].a.vk == 'Z' && keyTap[3].a.flags == KEYEVENTF_EXTENDEDKEY);

  // Visibility changes preserve content and history rather than clearing undo.
  MAc keyDown;
  keyDown.type = A_KD;
  keyDown.vk = 'A';
  MAc motion;
  motion.type = A_MOVE;
  motion.delay = 20;
  motion.path = {{2, 3}};
  motion.pdelays = {30};
  MAc keyUp;
  keyUp.type = A_KU;
  keyUp.vk = 'A';
  keyUp.delay = 50;
  d.it = EdFromEvents({keyDown, motion, keyUp}, d.tail, EV_KEYS);
  d.undo.clear();
  d.redo.clear();
  d.visibilityMask = EV_KEYS;
  d.prefs.customVisibility = true;
  d.prefs.visibility = EV_KEYS;
  EdReload(&d, {0, 2}, 0);
  EdPush(&d);
  d.it[0].a.vk = 'B';
  d.prefs.visibility = EV_ALL;
  EdApplyVisibility(&d);
  assert(EdToEvents(d.it, d.tail).size() == 3 && d.it.size() == 5);
  assert((EdSel(&d) == std::vector<int>{0, 4}));
  EdUndo(&d);
  assert(d.it[0].a.vk == 'A' && d.it.size() == 5);
  EdRedo(&d);
  assert(d.it[0].a.vk == 'B' && d.it.size() == 5);
  g_cfg.edMoves = 0;
  d.prefs.customVisibility = false;
  EdApplyVisibility(&d);
  assert(d.it.size() == 3 && EdToEvents(d.it, d.tail)[1].path == motion.path);
  g_cfg.edMoves = 1;
  EdApplyVisibility(&d);
  assert(d.it.size() == 5);
  d.prefs.customVisibility = true;
  d.prefs.visibility = EV_KEYS;
  EdApplyVisibility(&d);
  assert(d.it.size() == 3);
  g_cfg.edMoves = 0;
  EdApplyVisibility(&d);
  assert(d.it.size() == 3);
  g_cfg.edMoves = 1;
  EdApplyVisibility(&d);
  assert(d.it.size() == 3);
  d.prefs.customVisibility = false;
  EdApplyVisibility(&d);
  assert(d.it.size() == 5);

  // Search/checklist controls use real hidden native windows, isolated from
  // the user's application and settings. Selection is independent of filtering.
  CheckDlg options;
  options.owner = editor;
  CheckRows(&options);
  HWND checklist =
      CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyEditorOptions",
                      L"Checklist tests", WS_POPUP | WS_CLIPCHILDREN, 100, 100,
                      CK_W, CK_H, editor, nullptr, g_hInst, &options);
  assert(checklist &&
         options.visible.size() == PreviewStandardKeys().size() + 16);
  for (const auto &row : options.rows)
    assert(CheckState(&options, row.id) == 0);
  auto listedKey = [&](unsigned key) {
    for (const auto &row : options.rows)
      if (row.id == 32 + (int)key)
        return true;
    return false;
  };
  assert(listedKey('A') && listedKey(VK_F12) && listedKey(VK_NUMPAD9));
  assert(listedKey(VK_OEM_102) && listedKey(VK_LCONTROL));
  assert(!listedKey(VK_F13) && !listedKey(VK_PACKET) &&
         !listedKey(VK_BROWSER_BACK));
  auto letter = std::find(PreviewStandardKeys().begin(),
                          PreviewStandardKeys().end(), 'A');
  auto functionKey = std::find(PreviewStandardKeys().begin(),
                               PreviewStandardKeys().end(), VK_F1);
  assert(letter < functionKey);
  for (const auto &row : options.rows)
    assert(row.label.find(L"[VK") == std::wstring::npos);
  assert(std::wstring(EdPanelText(EP_EXCLUSIONS)) == L(L_EXCLUSIONS));

  SetWindowTextW(options.search, L"Right Ctrl");
  assert(options.visible.size() == 1);
  CheckToggle(&options, 0);
  assert(options.prefs.exclude.KeyBlocked(VK_RCONTROL) &&
         !options.prefs.exclude.KeyBlocked(VK_LCONTROL));
  const auto &checkedRow = options.rows[(size_t)options.visible[0]];
  assert(CheckRowText(&options, checkedRow).find(L(L_EXCLUDED)) !=
         std::wstring::npos);
  wchar_t accessibleLabel[256];
  SendMessageW(options.list, LB_GETTEXT, 0, (LPARAM)accessibleLabel);
  assert(std::wstring(accessibleLabel).find(L(L_EXCLUDED)) !=
         std::wstring::npos);
  CheckToggle(&options, 0);
  assert(CheckRowText(&options, checkedRow).find(L(L_EXCLUDED)) ==
         std::wstring::npos);

  CheckSet(&options, CK_ALLKEYS, true);
  assert(CheckState(&options, CK_ALLKEYS) == 1);
  assert(options.prefs.exclude.KeyBlocked(VK_CANCEL));
  CheckRun(&options,
           1); // Clear all acts on the complete list, not only matches.
  assert(options.prefs.exclude.Empty());
  CheckRun(&options, 0);
  assert(options.prefs.exclude.mouse == 1023 &&
         options.prefs.exclude.wheel == 15 && options.prefs.exclude.movement);
  SetWindowTextW(options.search, L"no matching key name");
  assert(options.visible.empty());
  SetWindowTextW(options.search, L"");
  assert(options.visible.size() == PreviewStandardKeys().size() + 16);
  CheckRun(&options, 1);
  DestroyWindow(checklist);
  CheckDlg visibility;
  visibility.owner = editor;
  visibility.visibility = true;
  visibility.defaults = EV_KEYS;
  visibility.prefs.visibility = EV_KEYS;
  CheckRows(&visibility);
  checklist =
      CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoOntyEditorOptions",
                      L"Visibility tests", WS_POPUP | WS_CLIPCHILDREN, 100, 100,
                      CK_W, CK_H, editor, nullptr, g_hInst, &visibility);
  assert(checklist && visibility.visible.size() == 11);
  CheckSet(&visibility, 10, true);
  assert(visibility.prefs.customVisibility &&
         (visibility.prefs.visibility & EV_MOVES));
  CheckRun(&visibility, 2);
  assert(!visibility.prefs.customVisibility &&
         visibility.prefs.visibility == EV_KEYS);
  DestroyWindow(checklist);
  EdToggleMode(&d);
  assert(!d.editMode);
  RECT actual;
  GetWindowRect(editor, &actual);
  assert(actual.top == 350 && actual.bottom - actual.top == EDT_H);
  // A preview uses only a harmless timing action and never replaces g_events.
  d.it.assign(1, EdItem());
  d.it[0].a.type = A_WAIT;
  EdReload(&d, {0}, 0);
  g_events.assign(3, MAc());
  g_cfg.playCountdown = 7;
  PrepareModal(editor, L"1", true);
  EdPlaySelection(&d);
  assert(modalAnswers == 1 && !d.preview && g_state == ST_IDLE);
  PrepareModal(editor, L"1");
  EdPlaySelection(&d);
  assert(modalAnswers == 1 && g_playCountdown == 1 && g_cfg.playCountdown == 7);
  assert(d.preview && !IsWindowEnabled(d.lb));
  assert(!EdEnabled(&d, EB_DEL) && EdEnabled(&d, EB_PLAY));
  size_t count = d.it.size();
  EdRunButton(&d, EB_DEL);
  assert(d.it.size() == count);
  // The same button stops immediately without asking for another countdown.
  EdPlaySelection(&d);
  assert(modalAnswers == 1);
  Engine_Shutdown();
  SendMessageW(editor, WM_TIMER, 7, 0);
  assert(!d.preview && IsWindowEnabled(d.lb) && g_events.size() == 3);
  PrepareModal(editor, L"0");
  EdPlaySelection(&d);
  assert(modalAnswers == 1 && g_cfg.playCountdown == 7 && g_playCountdown == 0);
  Engine_Shutdown();
  SendMessageW(editor, WM_TIMER, 7, 0);
  // Failed shortcut insertion releases exactly the injected, unreleased keys.
  for (snipSent = 0; snipSent <= 6; ++snipSent) {
    assert(SnipShortcut() == (snipSent == 6));
    unsigned expected[] = {0, 1, 2, 3, 2, 1, 0};
    assert(snipCleanup.size() == expected[snipSent]);
    for (auto &key : snipCleanup)
      assert(key.type == INPUT_KEYBOARD && key.ki.dwFlags == KEYEVENTF_KEYUP);
  }
  snipSent = 6;
  // Native image controls track presets and numeric sensitivity.
  ImageDialog imageDialog;
  imageDialog.value.image =
      std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  WNDCLASSW imageClass{};
  imageClass.lpfnWndProc = ImageProc;
  imageClass.hInstance = g_hInst;
  imageClass.lpszClassName = L"ImageControlTest";
  RegisterClassW(&imageClass);
  auto imageSize = ImageDialogSize(ImageWindowDpi(editor));
  HWND imageWindow = CreateWindowExW(
      IMAGE_DIALOG_EX_STYLE, imageClass.lpszClassName, L"", IMAGE_DIALOG_STYLE,
      0, 0, imageSize.cx, imageSize.cy, editor, nullptr, g_hInst, &imageDialog);
  assert(imageWindow && imageDialog.slider && imageDialog.preset &&
         imageDialog.value.sensitivity == 85);
  // Native caption/border sizes must never consume the buttons' client area.
  RECT imageClient{}, useButton{};
  GetClientRect(imageWindow, &imageClient);
  useButton = ImageButton(IDOK);
  assert(useButton.bottom <= imageClient.bottom);
  using AdjustImageFrame = BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT);
  auto adjustImageFrame = (AdjustImageFrame)GetProcAddress(
      GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
  if (adjustImageFrame)
    for (UINT dpi : {96u, 144u, 192u, 288u}) {
      RECT frame{};
      assert(adjustImageFrame(&frame, IMAGE_DIALOG_STYLE, FALSE,
                              IMAGE_DIALOG_EX_STYLE, dpi));
      auto size = ImageDialogSize(dpi);
      assert(size.cy - (frame.bottom - frame.top) >= useButton.bottom);
      assert(size.cx - (frame.right - frame.left) >= 476);
    }
  RECT suggested{200, 200, 210, 210};
  SendMessageW(imageWindow, WM_DPICHANGED, MAKELONG(192, 192),
               (LPARAM)&suggested);
  RECT moved{};
  GetWindowRect(imageWindow, &moved);
  assert(moved.left == 200 && moved.top == 200 &&
         moved.bottom - moved.top == ImageDialogSize(192).cy);
  SendMessageW(imageDialog.slider, TBM_SETPOS, TRUE, 88);
  SendMessageW(imageWindow, WM_HSCROLL, 0, 0);
  assert(imageDialog.value.sensitivity == 88 &&
         SendMessageW(imageDialog.preset, CB_GETCURSEL, 0, 0) == 3);
  SendMessageW(imageDialog.preset, CB_SETCURSEL, 0, 0);
  SendMessageW(imageWindow, WM_COMMAND, MAKEWPARAM(11, CBN_SELCHANGE), 0);
  assert(imageDialog.value.sensitivity == 95);
  SetWindowTextW(imageDialog.timeout, L"4294967296");
  SendMessageW(imageWindow, WM_COMMAND, IDOK, 0);
  assert(!imageDialog.done && !imageDialog.ok);
  SetWindowTextW(imageDialog.timeout, L"99999999999");
  SendMessageW(imageWindow, WM_COMMAND, IDOK, 0);
  assert(!imageDialog.done);
  // Destruction by the owner must join a running worker too (WM_CLOSE is
  // bypassed).
  imageDialog.worker = std::thread([&] {
    while (!imageDialog.cancel)
      Sleep(1);
  });
  DestroyWindow(imageWindow);
  assert(imageDialog.done && !imageDialog.worker.joinable());
  // Values previously rejected silently now round-trip through the real editor.
  for (const wchar_t *timeout : {L"9999999", L"0", L"4294967295"}) {
    ImageDialog large;
    large.value.image = imageDialog.value.image;
    HWND dialog = CreateWindowExW(
        IMAGE_DIALOG_EX_STYLE, imageClass.lpszClassName, L"",
        IMAGE_DIALOG_STYLE, 0, 0, 620, 600, editor, nullptr, g_hInst, &large);
    assert(dialog);
    SetWindowTextW(large.timeout, timeout);
    SendMessageW(dialog, WM_COMMAND, IDOK, 0);
    assert(large.ok && large.done &&
           large.value.imageTimeout == wcstoull(timeout, nullptr, 10));
  }
  // Native source selection is explicit and cancel-safe; it never starts
  // capture by itself.
  WNDCLASSW sourceClass{};
  sourceClass.lpfnWndProc = ImageSourceProc;
  sourceClass.hInstance = g_hInst;
  sourceClass.lpszClassName = L"ImageSourceTest";
  RegisterClassW(&sourceClass);
  for (int choice = 0; choice < 3; ++choice) {
    ImageSourceDlg source;
    source.owner = editor;
    HWND window = CreateWindowW(sourceClass.lpszClassName, L"", WS_POPUP, 0, 0,
                                500, 206, editor, nullptr, g_hInst, &source);
    assert(window);
    RECT button = ImageSourceButton(choice);
    SendMessageW(window, WM_LBUTTONUP, 0,
                 MAKELPARAM(button.left + 2, button.top + 2));
    assert(source.done && source.choice == (choice == 2 ? 0 : choice + 1));
  }
  assert((IMAGE_DIALOG_STYLE & WS_CAPTION) ==
         0); // AutoOnty draws the window chrome.
  // Image payloads survive editor projection, changes, undo and redo.
  EdItem imageItem;
  imageItem.a.type = A_IMAGE;
  imageItem.a.image =
      std::make_shared<image::Pixels>(image::Pixels{1, 1, {0x123456}});
  imageItem.a.imageFile = image::Name(*imageItem.a.image);
  d.it = {imageItem};
  d.tail.clear();
  EdReload(&d, {0}, 0);
  assert(EdEnabled(&d, EB_KEY));
  assert(wcscmp(EdBtnText(EB_KEY, &d), L(L_IMAGE_SETTINGS)) == 0);
  EdPush(&d);
  d.it[0].a.sensitivity = 95;
  EdUndo(&d);
  assert(d.it[0].a.sensitivity == 85 && d.it[0].a.image == imageItem.a.image);
  EdRedo(&d);
  assert(d.it[0].a.sensitivity == 95);
  std::vector<MAc> imageTail;
  auto projected = EdFromEvents(EdToEvents(d.it, d.tail), imageTail, 0);
  assert(projected.size() == 1 && projected[0].a.type == A_IMAGE);
  if (GetEnvironmentVariableW(L"AUTOONTY_UI_PREVIEWS", nullptr, 0)) {
    auto savedTheme = g_cfg.theme;
    for (int theme : {0, 1}) {
      g_cfg.theme = theme;
      ImageDialog preview;
      preview.dedicated = true;
      preview.value.image = std::make_shared<image::Pixels>(
          image::Pixels{180, 54, std::vector<uint32_t>(180 * 54, 0xececec)});
      {
        Gdiplus::Bitmap button(180, 54, 180 * 4, PixelFormat32bppRGB,
                               (BYTE *)preview.value.image->rgb.data());
        Gdiplus::Graphics draw(&button);
        Gdiplus::Pen border(Gdiplus::Color(130, 130, 130));
        draw.DrawRectangle(&border, 1, 1, 177, 51);
        Gdiplus::Font font(L"Segoe UI", 14);
        Gdiplus::SolidBrush ink(Gdiplus::Color(25, 25, 25));
        draw.DrawString(L"Continue", -1, &font, Gdiplus::PointF(45, 13), &ink);
      }
      preview.value.imageTimeout = 0;
      preview.value.imageOptions.repeat = IR_REAPPEAR;
      HWND window =
          CreateWindowExW(IMAGE_DIALOG_EX_STYLE, imageClass.lpszClassName, L"",
                          IMAGE_DIALOG_STYLE, 0, 0, 620, 600, editor, nullptr,
                          g_hInst, &preview);
      assert(window);
      for (int page = 0; page < 3; ++page) {
        preview.page = page;
        ImagePage(&preview);
        HDC dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), 620, -600, 1, 32, BI_RGB};
        void *pixels = nullptr;
        HBITMAP bitmap =
            CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        auto old = SelectObject(dc, bitmap);
        SendMessageW(window, WM_PRINTCLIENT, (WPARAM)dc, 0);
        for (auto [child, childPage] : preview.pages)
          if (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) {
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, window, (LPPOINT)&r, 2);
            SaveDC(dc);
            SetViewportOrgEx(dc, r.left, r.top, nullptr);
            SendMessageW(child, WM_PRINT, (WPARAM)dc,
                         PRF_CLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
            RestoreDC(dc, -1);
          }
        GdiFlush();
        image::Pixels output{
            620, 600,
            std::vector<uint32_t>((uint32_t *)pixels,
                                  (uint32_t *)pixels + 620 * 600)};
        wchar_t executable[MAX_PATH];
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        auto path = image::Directory(executable) + L"image-ui-" +
                    std::to_wstring(theme) + L"-" + std::to_wstring(page) +
                    L".bmp";
        assert(image::Save(path, output));
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
      }
      DestroyWindow(window);
    }
    g_cfg.theme = savedTheme;
  }
  // Autoclick image controls use the shared image UI and a separate staged list.
  ImageDialog acSettings;
  acSettings.autoclick = true;
  acSettings.value.type = A_IMAGE;
  acSettings.value.image = imageDialog.value.image;
  acSettings.value.imageOptions.action = IA_AUTOCLICK;
  acSettings.value.imageOptions.interval = 37;
  acSettings.value.imageOptions.clicks = 12;
  acSettings.value.imageOptions.autoDuration = 1500;
  HWND acWindow = CreateWindowExW(IMAGE_DIALOG_EX_STYLE, imageClass.lpszClassName,
      L"", IMAGE_DIALOG_STYLE, 0, 0, 620, 600, editor, nullptr, g_hInst, &acSettings);
  assert(acWindow);
  assert(IsWindowEnabled(GetDlgItem(acWindow, IM_ACTION)));
  assert(SendMessageW(GetDlgItem(acWindow, IM_ACTION), CB_GETCURSEL, 0, 0) == 1);
  assert(!ImageButtonActive(&acSettings, 102));
  assert(ImageRead(&acSettings));
  assert(acSettings.value.imageOptions.interval == 37 &&
         acSettings.value.imageOptions.clicks == 12 &&
         acSettings.value.imageOptions.autoDuration == 1500);
  SendMessageW(GetDlgItem(acWindow, IM_ACTION), CB_SETCURSEL, 0, 0);
  SendMessageW(acWindow, WM_COMMAND, MAKEWPARAM(IM_ACTION, CBN_SELCHANGE), 0);
  assert(ImageRead(&acSettings) && acSettings.value.imageOptions.clicks == 1);
  SendMessageW(GetDlgItem(acWindow, IM_ACTION), CB_SETCURSEL, 1, 0);
  SetWindowTextW(GetDlgItem(acWindow, IM_CLICKS), L"3");
  assert(ImageRead(&acSettings) && acSettings.value.imageOptions.clicks == 3);
  SetWindowTextW(GetDlgItem(acWindow, IM_CLICKS), L"0");
  assert(!ImageRead(&acSettings));
  SendMessageW(GetDlgItem(acWindow, IM_ACTION), CB_SETCURSEL, 2, 0);
  SendMessageW(acWindow, WM_COMMAND, MAKEWPARAM(IM_ACTION, CBN_SELCHANGE), 0);
  assert(ImageRead(&acSettings) && acSettings.value.imageOptions.clicks == 0);
  acSettings.page = 1;
  SendMessageW(GetDlgItem(acWindow, IM_TARGET), CB_SETCURSEL, IT_FIXED, 0);
  assert(ImageButtonActive(&acSettings, 17));
  SetWindowTextW(GetDlgItem(acWindow, IM_INTERVAL), L"0");
  assert(!ImageRead(&acSettings));
  DestroyWindow(acWindow);
  AutoclickImagesDialog acList;
  acList.images = {acSettings.value, acSettings.value};
  acList.selectedMask = 2;
  WNDCLASSW acClass{};
  acClass.lpfnWndProc = AcImagesProc;
  acClass.hInstance = g_hInst;
  acClass.lpszClassName = L"AutoclickImageListTest";
  RegisterClassW(&acClass);
  HWND acListWindow = CreateWindowExW(WS_EX_TOOLWINDOW, acClass.lpszClassName,
      L"", WS_POPUP, 0, 0, 620, 492, editor, nullptr, g_hInst, &acList);
  assert(acListWindow && SendMessageW(acList.list, LB_GETCOUNT, 0, 0) == 2);
  assert(GetWindowLongW(GetDlgItem(acListWindow, IDOK), GWL_STYLE) & WS_TABSTOP);
  // No child may alias Cancel: that collision closed the old checkbox dialog.
  int cancelChildren = 0;
  for (HWND child = GetWindow(acListWindow, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    if (GetDlgCtrlID(child) == IDCANCEL) ++cancelChildren;
  assert(cancelChildren == 1);
  SendMessageW(acListWindow, WM_COMMAND, MAKEWPARAM(3, LBN_SELCHANGE), (LPARAM)acList.list);
  assert(!acList.done);
  SendMessageW(acList.list, LB_SETCURSEL, 0, 0);
  SendMessageW(acListWindow, WM_COMMAND, 12, 0);
  assert(acList.selectedMask == 1 && acList.images.size() == 1);
  acList.images.push_back(acSettings.value);
  AcImagesReload(&acList, 1);
  SendMessageW(acList.list, LB_SETCURSEL, 1, 0);
  SendMessageW(acListWindow, WM_COMMAND, 12, 0);
  assert(acList.images.size() == 1 && SendMessageW(acList.list, LB_GETCOUNT, 0, 0) == 1);
  SendMessageW(acListWindow, WM_COMMAND, IDCANCEL, 0);
  assert(acList.done && !IsWindow(acListWindow));
  // Picker clicks/Space change selections without accepting or closing it.
  AutoclickImagesDialog acPicker;
  acPicker.selecting = true;
  acPicker.images = {acSettings.value, acSettings.value};
  acPicker.selectedMask = 1;
  HWND picker = CreateWindowExW(WS_EX_TOOLWINDOW, acClass.lpszClassName,
      L"", WS_POPUP, 0, 0, 620, 492, editor, nullptr, g_hInst, &acPicker);
  assert(picker && SendMessageW(acPicker.list, LB_GETSEL, 0, 0) > 0);
  assert(!GetDlgItem(picker, 10));
  SendMessageW(acPicker.list, LB_SETSEL, FALSE, 0);
  SendMessageW(acPicker.list, LB_SETSEL, TRUE, 1);
  SendMessageW(picker, WM_COMMAND, MAKEWPARAM(3, LBN_SELCHANGE), (LPARAM)acPicker.list);
  assert(!acPicker.done && !acPicker.ok);
  SendMessageW(picker, WM_COMMAND, IDOK, 0);
  assert(acPicker.done && acPicker.ok && acPicker.selectedMask == 2);
  // Real modal picker: cancelling keeps the old selection, accepting commits it.
  assert(Autoclick_SaveImages({acSettings.value, acSettings.value}));
  g_cfg.acImageMask = 1;
  answerImagePicker = true;
  modalImageMask = 2;
  PrepareModal(editor, L"", true);
  assert(!Macros_SelectAutoclickImages(editor));
  assert(modalAnswers == 1 && g_cfg.acImageMask == 1);
  PrepareModal(editor, L"");
  assert(Macros_SelectAutoclickImages(editor));
  assert(modalAnswers == 1 && g_cfg.acImageMask == 2);
  answerImagePicker = false;
  assert(Autoclick_SaveImages({}));
  DestroyWindow(editor);
  g_hwnd = nullptr;
  GfxShutdown();
  puts("Editor tests passed: image controls/undo, native selection, rubber "
       "band, panel geometry, "
       "undo/redo, isolated preview.");
}
