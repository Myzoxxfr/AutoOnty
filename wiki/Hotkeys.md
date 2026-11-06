# Hotkeys & Controls

AutoOnty lets you trigger recordings and macros from anywhere across desktop applications and full-screen games.

---

## 1. Default Shortcuts

| Action            |                            Default Key                             | What it does                                 |
| :---------------- | :----------------------------------------------------------------: | :------------------------------------------- |
| **Record / Stop** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>R</kbd> | Starts or stops macro recording              |
| **Play / Stop**   | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>P</kbd> | Starts or stops macro playback               |
| **Pause / Resume** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>H</kbd> | Pauses/resumes playback; fallback if already assigned |
| **Autoclick**     | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd> | Starts or stops the autoclicker              |
| **Set Click Pos** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd> | Saves the cursor position for autoclick      |
| **Panic Stop**    |                  <kbd>Pause</kbd> (rebindable)                      | Immediately interrupts playback or autoclick |
| **Save macro**    |                    <kbd>Ctrl</kbd> + <kbd>S</kbd>                    | Saves the current macro (asks for a name the first time) |
| **Open file**     |                    <kbd>Ctrl</kbd> + <kbd>O</kbd>                    | Opens a `.onty` file from anywhere on your PC |
| **Undo / Redo (macro editor)** | <kbd>Ctrl</kbd> + <kbd>Z</kbd> / <kbd>Ctrl</kbd> + <kbd>Y</kbd> | Undoes / redoes the last edit (also <kbd>Ctrl+Shift+Z</kbd>) |
| **Cancel Dialog** |                           <kbd>Esc</kbd>                           | Closes open menus or cancels shortcut prompt |
| **Confirm**       |                          <kbd>Enter</kbd>                          | Confirms custom speed or loop count dialogs  |

---

## 2. Setting Custom Shortcuts

You can rebind Record, Play, Pause / resume, Autoclick and Set click position:

1. Open **Menu -> Hotkeys** and choose the action.
2. Press your new key or combination, such as <kbd>F8</kbd> or <kbd>Ctrl+Alt+Q</kbd>. Esc cancels.
3. The shortcut is saved automatically.

---

## 3. Macro Hotkeys (load a saved macro with one key)

Every macro in your library can have its own hotkey:

1. Open the macro library (the list button on the toolbar) and select a macro.
2. Click **Hotkey...** -> **Set hotkey...** and press the key you want (e.g. <kbd>F5</kbd>).
3. Press that key anywhere to **load** the macro. It does not play it: use your normal **Play** hotkey for that.

Good to know:

- For a macro load hotkey, use <kbd>F1</kbd>-<kbd>F24</kbd>, <kbd>Pause</kbd> or <kbd>Scroll Lock</kbd> alone. Other keys need <kbd>Ctrl</kbd>, <kbd>Alt</kbd> or <kbd>Shift</kbd> so plain typing keys remain available.
- Shortcuts already assigned to another action or macro are refused.
- The hotkey appears beside the macro. Use **Hotkey... -> Remove hotkey** to clear it.
- Macro load hotkeys work while AutoOnty is idle and the library is closed.
- If the current macro has unsaved changes you are asked before it is replaced.
- Hotkeys follow renamed macros and are removed when the macro is deleted.

---

## 4. Panic Stop (Emergency Abort)

Set the emergency key under **Menu -> Hotkeys -> Panic stop key** (default <kbd>Pause</kbd>). It stops playback or autoclick and must not conflict with another shortcut.

- **Stop on mouse move:** Off by default. When enabled, moving your physical mouse during active playback stops the macro. Moving it while paused or during the countdown does not stop it.

For pause and resume behavior, see [Playback & Editing](Playback-and-Editing.md#pause--resume).

### Status Bar Indicators

The status bar shows the active stop shortcut while recording or playing.

## Autoclick hold mode

Tick **Menu -> Autoclick settings -> Toggle Mode** to click while holding the hotkey. Unchecked (default) means press to start/stop. Toolbar clicks always toggle. See [Autoclick](Autoclick.md) for settings.
