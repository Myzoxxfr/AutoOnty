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
- **Mouse button:** Left, Middle, or Right.
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

Autoclick settings are stored in the `[Autoclick]` section of `AutoOnty.ini`: `mode`, `hours`, `mins`, `secs`, `ms`, `button`, `double`, `repeat_until_stop`, `repeat_count`, `countdown`, `jitter`, `has_pos`, `pos_x`, `pos_y`, and the hotkey values.
