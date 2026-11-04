#include "lang/lang.h"
#include "core/config.h"
#include "core/json.h"

struct LEntry {
  const char *key;
  const wchar_t *en;
};

static std::wstring g_tab[L_COUNT];

static const LEntry kTable[L_COUNT] = {
    {"app", L"AutoOnty"},
    {"record", L"Record"},
    {"stop", L"Stop"},
    {"play", L"Play"},
    {"open", L"Open..."},
    {"save", L"Save..."},
    {"speed", L"Speed"},
    {"speed05", L"0.5x"},
    {"speed1", L"1x (Normal)"},
    {"speed2", L"2x (Fast)"},
    {"speed5", L"5x"},
    {"speed10", L"10x"},
    {"speed_turbo", L"100x Turbo"},
    {"speed_max", L"Max Speed (Instant)"},
    {"speed_custom", L"Custom..."},
    {"loops", L"Playback loops"},
    {"loops1", L"1"},
    {"loops2", L"2"},
    {"loops3", L"3"},
    {"loops5", L"5"},
    {"loops10", L"10"},
    {"loops100", L"100"},
    {"continuous", L"Continuous (\u221E)"},
    {"loops_custom2", L"Custom..."},
    {"theme", L"Theme"},
    {"interface_theme", L"Interface theme"},
    {"theme_dark", L"Dark"},
    {"theme_light", L"Light"},
    {"theme_acrylic", L"Acrylic"},
    {"language", L"Language"},
    {"lang_en", L"English"},
    {"lang_import", L"Import translation JSON..."},
    {"lang_translate", L"Translate on GitHub..."},
    {"hotkeys", L"Hotkeys"},
    {"hk_record", L"Recording"},
    {"hk_play", L"Playback"},
    {"panic_keys", L"Panic stop keys"},
    {"panic_mouse", L"Stop on mouse move"},
    {"statusbar", L"Status bar"},
    {"sb_enable", L"Show status bar"},
    {"sb_top", L"Top of screen"},
    {"sb_bottom", L"Bottom of screen"},
    {"sb_follow_monitor", L"Follow cursor monitor"},
    {"assoc_onty", L"Associate .onty files"},
    {"assoc_success", L".onty files registered"},
    {"assoc_removed", L".onty association removed"},
    {"topmost", L"Always on top"},
    {"update", L"Check for updates..."},
    {"update_available", L"Update available!"},
    {"exit", L"Exit"},
    {"about", L"About AutoOnty"},
    {"status_rec_stop", L"Recording stopped"},
    {"status_play", L"Playing"},
    {"status_play_stop", L"Playback stopped"},
    {"status_saved", L"Macro saved"},
    {"status_nothing", L"Nothing recorded"},
    {"status_invalid", L"Invalid file"},
    {"status_conflict", L"Hotkey conflict"},
    {"status_max", L"Event limit reached"},
    {"status_uptodate", L"You have the latest version"},
    {"status_updatefail", L"Update check failed"},
    {"status_translated", L"Translation imported"},
    {"status_loaded", L"Macro loaded"},
    {"about_desc",
     L"An open-source successor to TinyTask for fast desktop and 3D "
     L"automation."},
    {"github", L"GitHub"},
    {"license", L"License"},
    {"prompt_loops", L"Number of playback loops (1-99999)"},
    {"prompt_speed", L"Speed multiplier (0.01-1000)"},
    {"ok", L"OK"},
    {"cancel", L"Cancel"},
    {"save_fail", L"Unable to save file"},
    {"minimize", L"Minimize"},
    {"hide", L"Hide to tray"},
    {"settings", L"Settings"},
    {"resave", L"Save changes"},
    {"saved", L"Saved"},
    {"hk_press", L"Press a new hotkey (Esc to cancel)"},
    {"yes", L"Yes"},
    {"no", L"No"},
    {"new_macro", L"New macro"},
    {"developed", L"Fork of OntyTask by Agzes"},
    {"lic_line", L"Licensed under GNU GPL v3.0"},
    {"tray_icon", L"Tray icon"},
    {"tray_auto", L"Follow theme"},
    {"ui_icon", L"Interface icon"},
    {"stop_bind", L"Stop"},
    {"autominimize", L"Auto-minimize on Rec/Play"},
    {"clipboard", L"Clipboard"},
    {"copy_clipboard", L"Copy macro to clipboard"},
    {"paste_clipboard", L"Paste macro from clipboard"},
    {"clipboard_loaded", L"Macro loaded from clipboard"},
    {"clipboard_copied", L"Macro copied to clipboard"},
    {"clipboard_empty", L"Clipboard has no valid macro"},
    {"create_shortcut", L"Create Desktop shortcut"},
    {"shortcut_success", L"Desktop shortcut created"},
    {"shortcut_fail", L"Failed to create shortcut"},
    {"autoclick", L"Autoclick"},
    {"ac_settings", L"Autoclick settings"},
    {"ac_interval", L"Click interval"},
    {"ac_hours", L"Hours"},
    {"ac_mins", L"Minutes"},
    {"ac_secs", L"Seconds"},
    {"ac_ms", L"Milliseconds"},
    {"ac_button", L"Mouse button"},
    {"ac_btn_left", L"Left"},
    {"ac_btn_right", L"Right"},
    {"ac_btn_middle", L"Middle"},
    {"ac_type", L"Click type"},
    {"ac_single", L"Single"},
    {"ac_double", L"Double"},
    {"ac_repeat", L"Click repeat"},
    {"ac_repeat_until", L"Repeat until stopped"},
    {"ac_repeat_count", L"Number of clicks"},
    {"prompt_ac_repeat", L"Number of clicks (1-999999)"},
    {"ac_position", L"Click position"},
    {"ac_follow", L"Follow mouse cursor"},
    {"ac_fixed", L"Fixed position"},
    {"ac_saved_pos", L"Saved position"},
    {"ac_not_set", L"not set"},
    {"hk_ac_pos", L"Set click position"},
    {"status_ac_stop", L"Autoclick stopped"},
    {"status_ac_pos_saved", L"Click position saved"},
    {"status_ac_nopos", L"Set a click position first"},
    {"status_ac_busy", L"Stop the macro first"},
    {"macros", L"Macros"},
    {"cur_macro", L"Macro:"},
    {"mc_none", L"none"},
    {"mc_load", L"Load"},
    {"mc_edit", L"Edit"},
    {"mc_rename", L"Rename"},
    {"mc_delete", L"Delete"},
    {"mc_save", L"Save current..."},
    {"mc_browse", L"Browse..."},
    {"mc_folder", L"Open folder"},
    {"mc_empty", L"No saved macros yet"},
    {"mc_name_prompt", L"Macro name"},
    {"mc_exists", L"A macro with this name already exists"},
    {"mc_replace_q", L"A macro with this name already exists. Replace it?"},
    {"mc_delete_q", L"Delete this macro?"},
    {"mc_discard_q", L"Discard the unsaved macro changes?"},
    {"mc_deleted", L"Macro deleted"},
    {"mc_fail", L"Operation failed"},
    {"ed_title", L"Macro editor"},
    {"ed_add", L"Add key..."},
    {"ed_change", L"Change key..."},
    {"ed_delay", L"Edit delay..."},
    {"ed_up", L"Move up"},
    {"ed_down", L"Move down"},
    {"ed_press_key", L"Press the key you want"},
    {"ed_delay_prompt", L"Wait before this action (ms)"},
    {"ed_kd", L"Key press"},
    {"ed_ku", L"Key release"},
    {"ed_md", L"Mouse press"},
    {"ed_mu", L"Mouse release"},
    {"ed_mv", L"Mouse move"},
    {"ed_wh", L"Wheel"},
    {"ed_hint", L"Delay = pause before the next action  |  Ins / Del / Ctrl+A"},
    {"ed_discard_q", L"Discard your edits?"},
    {"rec_inputs", L"Record inputs"},
    {"rec_move", L"Mouse movement"},
    {"rec_left", L"Left click"},
    {"rec_right", L"Right click"},
    {"rec_middle", L"Middle click"},
    {"rec_wheel_up", L"Wheel up"},
    {"rec_wheel_down", L"Wheel down"},
    {"ed_add_delay", L"Add delay..."},
    {"ed_delay_row", L"Delay"},
    {"ed_show", L"Show in macro editor"},
    {"ed_show_keys", L"Keyboard keys"},
    {"ed_undo", L"Undo"},
    {"ed_redo", L"Redo"},
    {"mc_hotkey", L"Hotkey..."},
    {"mc_settings", L"Settings..."},
    {"mc_search", L"Search macros..."},
    {"mc_hk_title", L"Macro hotkey"},
    {"mc_hk_set", L"Set hotkey..."},
    {"mc_hk_remove", L"Remove hotkey"},
    {"mc_hk_saved", L"Hotkey set"},
    {"mc_hk_removed", L"Hotkey removed"},
    {"mc_hk_taken", L"That hotkey is already used by"},
    {"mc_hk_needmod", L"Use an F-key, or add Ctrl / Alt / Shift"},
    {"mc_ov_default", L"General setting (default)"},
    {"sound_cue", L"Beep on start / stop"},
    {"ac_jitter", L"Random click timing (±20%)"},
    {"mc_nomatch", L"No matching macros"},
    {"systray", L"System tray"},
    {"pause_resume", L"Pause / resume"},
    {"paused", L"Paused"},
    {"countdown", L"Countdown"},
    {"countdown_prompt", L"Seconds before start (0-60; 0 = off):"},
    {"edit_mode", L"Edit mode"},
    {"play_selected", L"Play selected"},
    {"universal_delay", L"Universal delay"},
    {"key_down", L"Key press"},
    {"key_up", L"Key release"},
    {"mouse_button", L"Mouse button"},
    {"mouse_down", L"Mouse press"},
    {"mouse_up", L"Mouse release"},
    {"coordinates", L"Coordinates"},
    {"wheel_amount", L"Wheel amount"},
    {"mouse_button_prompt", L"Button: 0 left, 1 right, 2 middle, 3 X1, 4 X2"},
    {"x_prompt", L"X coordinate:"},
    {"y_prompt", L"Y coordinate:"},
    {"wheel_prompt", L"Wheel amount (-32768 to 32767):"},
    {"universal_prompt", L"Replace existing delays (ms):"},
    {"editor_hint", L"Ctrl+click / Shift+click / drag to select"},
    {"delay_only", L"Select at least one action to play. Delay-only selections "
                   L"have no actions."},
    {"range_error", L"Enter a whole number within the allowed range."},
    {"preview_exclusions", L"Exclusions..."},
    {"preview_exclusions_title", L"Play selected exclusions"},
    {"preview_exclusions_hint", L"Checked: this input will not play."},
    {"preview_modifiers_hint",
     L"Generic Ctrl / Shift / Alt / Win blocks both sides."},
    {"filter_entries", L"Search entries..."},
    {"select_all", L"Select all"},
    {"clear_all", L"Clear all"},
    {"block_key", L"Block specific key..."},
    {"all_keyboard_keys", L"All keyboard keys"},
    {"keyboard", L"Keyboard"},
    {"mouse_left", L"Left mouse button"},
    {"mouse_right", L"Right mouse button"},
    {"mouse_middle", L"Middle mouse button"},
    {"mouse_x1", L"Side button X1"},
    {"mouse_x2", L"Side button X2"},
    {"wheel_left", L"Wheel left"},
    {"wheel_right", L"Wheel right"},
    {"macro_visibility", L"Visibility..."},
    {"macro_visibility_title", L"Visibility for this macro"},
    {"macro_visibility_hint", L"Tick = shown in this macro editor."},
    {"macro_visibility_default", L"Following general defaults"},
    {"macro_visibility_custom", L"Custom visibility for this macro"},
    {"reset_default", L"Reset to default"},
    {"editor_settings_savefail", L"Could not save the editor settings."},
    {"matching_entries", L"matching entries"},
    {"preview_unchecked", L"Unchecked: this input will play."},
    {"excluded", L"Excluded"},
    {"press_input", L"Press a key or click a mouse button"},
    {"input_capture_fail", L"Could not capture mouse input. Please try again."},
    {"ac_hold", L"Toggle Mode"},
    {"ac_position_jitter", L"Random click position"},
    {"ac_radius", L"Position radius (pixels)"},
    {"play_target", L"Run macro when window is focused"},
    {"ac_hover_target", L"Click when mouse is over window"},
    {"target_pick", L"Select window..."},
    {"target_clear", L"Clear all / disable"},
    {"target_hint", L"Click a window; Esc cancels"},
    {"waiting", L"Waiting for"},
    {"release_stop", L"Release to stop"},
    {"image_recognition", L"Image recognition"},
    {"add_image", L"Add image"},
    {"use_image", L"Use this"},
    {"retake", L"Retake"},
    {"sensitivity", L"Sensitivity"},
    {"strict", L"Strict"},
    {"normal", L"Normal"},
    {"loose", L"Loose"},
    {"image_custom", L"Custom"},
    {"image_timeout", L"Timeout (ms)"},
    {"image_test", L"Test"},
    {"image_found", L"Found at %.1f%%"},
    {"image_not_found", L"Not found"},
    {"image_macro", L"Image recognition macro..."},
    {"image_source_title", L"Choose an image"},
    {"image_source_hint", L"Take a small screenshot or choose an image file."},
    {"image_screenshot", L"Take screenshot"},
    {"image_file", L"Choose file..."},
    {"image_capture_hint", L"AutoOnty will minimize and open Windows snipping. "
                           L"Drag around the button or icon; Esc cancels."},
    {"image_page", L"Image"},
    {"image_actions", L"When found"},
    {"image_waiting", L"Waiting & repeat"},
    {"image_do", L"What should happen?"},
    {"image_click", L"Click once"},
    {"image_autoclick", L"Auto click"},
    {"image_run", L"Run a saved macro"},
    {"image_wait_only", L"Wait only"},
    {"image_target", L"Where to click"},
    {"image_center", L"Image center"},
    {"image_cursor", L"Current cursor position"},
    {"image_fixed", L"Fixed position"},
    {"image_pick", L"Pick position..."},
    {"image_interval", L"Click interval (ms)"},
    {"image_click_count", L"Clicks (0 = until image disappears)"},
    {"image_auto_duration", L"Auto click time (ms, 0 = no limit)"},
    {"image_choose_macro", L"Choose macro..."},
    {"image_no_macro",
     L"Choose the macro to run once when the image is found."},
    {"image_repeats", L"Look for the image"},
    {"image_once", L"Once, then finish this step"},
    {"image_reappear", L"Again after it disappears"},
    {"image_repeat", L"Keep acting while visible"},
    {"image_locations", L"At each location once"},
    {"image_max_matches", L"Matches (0 = no limit)"},
    {"image_duration", L"Watch time (ms, 0 = no limit)"},
    {"image_cooldown", L"Time between actions (ms)"},
    {"image_found_outcome", L"After a match"},
    {"image_timeout_outcome", L"If waiting times out"},
    {"image_continue", L"Continue macro"},
    {"image_pause", L"Pause until I resume"},
    {"image_stop", L"Stop macro"},
    {"image_timeout_hint",
     L"0 = no timeout. 9999999 is valid. Waiting resets after a match."},
    {"image_size_option", L"Also try different image sizes"},
    {"image_hide", L"Minimize AutoOnty while searching"},
    {"image_start", L"Start watching"},
    {"image_save_only", L"Save macro..."},
    {"image_number_error", L"Enter whole numbers from 0 to 4294967295. Click "
                           L"intervals must be at least 1 ms."},
    {"image_import_error",
     L"Could not open that image. Choose a small BMP, PNG or JPG image."},
    {"image_macro_error", L"Could not load that macro, or it contains too many "
                          L"nested image actions."},
    {"image_point_error",
     L"Click a mouse position to choose where the action should click."},
    {"image_test_low", L"Not found. Best match: "},
    {"image_test_error",
     L"Could not capture the desktop. Keep the target visible and try again."},
    {"image_test_hint",
     L"Keep the target visible. Test hides AutoOnty and does not click."},
    {"image_searching", L"Searching for image"},
    {"image_clear", L"Waiting for image to disappear"},
    {"image_active_clicks", L"Image found: auto clicking"},
    {"image_active_run", L"Image found: running macro"},
    {"image_timeout_status", L"Image wait timed out"},
    {"image_capture_status", L"Could not capture desktop; retrying"},
    {"image_paused_found", L"Paused after image match"},
    {"image_paused_timeout", L"Paused after image timeout"},
    {"image_matches", L"matches"},
    {"image_best", L"best"},
    {"image_unlimited", L"no time limit"},
    {"image_backend", L"Matcher"},
    {"image_native", L"Portable"},
    {"image_low_detail",
     L"Tip: include a distinctive icon or text, not just a plain background."},
    {"image_running_macro", L"Run macro: "},
    {"image_settings", L"Image settings..."},
    {"image_action_hint",
     L"Choose an action, then decide where and how often it runs."},
    {"image_repeat_hint", L"Repeating stays on this step. Use match/watch-time "
                          L"limits, or Once to continue after one match."},
    {"image_click_error",
     L"Image found, but that position could not be clicked."},
    {"image_match_error",
     L"Could not process this image. Try a smaller screenshot."},
    {"macro_settings", L"Macro settings"}};

const wchar_t *L(int id) { return g_tab[id].c_str(); }

static std::wstring CustomLangPath() {
  wchar_t p[MAX_PATH];
  GetModuleFileNameW(nullptr, p, MAX_PATH);
  wchar_t *s = wcsrchr(p, L'\\');
  if (s)
    *(s + 1) = 0;
  lstrcatW(p, L"AutoOnty.lang.json");
  return p;
}

void LangInit() {
  // Start from English so strings missing from an older custom translation
  // file show up in English instead of blank.
  for (int i = 0; i < L_COUNT; i++)
    g_tab[i] = kTable[i].en;
  if (g_cfg.lang == 0) {
    std::wstring dummy;
    if (LangImport(CustomLangPath().c_str(), dummy))
      return;
    g_cfg.lang = 1;
  }
  for (int i = 0; i < L_COUNT; i++)
    g_tab[i] = kTable[i].en;
}

bool LangImport(const wchar_t *path, std::wstring &nameOut) {
  std::string bytes;
  if (!json::ReadTextFile(path, bytes))
    return false;
  std::wstring src = json::ToWide(bytes);
  json::Js j{src.c_str(), src.c_str() + src.size()};
  if (!json::Eat(j, L'{'))
    return false;
  bool any = false;
  nameOut.clear();
  for (;;) {
    json::Ws(j);
    if (json::Eat(j, L'}'))
      break;
    std::wstring key;
    if (!json::Str(j, key))
      return false;
    if (!json::Eat(j, L':'))
      return false;
    if (key == L"lang") {
      json::Str(j, nameOut);
    } else if (key == L"strings") {
      json::Ws(j);
      if (!json::Eat(j, L'{'))
        return false;
      for (;;) {
        json::Ws(j);
        if (json::Eat(j, L'}'))
          break;
        std::wstring sk, sv;
        if (!json::Str(j, sk) || !json::Eat(j, L':') || !json::Str(j, sv))
          return false;
        for (int i = 0; i < L_COUNT; i++) {
          if (sk == json::ToWide(kTable[i].key)) {
            g_tab[i] = sv;
            any = true;
            break;
          }
        }
        if (!json::Eat(j, L',')) {
          json::Ws(j);
          if (!json::Eat(j, L'}'))
            return false;
          break;
        }
      }
    } else {
      if (!json::SkipVal(j))
        return false;
    }
    if (!json::Eat(j, L',')) {
      json::Ws(j);
      if (!json::Eat(j, L'}'))
        return false;
      break;
    }
  }
  if (any) {
    std::wstring customPath = CustomLangPath();
    if (_wcsicmp(path, customPath.c_str()) != 0) {
      CopyFileW(path, customPath.c_str(), FALSE);
    }
  }
  return any;
}
