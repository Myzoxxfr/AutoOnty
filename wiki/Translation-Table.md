# Translation Table and Word Dictionary

English text keys used by AutoOnty. Start with the [JSON template](.translation.json) when translating.

Values below match the built-in English text. Keep placeholders and the app name unchanged.

---

## Word Dictionary

| Key                 | English (`en`)                                                           | Where it appears          | Notes                    |
| :------------------ | :----------------------------------------------------------------------- | :------------------------ | :----------------------- |
| `app`               | AutoOnty | Window titles and headers | App name, keep unchanged |
| `record`            | Record | Toolbar and menu          | Main action              |
| `stop`              | Stop | Toolbar and menu          | Shown while active       |
| `play`              | Play | Toolbar and menu          | Main action              |
| `open`              | Open... | Toolbar and menu          | Open file                |
| `save`              | Save... | Toolbar and menu          | Save file                |
| `speed`             | Speed | Main menu                 | Speed submenu            |
| `speed05`           | 0.5x | Speed menu                | Half speed               |
| `speed1`            | 1x (Normal) | Speed menu                | Real-time speed          |
| `speed2`            | 2x (Fast) | Speed menu                | 2x speed                 |
| `speed5`            | 5x | Speed menu                | 5x speed                 |
| `speed10`           | 10x | Speed menu                | 10x speed                |
| `speed_turbo`       | 100x Turbo | Speed menu                | Turbo speed              |
| `speed_max`         | Max Speed (Instant) | Speed menu                | Zero delay               |
| `speed_custom`      | Custom... | Speed menu                | Custom speed dialog      |
| `loops`             | Playback loops | Main menu                 | Loops submenu            |
| `loops1`            | 1 | Loops menu                | Play once                |
| `loops2`            | 2 | Loops menu                | 2 times                  |
| `loops3`            | 3 | Loops menu                | 3 times                  |
| `loops5`            | 5 | Loops menu                | 5 times                  |
| `loops10`           | 10 | Loops menu                | 10 times                 |
| `loops100`          | 100 | Loops menu                | 100 times                |
| `continuous`        | Continuous (∞) | Loops menu                | Infinity loop            |
| `loops_custom2`     | Custom... | Loops menu                | Custom loops dialog      |
| `theme`             | Theme | Main menu                 | Theme submenu            |
| `interface_theme`   | Interface theme | Theme menu                | Category label           |
| `theme_dark`        | Dark | Theme menu                | Dark theme               |
| `theme_light`       | Light | Theme menu                | Light theme              |
| `theme_acrylic`     | Acrylic | Theme menu                | Fluent blur effect       |
| `language`          | Language | Main menu                 | Language submenu         |
| `lang_en`           | English | Language menu             | English option           |
| `lang_import`       | Import translation JSON... | Language menu             | Import JSON file         |
| `lang_translate`    | Translate on GitHub... | Language menu             | Opens GitHub link        |
| `hotkeys`           | Hotkeys | Main menu                 | Hotkeys submenu          |
| `hk_record`         | Recording | Hotkeys menu              | Record shortcut          |
| `hk_play`           | Playback | Hotkeys menu              | Play shortcut            |
| `panic_keys`        | Panic stop keys | Hotkeys menu              | Panic stops              |
| `panic_mouse`       | Stop on mouse move | Panic keys                | Stop when mouse moves    |
| `statusbar`         | Status bar | Main menu                 | Status bar settings      |
| `sb_enable`         | Show status bar | Status bar menu           | Toggle on/off            |
| `sb_top`            | Top of screen | Status bar menu           | Position at top          |
| `sb_bottom`         | Bottom of screen | Status bar menu           | Position at bottom       |
| `sb_follow_monitor` | Follow cursor monitor | Status bar menu           | Move across screens      |
| `assoc_onty`        | Associate .onty files | Legacy key (menu removed)                 | Register file type       |
| `assoc_success`     | .onty files registered | Status notification       | Toast message            |
| `assoc_removed`     | .onty association removed | Status notification       | Toast message            |
| `topmost`           | Always on top | Main menu / pin button    | Pin window on top        |
| `update`            | Check for updates... | Menu & About dialog       | Check for updates        |
| `update_available`  | Update available! | Status notification       | Update found             |
| `exit`              | Exit | Main menu                 | Closes AutoOnty          |
| `about`             | About AutoOnty | Main menu                 | Opens About box          |
| `status_rec_stop`   | Recording stopped | Status notification       | Toast message            |
| `status_play`       | Playing | Status bar                | While playing            |
| `status_play_stop`  | Playback stopped | Status notification       | Toast message            |
| `status_saved`      | Macro saved | Status notification       | Toast message            |
| `status_nothing`    | Nothing recorded | Status notification       | Empty macro warning      |
| `status_invalid`    | Invalid file | Status notification       | Corrupted file warning   |
| `status_conflict`   | Hotkey conflict | Status notification       | Duplicate key warning    |
| `status_max`        | Event limit reached | Status notification       | Max events warning       |
| `status_uptodate`   | You have the latest version | Status notification       | Up-to-date message       |
| `status_updatefail` | Update check failed | Status notification       | Network error            |
| `status_translated` | Translation imported | Status notification       | Import success           |
| `status_loaded`     | Macro loaded | Status notification       | Load success             |
| `about_desc`        | An open-source successor to TinyTask for fast desktop and 3D automation. | About dialog              | App description          |
| `github`            | GitHub | About dialog              | GitHub button            |
| `license`           | License | About dialog              | License button           |
| `prompt_loops`      | Number of playback loops (1-99999) | Input dialog              | Prompt                   |
| `prompt_speed`      | Speed multiplier (0.01-1000) | Input dialog              | Prompt                   |
| `ok`                | OK | Dialog buttons            | Confirm                  |
| `cancel`            | Cancel | Dialog buttons            | Cancel                   |
| `save_fail`         | Unable to save file | Status notification       | Save error               |
| `minimize`          | Minimize | Window tooltip            | Minimize button          |
| `hide`              | Hide to tray | Window tooltip            | Close button             |
| `settings`          | Settings | Button tooltip            | Menu button              |
| `resave`            | Save changes | Button tooltip            | Quick save dot           |
| `saved`             | Saved | Button tooltip            | Up to date dot           |
| `hk_press`          | Press a new hotkey (Esc to cancel) | Hotkey capture modal      | Prompt                   |
| `yes`               | Yes | Dialog button             | Yes                      |
| `no`                | No | Dialog button             | No                       |
| `new_macro`         | New macro | Status bar                | Unsaved file label       |
| `developed`         | Fork of OntyTask by Agzes | About dialog | Agzes is the original OntyTask author; AutoOnty modifications are by Myzoxx |
| `lic_line`          | Licensed under GNU GPL v3.0 | About dialog              | License note             |
| `tray_icon`         | Tray icon | Menu setting              | Tray icon style          |
| `tray_auto`         | Follow theme | Menu setting              | Sync with theme          |
| `ui_icon`           | Interface icon | Menu setting              | Window icon style        |
| `stop_bind`         | Stop | Status bar                | Stop key hint            |
| `autominimize`      | Auto-minimize on Rec/Play | Menu checkbox             | Auto-minimize setting    |
| `clipboard`         | Clipboard | Legacy key (menu removed)              | Clipboard tools          |
| `copy_clipboard`    | Copy macro to clipboard | Legacy menu text | Use Ctrl+C in the main window |
| `paste_clipboard`   | Paste macro from clipboard | Legacy menu text | Use Ctrl+V in the main window |
| `clipboard_loaded`  | Macro loaded from clipboard | Status notification       | Load success             |
| `clipboard_copied`  | Macro copied to clipboard | Status notification       | Copy success             |
| `clipboard_empty`   | Clipboard has no valid macro | Status notification       | Copy/paste error         |
| `create_shortcut`   | Create Desktop shortcut | Menu item                 | Create shortcut          |
| `shortcut_success`  | Desktop shortcut created | Status notification       | Success toast            |
| `shortcut_fail`     | Failed to create shortcut | Status notification       | Error toast              |
| `autoclick`         | Autoclick | Toolbar, menu, hotkeys    | Autoclick action name    |
| `ac_settings`       | Autoclick settings | Main menu                 | Autoclick submenu        |
| `ac_interval`       | Click interval | Autoclick menu            | Interval submenu         |
| `ac_hours`          | Hours | Autoclick menu            | Interval field and its prompt |
| `ac_mins`           | Minutes | Autoclick menu            | Interval field and its prompt |
| `ac_secs`           | Seconds | Autoclick menu            | Interval field and its prompt |
| `ac_ms`             | Milliseconds | Autoclick menu            | Interval field and its prompt |
| `ac_button`         | Mouse button | Autoclick menu            | Button submenu           |
| `ac_btn_left`       | Left | Autoclick menu            | Left mouse button        |
| `ac_btn_right`      | Right | Autoclick menu            | Right mouse button       |
| `ac_btn_middle`     | Middle | Autoclick menu            | Middle mouse button      |
| `ac_type`           | Click type | Autoclick menu            | Click type submenu       |
| `ac_single`         | Single | Autoclick menu            | Single click             |
| `ac_double`         | Double | Autoclick menu            | Double click             |
| `ac_repeat`         | Click repeat | Autoclick menu            | Repeat submenu           |
| `ac_repeat_until`   | Repeat until stopped | Autoclick menu            | Click forever            |
| `ac_repeat_count`   | Number of clicks | Autoclick menu            | Opens count dialog       |
| `prompt_ac_repeat`  | Number of clicks (1-999999) | Input dialog              | Click count prompt       |
| `ac_position`       | Click position | Autoclick menu            | Position submenu         |
| `ac_follow`         | Follow mouse cursor | Autoclick menu            | Click wherever the cursor is |
| `ac_fixed`          | Fixed position | Autoclick menu            | Click at the saved spot  |
| `ac_saved_pos`      | Saved position | Autoclick menu            | Followed by X, Y         |
| `ac_not_set`        | not set | Autoclick menu            | No position saved yet    |
| `hk_ac_pos`         | Set click position | Hotkeys menu              | Saves cursor position    |
| `status_ac_stop`    | Autoclick stopped | Status notification       | Autoclick finished       |
| `status_ac_pos_saved` | Click position saved | Status notification       | Followed by X, Y         |
| `status_ac_nopos`   | Set a click position first | Status notification       | Fixed mode, no position  |
| `status_ac_busy`    | Stop the macro first | Status notification       | Rec/Play is active       |
| `macros`            | Macros | Main bar / Macro library  | Button tooltip, menu item and window title |
| `cur_macro`         | Macro: | Main bar                  | Prefix before the current macro name |
| `mc_none`           | none | Main bar                  | No macro recorded or loaded |
| `mc_load`           | Load | Macro library             | Make the selected macro current |
| `mc_edit`           | Edit | Macro library             | Open the macro editor    |
| `mc_rename`         | Rename | Macro library             | Rename the selected macro |
| `mc_delete`         | Delete | Macro library / editor    | Delete a macro file or selected events |
| `mc_save`           | Save current... | Macro library             | Save the current macro under a name |
| `mc_browse`         | Browse... | Macro library             | Open a macro from any folder |
| `mc_folder`         | Open folder | Macro library             | Open the macro folder in Explorer |
| `mc_empty`          | No saved macros yet | Macro library             | Shown when the list is empty |
| `mc_name_prompt`    | Macro name | Name prompt               | Save and rename prompt   |
| `mc_exists`         | A macro with this name already exists | Message box               | Rename conflict          |
| `mc_replace_q`      | A macro with this name already exists. Replace it? | Message box               | Save conflict            |
| `mc_delete_q`       | Delete this macro? | Message box               | Delete confirmation      |
| `mc_discard_q`      | Discard the unsaved macro changes? | Message box               | Shown before loading another macro |
| `mc_deleted`        | Macro deleted | Status notification       | After deleting a macro   |
| `mc_fail`           | Operation failed | Status notification       | Rename or delete failed  |
| `ed_title`          | Macro editor | Macro editor              | Window title             |
| `ed_add`            | Add key... | Macro editor              | Insert a key press       |
| `ed_change`         | Change key... | Macro editor              | Replace the key of an event |
| `ed_delay`          | Edit delay... | Macro editor              | Change the delay of selected events |
| `ed_up`             | Move up | Macro editor              | Move selected events up  |
| `ed_down`           | Move down | Macro editor              | Move selected events down |
| `ed_press_key`      | Press the key you want | Key capture               | Key capture window text  |
| `ed_delay_prompt`   | Wait before this action (ms) | Macro editor              | Delay prompt             |
| `ed_kd`             | Key press | Macro editor              | Event type               |
| `ed_ku`             | Key release | Macro editor              | Event type               |
| `ed_md`             | Mouse press | Macro editor              | Event type               |
| `ed_mu`             | Mouse release | Macro editor              | Event type               |
| `ed_mv`             | Mouse move | Macro editor              | Event type               |
| `ed_wh`             | Wheel | Macro editor              | Event type               |
| `ed_hint`           | Delay = pause before the next action  &#124;  Ins / Del / Ctrl+A | Macro editor | Footer hint |
| `ed_discard_q`      | Discard your edits? | Message box               | Shown when cancelling the editor |
| `rec_inputs`        | Record inputs | Settings menu             | Submenu: which inputs a recording captures |
| `rec_move`          | Mouse movement | Settings menu > Record inputs | Checkbox                 |
| `rec_left`          | Left click | Settings menu > Record inputs | Checkbox                 |
| `rec_right`         | Right click | Settings menu > Record inputs | Checkbox                 |
| `rec_middle`        | Middle click | Settings menu > Record inputs | Checkbox                 |
| `rec_wheel_up`      | Wheel up | Settings menu > Record inputs | Checkbox                 |
| `rec_wheel_down`    | Wheel down | Settings menu > Record inputs | Checkbox                 |
| `ed_add_delay`      | Add delay... | Macro editor              | Insert a Delay row       |
| `ed_delay_row`      | Delay | Macro editor              | Event type shown for a Delay row |
| `ed_show`          | Show in macro editor | Settings menu             | Submenu title            |
| `ed_show_keys`     | Keyboard keys | Settings menu > Show in macro editor| Checkbox                 |
| `ed_undo` | Undo | Macro editor | Undo button (right column) |
| `ed_redo` | Redo | Macro editor | Redo button (right column) |
| `mc_hotkey` | Hotkey... | Macro library | Button: set the load hotkey of a macro |
| `mc_settings` | Settings... | Macro library | Button: per-macro loops and speed |
| `mc_search` | Search macros... | Macro library | Search box hint (6+ macros) |
| `mc_hk_title` | Macro hotkey | Macro library | Title of the hotkey capture window |
| `mc_hk_set` | Set hotkey... | Macro library > Hotkey | Menu item |
| `mc_hk_remove` | Remove hotkey | Macro library > Hotkey | Menu item |
| `mc_hk_saved` | Hotkey set | Status bar | Shown after setting a macro hotkey |
| `mc_hk_removed` | Hotkey removed | Status bar | Shown after removing a macro hotkey |
| `mc_hk_taken` | That hotkey is already used by | Message box | Followed by the name of the other hotkey / macro |
| `mc_hk_needmod` | Use an F-key, or add Ctrl / Alt / Shift | Message box | Plain letters can't be global hotkeys |
| `mc_ov_default` | General setting (default) | Macro library > Settings | Use the global Speed / Loops value |
| `sound_cue` | Beep on start / stop | Settings menu | Checkbox: beep when playback or autoclick starts / stops |
| `ac_jitter` | Random click timing (±20%) | Autoclick settings menu | Checkbox: vary the interval a little |
| `mc_nomatch` | No matching macros | Macro library | Shown when the search finds nothing |
| `systray` | System tray | Settings menu | Checkbox: show the tray icon and hide to tray; off = taskbar only |

---

## Playback and Editor

| Key | English (`en`) |
| :--- | :--- |
| `pause_resume` | Pause / resume |
| `paused` | Paused |
| `countdown` | Countdown |
| `countdown_prompt` | Seconds before start (0-60; 0 = off): |
| `edit_mode` | Edit mode |
| `play_selected` | Play selected |
| `universal_delay` | Universal delay |
| `key_down` | Key press |
| `key_up` | Key release |
| `mouse_button` | Mouse button |
| `mouse_down` | Mouse press |
| `mouse_up` | Mouse release |
| `coordinates` | Coordinates |
| `wheel_amount` | Wheel amount |
| `mouse_button_prompt` | Button: 0 left, 1 right, 2 middle, 3 X1, 4 X2 |
| `x_prompt` | X coordinate: |
| `y_prompt` | Y coordinate: |
| `wheel_prompt` | Wheel amount (-32768 to 32767): |
| `universal_prompt` | Replace existing delays (ms): |
| `editor_hint` | Ctrl+click / Shift+click / drag to select |
| `delay_only` | Select at least one action to play. Delay-only selections have no actions. |
| `range_error` | Enter a whole number within the allowed range. |
| `preview_exclusions` | Exclusions... |
| `preview_exclusions_title` | Play selected exclusions |
| `preview_exclusions_hint` | Checked: this input will not play. |
| `preview_modifiers_hint` | Generic Ctrl / Shift / Alt / Win blocks both sides. |
| `filter_entries` | Search entries... |
| `select_all` | Select all |
| `clear_all` | Clear all |
| `block_key` | Block specific key... |
| `all_keyboard_keys` | All keyboard keys |
| `keyboard` | Keyboard |
| `mouse_left` | Left mouse button |
| `mouse_right` | Right mouse button |
| `mouse_middle` | Middle mouse button |
| `mouse_x1` | Side button X1 |
| `mouse_x2` | Side button X2 |
| `wheel_left` | Wheel left |
| `wheel_right` | Wheel right |
| `macro_visibility` | Visibility... |
| `macro_visibility_title` | Visibility for this macro |
| `macro_visibility_hint` | Tick = shown in this macro editor. |
| `macro_visibility_default` | Following general defaults |
| `macro_visibility_custom` | Custom visibility for this macro |
| `reset_default` | Reset to default |
| `editor_settings_savefail` | Could not save the editor settings. |
| `matching_entries` | matching entries |
| `preview_unchecked` | Unchecked: this input will play. |
| `excluded` | Excluded |
| `press_input` | Press a key or click a mouse button |
| `input_capture_fail` | Could not capture mouse input. Please try again. |

---

## Hold Mode and Window Targets

| Key | English |
| --- | --- |
| `ac_hold` | Toggle Mode |
| `ac_position_jitter` | Random click position |
| `ac_radius` | Position radius (pixels) |
| `play_target` | Run macro when window is focused |
| `ac_hover_target` | Click when mouse is over window |
| `target_pick` | Select window... |
| `target_clear` | Clear all / disable |
| `target_hint` | Click a window; Esc cancels |
| `waiting` | Waiting for |
| `release_stop` | Release to stop |

## Image Recognition

| Key | English |
| --- | --- |
| `image_recognition` | Image recognition |
| `add_image` | Add image |
| `use_image` | Use this |
| `retake` | Retake |
| `sensitivity` | Sensitivity |
| `strict` | Strict |
| `normal` | Normal |
| `loose` | Loose |
| `image_custom` | Custom |
| `image_timeout` | Timeout (ms) |
| `image_test` | Test |
| `image_found` | Found at %.1f%% |
| `image_not_found` | Not found |


## Image Actions and Watchers

| Key | English |
| --- | --- |
| `image_macro` | Image recognition macro... |
| `image_source_title` | Choose an image |
| `image_source_hint` | Take a small screenshot or choose an image file. |
| `image_screenshot` | Take screenshot |
| `image_file` | Choose file... |
| `image_capture_hint` | AutoOnty will minimize and open Windows snipping. Drag around the button or icon; Esc cancels. |
| `image_page` | Image |
| `image_actions` | When found |
| `image_waiting` | Waiting & repeat |
| `image_do` | What should happen? |
| `image_click` | Click once |
| `image_autoclick` | Auto click |
| `image_run` | Run a saved macro |
| `image_wait_only` | Wait only |
| `image_target` | Where to click |
| `image_center` | Image center |
| `image_cursor` | Current cursor position |
| `image_fixed` | Fixed position |
| `image_pick` | Pick position... |
| `image_interval` | Click interval (ms) |
| `image_click_count` | Clicks (0 = until image disappears) |
| `image_auto_duration` | Auto click time (ms, 0 = no limit) |
| `image_choose_macro` | Choose macro... |
| `image_no_macro` | Choose the macro to run once when the image is found. |
| `image_repeats` | Look for the image |
| `image_once` | Once, then finish this step |
| `image_reappear` | Again after it disappears |
| `image_repeat` | Keep acting while visible |
| `image_locations` | At each location once |
| `image_max_matches` | Matches (0 = no limit) |
| `image_duration` | Watch time (ms, 0 = no limit) |
| `image_cooldown` | Time between actions (ms) |
| `image_found_outcome` | After a match |
| `image_timeout_outcome` | If waiting times out |
| `image_continue` | Continue macro |
| `image_pause` | Pause until I resume |
| `image_stop` | Stop macro |
| `image_timeout_hint` | 0 = no timeout. 9999999 is valid. Waiting resets after a match. |
| `image_size_option` | Also try different image sizes |
| `image_hide` | Minimize AutoOnty while searching |
| `image_start` | Start watching |
| `image_save_only` | Save macro... |
| `image_number_error` | Enter whole numbers from 0 to 4294967295. Click intervals must be at least 1 ms. |
| `image_import_error` | Could not open that image. Choose a small BMP, PNG or JPG image. |
| `image_macro_error` | Could not load that macro, or it contains too many nested image actions. |
| `image_point_error` | Click a mouse position to choose where the action should click. |
| `image_test_low` | Not found. Best match:  |
| `image_test_error` | Could not capture the desktop. Keep the target visible and try again. |
| `image_test_hint` | Keep the target visible. Test hides AutoOnty and does not click. |
| `image_searching` | Searching for image |
| `image_clear` | Waiting for image to disappear |
| `image_active_clicks` | Image found: auto clicking |
| `image_active_run` | Image found: running macro |
| `image_timeout_status` | Image wait timed out |
| `image_capture_status` | Could not capture desktop; retrying |
| `image_paused_found` | Paused after image match |
| `image_paused_timeout` | Paused after image timeout |
| `image_matches` | matches |
| `image_best` | best |
| `image_unlimited` | no time limit |
| `image_backend` | Matcher |
| `image_native` | Portable |
| `image_low_detail` | Tip: include a distinctive icon or text, not just a plain background. |
| `image_running_macro` | Run macro:  |
| `image_settings` | Image settings... |
| `image_action_hint` | Choose an action, then decide where and how often it runs. |
| `image_repeat_hint` | Repeating stays on this step. Use match/watch-time limits, or Once to continue after one match. |
| `image_click_error` | Image found, but that position could not be clicked. |
| `image_match_error` | Could not process this image. Try a smaller screenshot. |

## Macro and Autoclick Settings

| Key | English |
| --- | --- |
| `macro_settings` | Macro settings |
| `ac_images` | Image recognition... |
| `ac_images_enable` | Use images when autoclicking |
| `ac_images_hint` | Separate speed and limits per image. 0 = unlimited; reset after disappearance. |
| `ac_images_empty` | Add and save an image in Image recognition... first. |
| `ac_images_limit` | The autoclick image list can contain up to 64 images. |
| `ac_images_load_error` | Could not load or migrate saved autoclick images. Check access to Documents\AutoOnty\ImageDetections and keep the old image files until migration succeeds. |
| `ac_images_select` | Choose images for autoclick |
| `ac_images_select_hint` | Click or press Space to select one or more images, then OK. |
| `ac_images_select_empty` | Select at least one image for autoclick. |
| `ac_image_count` | Click a specific number of times |
| `ac_image_continuous` | Keep clicking while visible |
| `ac_image_count_error` | Enter a click count from 1 to 4294967295. |

---

## How to Test Your Translation

1. Save your translated words into a UTF-8 `.json` file (e.g. `es.json` or `de.json`).
2. Open AutoOnty and click **Menu -> Language -> Import translation JSON...**.
3. Choose your file to test it immediately.
4. Share it on our [GitHub Issues](https://github.com/Myzoxxfr/AutoOnty/issues/new?template=translations.yml)!
