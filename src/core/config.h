#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct Config {
  int x, y, speed, loops, continuous, topmost;
  double speedCustom;
  int recordMods, recordVk, playMods, playVk;
  int theme, lang, statusbarTop, autoupdate, trayIcon, uiIcon;
  int panicMods, panicVk, panicMouseMove; // panic stop hotkey (rebindable)
  // What a recording captures (all on by default).
  int recMouseMove, recLeft, recRight, recMiddle, recWheelUp, recWheelDown;
  // What the macro editor lists (mouse moves are off by default).
  int edKeys, edLeft, edRight, edMiddle, edWheelUp, edWheelDown, edMoves;
  int statusbarShow, statusbarFollowCursor;
  int autoMinimize;
  int soundCue;                        // beep when playback / autoclick starts or stops
  // Autoclick: interval, click options, repeat, position mode and hotkeys.
  int acMode;                          // 0 = follow cursor, 1 = fixed position
  int acHours, acMins, acSecs, acMs;   // click interval
  int acButton;                        // 0 = left, 1 = right, 2 = middle
  int acDouble;                        // 0 = single click, 1 = double click
  int acRepeatUntilStop, acRepeatCount;
  int acHasPos, acX, acY;              // saved fixed position (screen coords)
  int acJitter;                        // 1 = randomise each interval a little
  int acMods, acVk, acPosMods, acPosVk;
};

extern Config g_cfg;
extern HWND g_hwnd;
extern HINSTANCE g_hInst;
extern std::vector<std::wstring> g_recentFiles;

void ConfigLoad();
void ConfigSave();
void RecentAdd(const std::wstring &path);
void RecentClear();
bool IsFileAssociationRegistered();
bool RegisterFileAssociation(bool enable);
