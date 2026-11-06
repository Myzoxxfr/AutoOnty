# Autoclick

AutoOnty has a built-in autoclicker next to the macro tools. It is separate from macro recording: you don't need to record anything, just pick a speed and start it.

---

## 1. Starting and Stopping

- Click the **Autoclick** button on the toolbar (the pointer icon between Play and Menu), or press its hotkey (default <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd>).
- While it runs, the button turns into a blue stop square and the status bar shows `CLICK 00:12 • 118 • Stop: <hotkey>`.
- Press the hotkey again to stop. Your panic stop key (default <kbd>Pause</kbd>) stops it too. This is the safe way out when clicking at a fixed position, because the cursor stays pinned to that spot.
- Autoclick never clicks on AutoOnty's own window, menus, or dialogs. If you start it from the toolbar button, it skips clicks until your cursor is over something else. Finite repeat counts include skipped attempts, so clicking over AutoOnty cannot leave it running forever.
- Autoclick and macro Record/Play don't run at the same time. Starting a recording or playback stops autoclick first.

---

## 2. Click Position

Under **Menu -> Autoclick settings -> Click position**:

| Mode                    | What it does                                                                 |
| :---------------------- | :--------------------------------------------------------------------------- |
| **Follow mouse cursor** | Clicks wherever your cursor currently is. Move the mouse and the clicks move. |
| **Fixed position**      | Clicks at a spot you saved. The cursor is moved there before every click.    |

To save a fixed position, hover over the spot and press the **Set click position** hotkey (default <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd>). The position is saved to `AutoOnty.ini` and switches the mode to **Fixed position**. The saved coordinates are shown (greyed out) in the Click position menu.

---

## 3. Autoclick Settings

All under **Menu -> Autoclick settings**:

- **Countdown:** Wait 0–60 seconds before the first click (0 = off). The status bar shows the remaining time.
- **Click interval:** Hours, Minutes, Seconds, and Milliseconds. Pick a field to type a value. Default: 100 ms. The shortest possible interval is 1 ms.
- **Mouse button:** Left, Middle, Right, or side buttons X1/X2.
- **Click type:** Single or Double.
- **Click repeat:** **Repeat until stopped**, or **Number of clicks** (1-999999) to stop on its own.
- **Click position:** Follow mouse cursor / Fixed position (see above).
- **Random click timing (±20%):** A toggle (off by default). When on, every wait between clicks is randomly 80-120 % of the interval you set, so the clicks don't land at a perfectly regular rhythm. Your interval stays the average.

The settings are read when autoclick starts, so changing one while it's running takes effect the next time you start it.

---

## 4. Hotkeys

Under **Menu -> Hotkeys** you can rebind both:

| Action                 |                            Default Key                             |
| :--------------------- | :----------------------------------------------------------------: |
| **Autoclick**          | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>A</kbd> |
| **Set click position** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Alt</kbd> + <kbd>S</kbd> |

Autoclick hotkeys must not conflict with Record, Play, Pause / resume, Panic stop, or macro hotkeys. AutoOnty warns you if you pick one that's already in use.

---

## 5. Settings File

Autoclick settings are stored in the `[Autoclick]` section of `Documents\AutoOnty\AutoOnty.ini`: `mode`, `hours`, `mins`, `secs`, `ms`, `button`, `double`, `repeat_until_stop`, `repeat_count`, `countdown`, `jitter`, `has_pos`, `pos_x`, `pos_y`, the chosen-image mask and the hotkey values. Image recognition always starts disabled; enabling it applies only to the current app session.

## Image recognition

Available in v1.6.1. The image library is separate from your recorded or loaded macro and supports up to 64 images.

Open **Menu -> Autoclick settings -> Image recognition...**. **Add image** uses the same screenshot/file chooser as image macros. Each row has a thumbnail; select **Image settings...** or double-click to change it. Delete removes a row from this list. Save applies changes; Cancel keeps the previous list.

Each image has its own sensitivity, click destination (image center, cursor or fixed point), interval and time limit in milliseconds. Under **When found**, choose **Click once**, **Click a specific number of times** (enter a count of at least 1), or **Keep clicking while visible**. Time `0` means unlimited. Clicking stops when the image disappears or its count/time limit is reached. A limited image becomes eligible again after it disappears and reappears. Several selected, visible images can run their own click schedules. Screen checks run about every 150 ms plus matching time; large images/lists can slow clicking. Short intervals are best-effort.

The image-list window only manages saved images. To enable recognition, click **Menu -> Autoclick settings -> Use images when autoclicking**, directly below **Image recognition...**. A thumbnail picker opens: click or press Space to select one or more images, then choose OK. Cancel leaves recognition disabled and keeps the previous selection. Use the normal Autoclick button/hotkey to start; enabling recognition does not start clicking. Click the menu checkmark again to disable it. To change the selection, disable and enable it again.

The global mouse button (including X1/X2), single/double click, countdown, hold mode, jitter, window targets, panic key and total repeat limit still apply. A count is measured in click actions: Double sends two physical clicks per action, including **Click once**. The global interval initializes new rows; existing rows keep their own intervals. Per-image destinations replace the global cursor/fixed-position choice while image mode is enabled. The existing **Toggle Mode** checkbox enables hold-to-run when checked; unchecked means press once to start and again to stop. The toolbar always toggles.

The list and its BMP files are stored together in `Documents\AutoOnty\ImageDetections`; the list is named `AutoclickImages.onty`. Selected rows are remembered in `Documents\AutoOnty\AutoOnty.ini`, but recognition is unchecked on every launch, even when saved images exist. The recorded/loaded macro is unaffected. Saving an empty list keeps an empty metadata file so old images are not imported again; shared BMPs remain. Removing the last selected image disables image mode.

When the new files do not exist yet, AutoOnty imports the old INI/list/BMPs beside the executable. Existing Documents data takes priority. Original files remain intact; a failed import reports an error instead of replacing the saved list. INI import occurs at startup; the image list imports when its manager/picker is first opened. To back up autoclick settings, keep the INI and the ImageDetections folder together.

## Hold mode, random position and window targets (v1.6)

- **Toggle Mode** is unchecked by default. Tick it for hold-to-click: releasing any part of the hotkey stops clicking. The toolbar always toggles, and panic stop still works.
- **Random click position** is off by default. **Position radius (pixels)** defaults to 3 (range 1–50). Each click chooses a point in a circle around the saved fixed point or current cursor center, clamped to the virtual desktop. Cursor mode restores its center after each click to prevent drift.
- **Click when mouse is over window** has its own app list. Choose **Select window...**, then click an external window with the crosshair; Esc cancels. Selecting an already checked app removes it. Checked entries show a cross on the right; click an entry to remove it, or use **Clear all / disable**.
- Only exe names are saved in `AutoOnty.ini`, so different windows of the same application match. Hovering a selected app allows clicking even while another app has keyboard focus. Moving onto another app pauses clicking without consuming repeats. Starting while the mouse is elsewhere is allowed; the HUD shows **Waiting for** the selected apps. Panic stop also works while waiting.
