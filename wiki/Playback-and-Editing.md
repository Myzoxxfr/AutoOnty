# Playback & Editing

## Pause / Resume

Use **Menu -> Hotkeys -> Pause / resume** to change the shortcut. The default is **Ctrl+Shift+Alt+H**; AutoOnty chooses a fallback if it is already assigned. The menu shows your actual shortcut.

Pause releases keys and mouse buttons held by the macro. Resume restores them and continues from the same point. Stop while paused ends playback without restoring held input. Pause / resume is also available in the playback menu and Edit mode panel.

## Countdowns

**Menu -> Macro settings -> Countdown** sets the macro countdown. **Menu -> Autoclick settings -> Countdown** sets it separately for autoclicking. Both accept 0–60 seconds, default to 0 (off), and run only before the first loop or click. The status bar shows the remaining seconds when enabled. Stop cancels the countdown; macro pause freezes it.

## Play Selected

Select actions and click **Play selected**. Choose a countdown (0-60 seconds); this applies only to the preview. Cancel leaves playback stopped.

Selected actions play once, in their original order and at the current playback speed. Delays between them are kept even when Delay rows are not selected. Hidden mouse movements between the first and last selected actions also play unless excluded. Other hidden inputs and visible unselected movements stay out, with their timing preserved. To include the delay before the first action, select that Delay row too.

The editor stays open with editing disabled. **Play selected** becomes **Stop**; the playback, pause and panic hotkeys remain available. Previewing does not save your edits or replace the loaded macro.

## Edit Mode

Toggle **Edit mode** to open bulk controls. Use Ctrl+click, Shift+click, Ctrl+A or drag to select; Ctrl+drag toggles entries and dragging past the list edge scrolls.

- **Bulk edits:** Change key codes, key press/release, mouse buttons, mouse press/release, coordinates or wheel amounts. Mixed selections change only compatible entries. Coordinates change stored positions, including a movement's starting position; they do not redraw its path.
- **Universal delay:** Replaces existing selected Delay rows and delays directly before selected actions. With nothing selected, it replaces all visible Delay rows. It never inserts delays or changes hidden movement timing.
- **Add key:** Capture a key or mouse button, including X1/X2. Mouse clicks use the cursor position at press time without clicking another app. Visibility filters still apply.

Edits support Undo / Redo. Change key opens image settings for a single Image recognition row; otherwise it and Block specific key remain keyboard-only.

## Exclusions

Open **Edit mode -> Exclusions**. **Checked means the input will not play; unchecked means it can play under the selection rules above.** Nothing is excluded by default. These choices affect Play selected only.

Choose keyboard keys, mouse buttons, wheel directions or movement to exclude. Keys are blocked on both press and release; generic Ctrl/Shift/Alt/Win cover both sides. Search, **Block specific key**, **Select all** and **Clear all** help manage the list.

Exclusions preserve delays. Excluding mouse movements also prevents cursor positioning by selected clicks or wheel events. Held inputs are still released at the end for cleanup.

## Visibility for Each Macro

**Menu -> Macro settings -> Show in macro editor** sets the default. **Edit mode -> Visibility** overrides it for the current macro, with separate toggles for keyboard, each mouse button, wheel direction and movement. **Reset to default** removes the override so future general changes apply again. Changing visibility preserves edits and undo history.

Visibility follows the Play selected rules above. Visibility and exclusions are saved per macro in `Documents\AutoOnty\Ini\editor-settings.ini`, including macros outside the library. Unsaved recordings keep temporary choices until saved.

## Image recognition

Use **Add image** beside **Play selected** to pick from the shared image library. Select an image row and choose **Image settings...**, or double-click it, to edit its behavior. Choosing a saved macro copies its actions after the image step; Undo / Redo includes the copy.

Create an image-only watcher under **Menu -> Macro settings -> Image recognition macro...**. See [Image Recognition](Image-Recognition.md#use-with-macros) for matching, actions, timeouts and saving images.
