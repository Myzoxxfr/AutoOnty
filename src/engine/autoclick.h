#pragma once
#include <atomic>
#include <windows.h>

// Posted to g_hwnd when the autoclick thread has finished (stopped by the user,
// by a panic key, or because the configured number of clicks was reached).
#define WM_APP_AUTOCLICKDONE (WM_APP + 6)

extern std::atomic<bool> g_acRunning;
extern std::atomic<unsigned> g_acClicks;
extern std::atomic<DWORD> g_acStartTick;
extern std::atomic<int> g_acCountdown;

// Interval from the Hours / Mins / Secs / Ms settings in milliseconds (>= 1).
DWORD Autoclick_IntervalMs();

// Starts clicking with the current g_cfg settings. Returns false if it is
// already running or the thread could not be created.
bool Autoclick_Start();

// Asks the thread to stop (non-blocking). WM_APP_AUTOCLICKDONE follows.
void Autoclick_Stop();

// Stops and waits up to `ms` for the thread to end (used before Rec/Play and
// on exit).
void Autoclick_StopAndWait(DWORD ms);

// Saves the current cursor position as the fixed click position and switches
// to "fixed position" mode.
bool Autoclick_SavePosition();
