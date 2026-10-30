#include "../src/ui/macros.cpp"
#include <cassert>
#include <cstdio>
std::wstring g_lastFile;
bool g_dirty = false;

// Answer only modal dialogs owned by this isolated editor; never inject input.
static HWND modalOwner;
static const wchar_t *modalValue = L"0";
static bool modalCancel = false;
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
  DestroyWindow(editor);
  g_hwnd = nullptr;
  GfxShutdown();
  puts("Editor tests passed: native selection, rubber band, panel geometry, "
       "undo/redo, isolated preview.");
}
