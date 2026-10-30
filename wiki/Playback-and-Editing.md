# Playback & Editing

## Pause / Resume

Use **Menu -> Hotkeys -> Pause / resume** to change the shortcut. The default is **Ctrl+Shift+Alt+H**; AutoOnty chooses a fallback if it is already assigned. The menu shows your actual shortcut.

Pause releases keys and mouse buttons held by the macro. Resume restores them and continues from the same point. Stop while paused ends playback without restoring held input. Pause / resume is also available in the playback menu and Edit mode panel.

## Countdowns

**Menu -> Countdown** sets the macro countdown. **Menu -> Autoclick settings -> Countdown** sets it separately for autoclicking. Both accept 0–60 seconds, default to 0 (off), and run only before the first loop or click. The status bar shows the remaining seconds when enabled. Stop cancels the countdown; macro pause freezes it.

## Play Selected

Select actions in the editor and click **Play selected**. A countdown prompt appears each time (0–60 seconds). It starts with the general countdown value; your answer applies only to this preview. Cancel leaves playback stopped.

Selected actions play once, in their original order and at the current playback speed. Delays between them are kept even when Delay rows are not selected. Hidden mouse movements between the first and last selected actions also play unless excluded. Other hidden inputs and visible unselected movements stay out, with their timing preserved. To include the delay before the first action, select that Delay row too.

The editor stays open with editing disabled. **Play selected** becomes **Stop**; the playback, pause and panic hotkeys remain available. Previewing does not save your edits or replace the loaded macro.

## Edit Mode

Toggle **Edit mode** at the top of the editor to open the extra panel. The original right-hand controls stay in place. Use Ctrl+click, Shift+click, Ctrl+A or drag a selection box to select entries. Ctrl+drag toggles entries; dragging past the list edge scrolls it.

- **Bulk edits:** Change key codes, key press/release, mouse buttons, mouse press/release, coordinates or wheel amounts. Mixed selections change only compatible entries. Coordinates change stored positions, including a movement's starting position; they do not redraw its path.
- **Universal delay:** Replaces existing selected Delay rows and delays directly before selected actions. With nothing selected, it replaces all visible Delay rows. It never inserts delays or changes hidden movement timing.
- **Add key:** Accepts a key or a left/right/middle/X1/X2 mouse button. For a mouse click, it adds press/release entries using the cursor position when pressed, even if you move before release. The capture click does not click another app. Existing visibility filters still apply.

Edits support Undo / Redo. Change key and Block specific key remain keyboard-only.

## Exclusions

Open **Edit mode -> Exclusions**. **Checked means the input will not play; unchecked means it can play under the selection rules above.** Nothing is excluded by default. These choices affect Play selected only.

Search the list of full-size keyboard keys, mouse buttons, wheel directions and mouse movements. Keys are ordered from common typing keys to function, numpad and system keys. Excluding a key blocks both press and release. Generic Ctrl/Shift/Alt/Win block both sides; separate left/right entries are available. **All keyboard keys**, **Block specific key**, **Select all** and **Clear all** make larger changes easier. Excluded keys are marked **Excluded** in the checklist and editor.

Exclusions preserve delays. Excluding mouse movements also prevents cursor positioning by selected clicks or wheel events. Held inputs are still released at the end for cleanup.

## Visibility for Each Macro

**Menu -> Show in macro editor** sets the default. **Edit mode -> Visibility** overrides it for the current macro, with separate toggles for keyboard, each mouse button, wheel direction and movement. **Reset to default** removes the override so future general changes apply again. Changing visibility preserves edits and undo history.

Shown entries can be selected, edited and played normally. Hidden entries follow the Play selected rules above. Exclusions and visibility choices are saved locally in `Documents\AutoOnty\editor-settings.ini` against each macro's full path, including files outside the library. Unsaved recordings keep temporary choices until saved.
