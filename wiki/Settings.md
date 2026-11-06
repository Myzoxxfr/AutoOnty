# Themes & Customization

AutoOnty provides several visual themes, tray icon options, and a configurable status overlay.

---

## 1. Visual Themes

You can switch the interface theme anytime under **Menu -> Theme -> Interface theme**:

- **Dark Theme:** A sleek dark palette designed for low-light environments.
- **Light Theme:** A high-contrast light design with native light popup menus.
- **Acrylic Theme:** A modern semi-transparent blur effect using Windows DWM composition (Windows 10 and Windows 11).

---

## 2. Tray and Window Icons

Icon appearance can be adjusted under **Menu -> Theme**:

- **Tray icon:**
    - `Follow theme`: Automatically toggles between light and dark icons based on the active theme.
    - `Dark`: Displays a dark tray icon.
    - `Light`: Displays a white/light tray icon (ideal for dark Windows taskbars).
- **Auto-minimize on Rec/Play:** When enabled, the AutoOnty window automatically minimizes when recording or playing a macro to stay out of view.
- **Beep on start / stop:** When enabled (off by default), a short beep plays when macro playback or autoclick starts (high tone) and stops (low tone).

---

## 3. Floating Status Bar (HUD)

AutoOnty features a lightweight, borderless status HUD that gives real-time visual feedback:

- **Active State:** Displays timer and loop status (e.g. `REC 00:02 • Stop: Ctrl+Shift+Alt+R` or `PLAY 00:03 (1/5) • Stop: Pause`).
- **Countdown / Pause:** Shows remaining countdown seconds before playback/autoclick, or Paused while macro playback is paused.
- **Toast Alerts:** Shows brief notifications when files are saved, shortcuts conflict, or settings change.

### Configuration

Under **Menu -> Status bar**:

- **Show status bar:** Toggles the HUD on or off.
- **Top / Bottom of screen:** Sets the default screen position.
- **Follow cursor monitor:** Automatically moves the status bar to whichever monitor currently contains your mouse cursor.

### Cursor Evasion

To ensure the status bar never covers clickable buttons or in-game elements:

- When your mouse cursor moves close to the status bar, it smoothly jumps to the opposite edge of the screen.
- Once the cursor leaves the area, it returns to its configured position.

## Window targets (v1.6)

**Menu -> Macro settings -> Run macro when window is focused** and **Autoclick settings -> Click when mouse is over window** keep independent application lists. Choose **Select window...** and click a window with the crosshair (Esc cancels); AutoOnty's own windows are ignored. The picker resolves the top-level window and saves only the process exe name. Pick an app again to unselect it, click its checked entry with the right-hand cross to remove it, or **Clear all / disable**.

Playback releases held inputs while another app is focused and restores them on automatic resume. Manual pause remains independent: returning focus does not undo a manual pause. Autoclick runs while the mouse is over a selected app, even if that app does not have keyboard focus; moving onto another app makes it wait without spending finite repeats. Fixed destinations and jitter landing points must also belong to a selected app. Both can start while waiting, show the selected exe names, and remain stoppable with panic stop. Settings persist in `AutoOnty.ini`.

**Toggle Mode**, **Random click position**, and **Position radius (pixels)** are in Autoclick settings. Hold mode and position jitter default off; radius defaults to 3 pixels (1–50).

## Menu groups

Record, Play, Autoclick, Macros, Open and Save share the first group. Macro settings, Autoclick settings and Hotkeys share the next group. Macro settings contains Image recognition macro, speed, playback loops, countdown, window targeting, record inputs and editor visibility. Record inputs includes independent side-button X1/X2 switches, on by default. Autoclick settings has its own saved image list; see [Autoclick](Autoclick.md#image-recognition). `.onty` registration is automatic at launch; Clipboard and association toggles are no longer shown. Copy/paste keyboard shortcuts remain available.

## Storage and upgrading to v1.6.1

Settings are saved in `Documents\AutoOnty\AutoOnty.ini`. Saved macros, per-macro settings and editor preferences also use `Documents\AutoOnty`. The autoclick image list, `AutoclickImages.onty`, and its BMP images share `Documents\AutoOnty\ImageDetections`. Windows resolves the Documents folder, including redirected locations.

When upgrading, keep your old files beside the executable for the first run. If no Documents INI exists, AutoOnty imports the old INI at startup. The old autoclick image list and its assets import when you first open the image manager or picker. Existing Documents files take priority, and the originals are retained. A failed import reports an error; it does not silently overwrite your existing data.

Back up `Documents\AutoOnty` to keep the library, settings and autoclick images together. Macros saved elsewhere may have their own adjacent `ImageDetections` folder; keep that folder with those macros too. The minimal INI supplied beside the app is only a migration seed: changes are saved to Documents after startup.

Image recognition is off every launch, even if it was enabled when you quit. Your saved images and selected rows are remembered. Enable **Use images when autoclicking** when you want to use them.
