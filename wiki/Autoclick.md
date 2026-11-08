# Autoclick

AutoOnty can click repeatedly without recording a macro. Choose an interval and start it from the toolbar or hotkey.

---

## 1. Starting and Stopping

- Click the **Autoclick** button on the toolbar (the pointer icon between Play and Menu), or press its hotkey (default <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd>).
- While running, the button becomes a blue stop square and the status bar shows elapsed time and clicks.
- Press the hotkey again to stop, or release it in hold mode. The panic key (default <kbd>Pause</kbd>) also stops clicking, including at a fixed position.
- Clicks over AutoOnty's own windows, menus and dialogs are skipped. Ordinary autoclick counts those skipped attempts toward finite repeats; image mode counts only sent click actions.
- Autoclick and macro Record/Play don't run at the same time. Starting a recording or playback stops autoclick first.

---

## 2. Click Position

Under **Menu -> Autoclick settings -> Click position**:

| Mode                    | What it does                                                                 |
| :---------------------- | :--------------------------------------------------------------------------- |
| **Follow mouse cursor** | Clicks wherever your cursor currently is. Move the mouse and the clicks move. |
| **Fixed position**      | Clicks at a spot you saved. The cursor is moved there before every click.    |

To save a fixed position, hover over the spot and press **Set click position** (default <kbd>Ctrl+Shift+Alt+S</kbd>). This selects Fixed position mode and saves the coordinates, shown in the Click position menu.

---

## 3. Autoclick Settings

All under **Menu -> Autoclick settings**:

- **Countdown:** Wait 0–60 seconds before the first click (0 = off). The status bar shows the remaining time.
- **Click interval:** Hours, Minutes, Seconds, and Milliseconds. Pick a field to type a value. Default: 100 ms. The shortest possible interval is 1 ms.
- **Mouse button:** Left, Middle, Right, or side buttons X1/X2.
- **Click type:** Single or Double.
- **Click repeat:** **Repeat until stopped**, or **Number of clicks** (1-999999) to stop on its own.
- **Click position:** Follow mouse cursor / Fixed position (see above).
- **Random click timing (±20%):** Off by default. Varies each interval between 80% and 120% of the value you set.

The settings are read when autoclick starts, so changing one while it's running takes effect the next time you start it.

---

## 4. Hotkeys

Under **Menu -> Hotkeys** you can rebind both:

| Action                 |                            Default Key                             |
| :--------------------- | :----------------------------------------------------------------: |
| **Autoclick**          | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd> |
| **Set click position** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd> |

AutoOnty warns you if a shortcut is already used by another action or macro.

---

## 5. Settings File

Autoclick settings are saved in `Documents\AutoOnty\Ini\AutoOnty.ini`. For backups and importing settings from beside the app, see [Settings](Settings.md#storage-and-backups).

## Image recognition

Autoclick and macro image recognition share one library of up to 64 images, with separate settings for each workflow.

1. Open **Menu -> Autoclick settings -> Image recognition...** to add, edit, delete and select images.
2. Tick the images to use, then choose **Done**. Untick every image to return to ordinary autoclicking.
3. Start with the Autoclick toolbar button or hotkey. Use the panic key to stop, or release the hotkey in hold mode.

Recognition starts disabled each launch; your saved images and selection are remembered. See [Image Recognition](Image-Recognition.md#use-with-autoclick) for matching, per-image limits and using images in macros.

## Hold mode, random position and window targets

- **Toggle Mode** enables hold-to-click when checked; releasing any part of the hotkey stops clicking. Unchecked (default) means press to start/stop. The toolbar always toggles.
- **Random click position** adds jitter around the cursor or fixed position. It is off by default; **Position radius (pixels)** defaults to 3 (range 1-50). Cursor mode restores the center after each click to prevent drift.
- **Click when mouse is over window** restricts clicking to selected apps. Choose **Select window...** and click a window; Esc cancels. Pick a selected app again to remove it, or use **Clear all / disable**.
- All windows from a selected executable match, even without keyboard focus. Moving to an unselected app makes autoclick wait without consuming repeats. See [window targets](Settings.md#window-targets) for details.
