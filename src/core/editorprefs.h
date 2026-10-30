#pragma once
#include "engine/engine.h"
#include <bitset>

inline bool PreviewKeyboardKey(unsigned key) {
  return key == VK_CANCEL || (key >= VK_BACK && key <= 255);
}

// Familiar full-size keyboard keys, in common-to-less-common groups.
// Keep raw virtual-key support separate for saved choices and explicit capture.
inline const std::vector<unsigned> &PreviewStandardKeys() {
  static const std::vector<unsigned> keys = [] {
    std::vector<unsigned> out;
    auto add = [&](unsigned key) {
      for (auto existing : out)
        if (existing == key)
          return;
      out.push_back(key);
    };
    for (unsigned key = 'A'; key <= 'Z'; ++key)
      add(key);
    for (unsigned key = '0'; key <= '9'; ++key)
      add(key);
    for (unsigned key :
         {VK_SPACE,      VK_RETURN, VK_BACK,     VK_TAB,       VK_ESCAPE,
          VK_CONTROL,    VK_SHIFT,  VK_MENU,     255,          VK_LCONTROL,
          VK_RCONTROL,   VK_LSHIFT, VK_RSHIFT,   VK_LMENU,     VK_RMENU,
          VK_LWIN,       VK_RWIN,   VK_LEFT,     VK_RIGHT,     VK_UP,
          VK_DOWN,       VK_OEM_1,  VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS,
          VK_OEM_PERIOD, VK_OEM_2,  VK_OEM_3,    VK_OEM_4,     VK_OEM_5,
          VK_OEM_6,      VK_OEM_7,  VK_HOME,     VK_END,       VK_PRIOR,
          VK_NEXT,       VK_INSERT, VK_DELETE})
      add(key);
    for (unsigned key = VK_F1; key <= VK_F12; ++key)
      add(key);
    for (unsigned key = VK_NUMPAD0; key <= VK_NUMPAD9; ++key)
      add(key);
    for (unsigned key :
         {VK_ADD, VK_SUBTRACT, VK_MULTIPLY, VK_DIVIDE, VK_DECIMAL, VK_CAPITAL,
          VK_NUMLOCK, VK_SCROLL, VK_SNAPSHOT, VK_PAUSE, VK_APPS, VK_OEM_102})
      add(key); // extra punctuation key on ISO full-size layouts
    return out;
  }();
  return keys;
}

// Local per-file editor preferences; never embedded in a macro's input stream.
struct PreviewExclusions {
  unsigned mouse = 0; // button * 2 + (release ? 1 : 0), for all five buttons
  unsigned wheel = 0; // up, down, left, right
  bool movement = false;
  std::bitset<256> keys; // 255 is the generic Windows-key group
  bool Empty() const { return !mouse && !wheel && !movement && keys.none(); }
  bool KeyBlocked(BYTE vk, DWORD flags = 0) const {
    if (keys[vk])
      return true;
    if ((vk == VK_LWIN || vk == VK_RWIN) && keys[255])
      return true;
    struct Mod {
      BYTE generic, left, right;
    };
    const Mod mods[] = {{VK_CONTROL, VK_LCONTROL, VK_RCONTROL},
                        {VK_SHIFT, VK_LSHIFT, VK_RSHIFT},
                        {VK_MENU, VK_LMENU, VK_RMENU}};
    for (auto mod : mods) {
      if ((vk == mod.left || vk == mod.right) && keys[mod.generic])
        return true;
      if (vk == mod.generic) {
        BYTE side = (flags & KEYEVENTF_EXTENDEDKEY) ? mod.right : mod.left;
        if (keys[side])
          return true;
      }
    }
    return false;
  }
  bool ButtonBlocked(BYTE button, bool release) const {
    return button < 5 && (mouse & (1u << (button * 2 + (release ? 1 : 0))));
  }
  bool Blocked(const MAc &a) const {
    if (a.type == A_KD || a.type == A_KU)
      return KeyBlocked(a.vk, a.flags);
    if (a.type == A_MD || a.type == A_MU)
      return ButtonBlocked(a.btn, a.type == A_MU);
    if (a.type == A_MOVE)
      return movement;
    if (a.type == A_WH && a.wheel != 0) {
      int direction =
          a.horizontal ? (a.wheel < 0 ? 2 : 3) : (a.wheel > 0 ? 0 : 1);
      return (wheel & (1u << direction)) != 0;
    }
    return false;
  }
};

enum EditorVisibility {
  EV_KEYS = 1 << 0,
  EV_LEFT = 1 << 1,
  EV_RIGHT = 1 << 2,
  EV_MIDDLE = 1 << 3,
  EV_X1 = 1 << 4,
  EV_X2 = 1 << 5,
  EV_WHEELUP = 1 << 6,
  EV_WHEELDOWN = 1 << 7,
  EV_WHEELLEFT = 1 << 8,
  EV_WHEELRIGHT = 1 << 9,
  EV_MOVES = 1 << 10,
  EV_ALL = (1 << 11) - 1
};
struct EditorPreferences {
  PreviewExclusions exclude;
  bool customVisibility = false;
  unsigned visibility = EV_ALL;
  bool Empty() const { return exclude.Empty() && !customVisibility; }
};
EditorPreferences EditorPrefs_Get(const std::wstring &path);
bool EditorPrefs_Set(const std::wstring &path, const EditorPreferences &prefs);
void EditorPrefs_ResetTemporary();
bool EditorPrefs_SaveTemporary(const std::wstring &path);
bool EditorPrefs_Rename(const std::wstring &oldPath,
                        const std::wstring &newPath);
bool EditorPrefs_Remove(const std::wstring &path);
