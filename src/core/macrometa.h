#pragma once
#include <string>
#include <vector>
#include <windows.h>

// Per-macro extras for macros stored in the library folder
// ("<Documents>\AutoOnty"): a load hotkey and optional playback overrides.
// Kept in "<library>\macros.ini" next to the .onty files, keyed by file name.
// Without an override a macro simply uses the general Speed / Loops settings.
struct MacroMeta {
  std::wstring name; // file name without ".onty"
  int hkMods = 0, hkVk = 0; // load hotkey, hkVk == 0 -> none
  int ovLoops = 0;          // 1 = this macro has its own loop setting
  int loops = 1, continuous = 0;
  int ovSpeed = 0;          // 1 = this macro has its own speed
  int speed = 1;            // same codes as Config::speed
  double speedCustom = 1.0;
};

std::vector<MacroMeta> &MacroMeta_All();

// nullptr when the file isn't in the library or has no entry.
MacroMeta *MacroMeta_Find(const std::wstring &path);
// Entry for a library file, created if needed. nullptr if not in the library.
MacroMeta *MacroMeta_Edit(const std::wstring &path);
void MacroMeta_Rename(const std::wstring &oldPath, const std::wstring &newPath);
void MacroMeta_Remove(const std::wstring &path);
// Drops entries with nothing set, then writes the file.
void MacroMeta_Save();
// Macro that already uses this hotkey (other than `except`), or nullptr.
const MacroMeta *MacroMeta_ByHotkey(int mods, int vk, const MacroMeta *except);
// Bumped on every change so main.cpp knows to re-register the hotkeys.
unsigned MacroMeta_Version();

// Run-only: makes Eff_* ignore this macro's own loop / speed override while
// it is the loaded file (used by --loop / --speed). Nothing is stored.
void MacroMeta_IgnoreOverrides(const std::wstring &path, bool loops,
                               bool speed);

// Playback settings that apply to the current macro: its own override if it
// has one, otherwise the general setting from g_cfg.
int Eff_Loops();
int Eff_Continuous();
int Eff_Speed();
double Eff_SpeedCustom();
