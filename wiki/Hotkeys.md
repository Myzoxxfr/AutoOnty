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

You can rebind Record, Play, Pause / resume, Autoclick, and Set click position to any single key or combination:

1. Open settings: click **Menu** on the toolbar (or right-click the tray icon).
2. Go to **Hotkeys** and pick **Recording**, **Playback**, **Pause / resume**, **Autoclick**, or **Set click position**.
3. A prompt appears: `Press a new hotkey (Esc to cancel)`.
4. Press your desired key or combination (e.g. <kbd>F8</kbd>, <kbd>Alt+F8</kbd>, <kbd>Ctrl+Alt+Q</kbd>).
5. The shortcut is updated immediately and saved to `AutoOnty.ini`.

---

## 2b. Macro Hotkeys (load a saved macro with one key)

Every macro in your library can have its own hotkey:

1. Open the macro library (the list button on the toolbar) and select a macro.
2. Click **Hotkey...** -> **Set hotkey...** and press the key you want (e.g. <kbd>F5</kbd>).
3. Press that key anywhere to **load** the macro. It does not play it: use your normal **Play** hotkey for that.

Good to know:

- A key on its own must be an <kbd>F1</kbd>-<kbd>F24</kbd>, <kbd>Pause</kbd> or <kbd>Scroll Lock</kbd> key. For anything else add <kbd>Ctrl</kbd> / <kbd>Alt</kbd> / <kbd>Shift</kbd>, otherwise a plain letter would stop working in every other program.
- A key already used by Record, Play, Autoclick, Set click position, Pause / resume, Panic stop or another macro is refused, and the message tells you which one.
- The hotkey is shown on the right of the macro's row in the library. **Hotkey... -> Remove hotkey** clears it.
- Macro hotkeys only react while AutoOnty is idle: during recording, playback and autoclick (and while a library window is open) the key behaves normally.
- If the current macro has unsaved changes you are asked before it is replaced.
- Hotkeys follow a macro when you rename it and disappear when you delete it. They are stored in `macros.ini` inside the library folder.

---

## 3. Panic Stop Keys (Emergency Abort)

To abort a running macro or continuous loop, set your own emergency key under **Menu -> Hotkeys -> Panic stop key** (default: <kbd>Pause</kbd>). Pick any key or combo, as long as it does not clash with your other hotkeys. There is also a second option:

- **Stop on mouse move:** When enabled, moving your physical mouse during active playback stops the macro. Moving it while paused or during the countdown does not stop it.

Pause releases held keys/buttons and restores them on resume. Stop while paused ends playback without restoring them. See [Playback & Editing](Playback-and-Editing.md).

### Status Bar Indicators

- During recording or playback, the status bar displays the active stop key (e.g. `PLAY 00:03 (1/5) • Stop: Pause`).
- If you move the mouse during playback with mouse-stop disabled, a quick reminder tooltip appears: `Macro playing • Press Pause to stop`.
