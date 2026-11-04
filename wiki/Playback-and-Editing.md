# Playback & Editing

## Pause / Resume

Use **Menu -> Hotkeys -> Pause / resume** to change the shortcut. The default is **Ctrl+Shift+Alt+H**; AutoOnty chooses a fallback if it is already assigned. The menu shows your actual shortcut.

Pause releases keys and mouse buttons held by the macro. Resume restores them and continues from the same point. Stop while paused ends playback without restoring held input. Pause / resume is also available in the playback menu and Edit mode panel.

## Countdowns

**Menu -> Macro settings -> Countdown** sets the macro countdown. **Menu -> Autoclick settings -> Countdown** sets it separately for autoclicking. Both accept 0–60 seconds, default to 0 (off), and run only before the first loop or click. The status bar shows the remaining seconds when enabled. Stop cancels the countdown; macro pause freezes it.

## Play Selected

Select actions in the editor and click **Play selected**. A countdown prompt appears each time (0–60 seconds). It starts with the general countdown value; your answer applies only to this preview. Cancel leaves playback stopped.

Selected actions play once, in their original order and at the current playback speed. Delays between them are kept even when Delay rows are not selected. Hidden mouse movements between the first and last selected actions also play unless excluded. Other hidden inputs and visible unselected movements stay out, with their timing preserved. To include the delay before the first action, select that Delay row too.

The editor stays open with editing disabled. **Play selected** becomes **Stop**; the playback, pause and panic hotkeys remain available. Previewing does not save your edits or replace the loaded macro.

## Edit Mode

Toggle **Edit mode** at the top of the editor to open the extra panel. The original right-hand controls stay in place. Use Ctrl+click, Shift+click, Ctrl+A or drag a selection box to select entries. Ctrl+drag toggles entries; dragging past the list edge scrolls it.

- **Bulk edits:** Change key codes, key press/release, mouse buttons, mouse press/release, coordinates or wheel amounts. Mixed selections change only compatible entries. Coordinates change stored positions, including a movement's starting position; they do not redraw its path.
- **Universal delay:** Replaces existing selected Delay rows and delays directly before selected actions. With nothing selected, it replaces all visible Delay rows. It never inserts delays or changes hidden movement timing.
- **Add key:** Accepts a key or a left/right/middle/X1/X2 mouse button. For a mouse click, it adds press/release entries using the cursor position when pressed, even if you move before release. The capture click does not click another app. Existing visibility filters still apply.

Edits support Undo / Redo. Change key opens image settings for a single Image recognition row; otherwise it and Block specific key remain keyboard-only.

## Exclusions

Open **Edit mode -> Exclusions**. **Checked means the input will not play; unchecked means it can play under the selection rules above.** Nothing is excluded by default. These choices affect Play selected only.

Search the list of full-size keyboard keys, mouse buttons, wheel directions and mouse movements. Keys are ordered from common typing keys to function, numpad and system keys. Excluding a key blocks both press and release. Generic Ctrl/Shift/Alt/Win block both sides; separate left/right entries are available. **All keyboard keys**, **Block specific key**, **Select all** and **Clear all** make larger changes easier. Excluded keys are marked **Excluded** in the checklist and editor.

Exclusions preserve delays. Excluding mouse movements also prevents cursor positioning by selected clicks or wheel events. Held inputs are still released at the end for cleanup.

## Visibility for Each Macro

**Menu -> Macro settings -> Show in macro editor** sets the default. **Edit mode -> Visibility** overrides it for the current macro, with separate toggles for keyboard, each mouse button, wheel direction and movement. **Reset to default** removes the override so future general changes apply again. Changing visibility preserves edits and undo history.

Shown entries can be selected, edited and played normally. Hidden entries follow the Play selected rules above. Exclusions and visibility choices are saved locally in `Documents\AutoOnty\editor-settings.ini` against each macro's full path, including files outside the library. Unsaved recordings keep temporary choices until saved.

## Image recognition

### Create an image-only macro

Choose **Menu -> Macro settings -> Image recognition macro...** (the new image/target icon). Choose **Take screenshot** or **Choose file** first. The prompt explains that taking a screenshot minimizes AutoOnty and opens Windows snipping; nothing captures until you choose it. Drag a small rectangle around a distinctive button or icon. Esc cancels the snip; AutoOnty returns after 30 seconds, or sooner when restored from its tray/taskbar. BMP, PNG and JPG files can also be imported.

The AutoOnty-themed editor has three pages:

- **Image:** preview, Retake, Choose file, sensitivity and Test. Strict 95 / Normal 85 / Loose 70 set the slider; other values show Custom. **Test** temporarily hides AutoOnty, scans without clicking and shows confidence, a best-match estimate, or an explanatory error. Enable **Also try different image sizes** for a scaled target; this takes longer. Keep **Minimize AutoOnty while searching** enabled if AutoOnty would cover the target.
- **When found:** choose **Click once**, **Auto click**, **Run a saved macro**, or **Wait only**. Click at the image center, current cursor, or a fixed position (which can be picked). Auto click has its own interval, click count and duration; zero clicks means until the image disappears, and zero duration means no extra time limit. These controls are independent of the separate autoclicker. A selected macro is copied into the image action and runs once at the parent playback speed, then returns; changing the original file does not change this copy.
- **Waiting & repeat:** choose **Once**, **Again after it disappears**, **Keep acting while visible**, or **At each location once**. Set a match limit, total watch time and time between actions. Zero means unlimited for match/time limits. Repeating stays inside this step; choose Once to continue after the first match. A waiting timeout resets after each match, while total watch time covers the whole watcher. A triggered saved macro finishes its current run before the watch-time limit takes effect.

Choose **Continue macro**, **Pause until I resume**, or **Stop macro** separately for a match and for timeout. Pausing uses the existing pause/resume hotkey. The dedicated watcher defaults to waiting indefinitely and acting once per appearance; it waits until matching images disappear before rearming. Use **At each location once** to act on identical copies at different positions.

Finish with **Start watching** or **Save macro...**. You do not need a dummy recording or keyboard steps. Standalone image macros run their configured watcher once even if the general playback-loop setting is continuous.

### Add an image wait to an existing macro

The original **Add image** button remains beside **Play selected**. It opens the same source chooser and themed settings. New image steps default to **Wait only**: the macro waits for the image and then continues. Select **Click once** or another action if wanted. Existing saved image steps keep their click behavior. Select the row and choose **Image settings...**, or double-click it, to edit any choice. Undo/Redo includes the image, action and copied macro.

**Timeout (ms)** accepts `0` (wait forever), `9999999`, and values up to `4294967295`. Invalid numbers show an explanation instead of silently refusing to save. In a regular macro the default waiting timeout remains 3000 ms; the dedicated image watcher defaults to 0. On timeout the chosen continue/pause/stop outcome applies.

### Matching and status

The status overlay and dock report searching, best confidence, remaining wait, waiting for disappearance, auto clicking, running a macro, and pause/timeout states. Macro focus targets still apply; focus/manual pauses freeze timers and resuming takes a new screenshot. Panic/stop remains available while searching, auto clicking or running a copied macro. Failed or clipped cursor placement never clicks at the old position.

Matching compares visual structure and RGB similarity, ignores alpha, and picks the strongest match. Capture at the current display/app scale; a distinctive icon/text crop works better than a plain background. The optional size search tries several sizes and retains the best result. The native matcher uses Win32 desktop capture. The virtual desktop can span negative coordinates and supports up to 65536 pixels per side and 64 million pixels.

Saving writes snippets to `ImageDetections/image-<hash>.bmp` beneath the macro folder (normally `Documents\AutoOnty\ImageDetections`). Copied macro actions are stored inside the `.onty`; their images use the same subfolder. Keep `ImageDetections` with the macro when sharing. Older macros with BMPs directly beside them still load; saving them writes the new subfolder layout. Sidecars may be shared and remain after deletion of a step/macro. Clipboard copies cache all required images in the local library; clipboard JSON alone does not include the image files. Missing/invalid assets reject the load without replacing the current macro.
