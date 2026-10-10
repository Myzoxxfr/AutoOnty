#pragma once
#include <string>
#include <windows.h>

// State owned by main.cpp that the macro library needs to read and update:
// the file the current macro was loaded from / saved to (empty = a fresh,
// never-saved recording) and whether it has changes that aren't on disk.
extern std::wstring g_lastFile;
extern bool g_dirty;

// "<Documents>\AutoOnty" - where named macros are stored. Created on demand.
std::wstring Macros_Dir();

// File name without folder and extension ("C:\x\Welcome.onty" -> "Welcome").
std::wstring Macros_Stem(const std::wstring &path);

// True while the library / editor / a prompt is on screen. main.cpp uses this
// to ignore hotkeys that would change the macro underneath the editor.
bool Macros_IsModalOpen();

// "Macro: Welcome" / "Macro: New macro" / "Macro: none" - shown on the bar.
std::wstring Macros_CurrentLabel();

void Macros_RegisterClasses();

// The macro list window (Load / Edit / Rename / Delete / Save / Browse...).
void Macros_ShowLibrary(HWND owner);

// Asks for a name and saves the current macro into the library folder.
// Returns true if it was saved.
bool Macros_SaveCurrent(HWND owner);
