# Hotkeys & Controls

AutoOnty lets you trigger recordings and macros from anywhere across desktop applications and full-screen games.

---

## 1. Default Shortcuts

| Action            |                            Default Key                             | What it does                                 |
| :---------------- | :----------------------------------------------------------------: | :------------------------------------------- |
| **Record / Stop** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>R</kbd> | Starts or stops macro recording              |
| **Play / Stop**   | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>P</kbd> | Starts or stops macro playback               |
| **Autoclick**     | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd> | Starts or stops the autoclicker              |
| **Set Click Pos** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd> | Saves the cursor position for autoclick      |
| **Panic Stop**    |                  <kbd>Pause</kbd> (rebindable)                      | Immediately interrupts playback or autoclick |
| **Save macro**    |                    <kbd>Ctrl</kbd> + <kbd>S</kbd>                    | Saves the current macro (asks for a name the first time) |
| **Open file**     |                    <kbd>Ctrl</kbd> + <kbd>O</kbd>                    | Opens a `.onty` file from anywhere on your PC |
| **Cancel Dialog** |                           <kbd>Esc</kbd>                           | Closes open menus or cancels shortcut prompt |
| **Confirm**       |                          <kbd>Enter</kbd>                          | Confirms custom speed or loop count dialogs  |

---

## 2. Setting Custom Shortcuts

You can rebind Record, Play, Autoclick, and Set click position to any single key or combination:

1. Open settings: click **Menu** on the toolbar (or right-click the tray icon).
2. Go to **Hotkeys** and pick **Recording**, **Playback**, **Autoclick**, or **Set click position**.
3. A prompt appears: `Press a new hotkey (Esc to cancel)`.
4. Press your desired key or combination (e.g. <kbd>F8</kbd>, <kbd>Alt+F8</kbd>, <kbd>Ctrl+Alt+Q</kbd>).
5. The shortcut is updated immediately and saved to `AutoOnty.ini`.

---

## 3. Panic Stop Keys (Emergency Abort)

To abort a running macro or continuous loop, set your own emergency key under **Menu -> Hotkeys -> Panic stop key** (default: <kbd>Pause</kbd>). Pick any key or combo, as long as it does not clash with your other hotkeys. There is also a second option:

- **Stop on mouse move:** When enabled, simply moving your physical mouse during playback will immediately stop the macro.

### Status Bar Indicators

- During recording or playback, the status bar displays the active stop key (e.g. `PLAY 00:03 (1/5) • Stop: Pause`).
- If you move the mouse during playback with mouse-stop disabled, a quick reminder tooltip appears: `Macro playing • Press Pause to stop`.
