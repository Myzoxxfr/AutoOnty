# Settings & Customization

Configure themes, tray behavior, the status overlay, application targets and storage.

---

## 1. Visual Themes

You can switch the interface theme anytime under **Menu -> Theme -> Interface theme**:

- **Dark Theme:** A dark palette for low-light environments.
- **Light Theme:** A high-contrast light design with native light popup menus.
- **Acrylic Theme:** A modern semi-transparent blur effect using Windows DWM composition (Windows 10 and Windows 11).

---

## 2. Tray and Window Icons

Icon appearance can be adjusted under **Menu -> Theme**:

- **Tray icon:** Choose `Follow theme`, `Dark` or `Light`. A light icon is easier to see on a dark taskbar.
- **Auto-minimize on Rec/Play:** Minimizes AutoOnty while recording or playing a macro.
- **Beep on start / stop:** Plays a sound when playback or autoclick starts and stops. Off by default.

---

## 3. Floating Status Bar (HUD)

The floating status bar shows recording/playback progress, countdowns, pause state and notifications.

### Configuration

Under **Menu -> Status bar**:

- **Show status bar:** Toggles the HUD on or off.
- **Top / Bottom of screen:** Sets the default screen position.
- **Follow cursor monitor:** Automatically moves the status bar to whichever monitor currently contains your mouse cursor.

### Cursor Evasion

The status bar moves to the opposite edge when your cursor approaches and returns when the area is clear.

## Window targets

**Menu -> Macro settings -> Run macro when window is focused** and **Autoclick settings -> Click when mouse is over window** have separate application lists. Choose **Select window...** and click an external window; Esc cancels. Picking an app matches all windows from that executable. Pick it again or click its checked entry to remove it, or use **Clear all / disable**.

Playback waits while an unselected app is focused, releasing held inputs and restoring them when it resumes. Returning focus does not undo a manual pause. Autoclick waits while the mouse is over an unselected app, without spending repeats; fixed destinations and jitter points must also be in a selected app. Both show **Waiting for** while waiting and remain stoppable with the panic key.

For hold mode and position jitter, see [Autoclick](Autoclick.md#hold-mode-random-position-and-window-targets).

## Finding settings

**Macro settings** contains image macros, playback speed, loops, countdown, window targets, record inputs and editor visibility. **Record inputs** includes X1/X2 side-button switches, on by default. **Autoclick settings** controls clicking and its [saved images](Image-Recognition.md#use-with-autoclick). Use **Hotkeys** to change shortcuts.

`.onty` file registration happens automatically at launch. Copy/paste macros with **Ctrl+C / Ctrl+V** in the main window.

## Storage and backups

Settings use `Documents\AutoOnty\AutoOnty.ini`. Macros, per-macro settings and editor preferences also use `Documents\AutoOnty`; autoclick images use its `ImageDetections` folder. Redirected Windows Documents folders are supported.

### Importing settings from beside the executable

Keep your old files beside the executable until they have been imported. If no Documents INI exists, AutoOnty imports the old INI at startup. The old autoclick image list and its assets import when you first open the image manager or picker. Existing Documents files take priority, and the originals are retained. A failed import reports an error; it does not silently overwrite your existing data.

### Backing up data

Back up `Documents\AutoOnty` to keep the library, settings and autoclick images together. Macros saved elsewhere may have their own adjacent `ImageDetections` folder; keep that folder with those macros too.

Image-based autoclicking must be enabled each launch; your saved images and selection are remembered.
