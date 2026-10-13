#include "lang/lang.h"
#include "core/config.h"
#include "core/json.h"

struct LEntry {
  const char *key;
  const wchar_t *en;
  const wchar_t *ru;
};

static std::wstring g_tab[L_COUNT];

static const LEntry kTable[L_COUNT] = {
    {"app", L"AutoOnty", L"AutoOnty"},
    {"record", L"Record", L"Запись"},
    {"stop", L"Stop", L"Остановить"},
    {"play", L"Play", L"Воспроизвести"},
    {"open", L"Open...", L"Открыть..."},
    {"save", L"Save...", L"Сохранить..."},
    {"speed", L"Speed", L"Скорость"},
    {"speed05", L"0.5x", L"0.5x"},
    {"speed1", L"1x (Normal)", L"1x (Обычная)"},
    {"speed2", L"2x (Fast)", L"2x (Быстрая)"},
    {"speed5", L"5x", L"5x"},
    {"speed10", L"10x", L"10x"},
    {"speed_turbo", L"100x Turbo", L"100x Турбо"},
    {"speed_max", L"Max Speed (Instant)", L"Максимальная (Без задержек)"},
    {"speed_custom", L"Custom...", L"Своя..."},
    {"loops", L"Playback loops", L"Повторы"},
    {"loops1", L"1", L"1"},
    {"loops2", L"2", L"2"},
    {"loops3", L"3", L"3"},
    {"loops5", L"5", L"5"},
    {"loops10", L"10", L"10"},
    {"loops100", L"100", L"100"},
    {"continuous", L"Continuous (\u221E)", L"Бесконечно (\u221E)"},
    {"loops_custom2", L"Custom...", L"Своё..."},
    {"theme", L"Theme", L"Тема"},
    {"interface_theme", L"Interface theme", L"Тема интерфейса"},
    {"theme_dark", L"Dark", L"Тёмная"},
    {"theme_light", L"Light", L"Светлая"},
    {"theme_acrylic", L"Acrylic", L"Acrylic"},
    {"language", L"Language", L"Язык"},
    {"lang_en", L"English", L"English"},
    {"lang_ru", L"Русский", L"Русский"},
    {"lang_import", L"Import translation JSON...", L"Импорт перевода JSON..."},
    {"lang_translate", L"Translate on GitHub...", L"Перевести на GitHub..."},
    {"hotkeys", L"Hotkeys", L"Горячие клавиши"},
    {"hk_record", L"Recording", L"Запись"},
    {"hk_play", L"Playback", L"Воспроизведение"},
    {"panic_keys", L"Panic stop keys", L"Экстренная остановка"},
    {"panic_mouse", L"Stop on mouse move", L"Остановка при движении мыши"},
    {"statusbar", L"Status bar", L"Статус-бар"},
    {"sb_enable", L"Show status bar", L"Показывать статус-бар"},
    {"sb_top", L"Top of screen", L"Вверху экрана"},
    {"sb_bottom", L"Bottom of screen", L"Внизу экрана"},
    {"sb_follow_monitor", L"Follow cursor monitor",
     L"Следовать за монитором курсора"},
    {"assoc_onty", L"Associate .onty files", L"Ассоциировать файлы .onty"},
    {"assoc_success", L".onty files registered", L"Файлы .onty привязаны"},
    {"assoc_removed", L".onty association removed", L"Привязка .onty удалена"},
    {"topmost", L"Always on top", L"Поверх всех окон"},
    {"update", L"Check for updates...", L"Проверить обновления..."},
    {"update_available", L"Update available!", L"Доступно обновление!"},
    {"exit", L"Exit", L"Выход"},
    {"about", L"About AutoOnty", L"О программе AutoOnty"},
    {"status_rec_stop", L"Recording stopped", L"Запись остановлена"},
    {"status_play", L"Playing", L"Воспроизведение"},
    {"status_play_stop", L"Playback stopped", L"Воспроизведение остановлено"},
    {"status_saved", L"Macro saved", L"Макрос сохранён"},
    {"status_nothing", L"Nothing recorded", L"Нет записи"},
    {"status_invalid", L"Invalid file", L"Неверный файл"},
    {"status_conflict", L"Hotkey conflict", L"Конфликт горячих клавиш"},
    {"status_max", L"Event limit reached", L"Достигнут лимит событий"},
    {"status_uptodate", L"You have the latest version",
     L"У вас последняя версия"},
    {"status_updatefail", L"Update check failed",
     L"Не удалось проверить обновления"},
    {"status_translated", L"Translation imported", L"Перевод импортирован"},
    {"status_loaded", L"Macro loaded", L"Макрос загружен"},
    {"about_desc",
     L"An open-source successor to TinyTask for fast desktop and 3D "
     L"automation.",
     L"Открытый наследник TinyTask для быстрой автоматизации на рабочем столе "
     L"и в 3D."},
    {"github", L"GitHub", L"GitHub"},
    {"license", L"License", L"Лицензия"},
    {"prompt_loops", L"Number of playback loops (1-99999)",
     L"Число повторов проигрывания (1-99999)"},
    {"prompt_speed", L"Speed multiplier (0.01-1000)",
     L"Множитель скорости (0.01-1000)"},
    {"ok", L"OK", L"OK"},
    {"cancel", L"Cancel", L"Отмена"},
    {"save_fail", L"Unable to save file", L"Не удалось сохранить файл"},
    {"minimize", L"Minimize", L"Свернуть"},
    {"hide", L"Hide to tray", L"Скрыть в трей"},
    {"settings", L"Settings", L"Настройки"},
    {"resave", L"Save changes", L"Сохранить изменения"},
    {"saved", L"Saved", L"Сохранено"},
    {"hk_press", L"Press a new hotkey (Esc to cancel)",
     L"Нажмите новую комбинацию (Esc - отмена)"},
    {"yes", L"Yes", L"Да"},
    {"no", L"No", L"Нет"},
    {"new_macro", L"New macro", L"Новый макрос"},
    {"developed", L"Fork of OntyTask by Agzes",
     L"Форк OntyTask от Agzes"},
    {"lic_line", L"Licensed under GNU GPL v3.0", L"Лицензия: GNU GPL v3.0"},
    {"tray_icon", L"Tray icon", L"Иконка трея"},
    {"tray_auto", L"Follow theme", L"Как тема"},
    {"ui_icon", L"Interface icon", L"Иконка интерфейса"},
    {"stop_bind", L"Stop", L"Остановить"},
    {"autominimize", L"Auto-minimize on Rec/Play",
     L"Авто-сворачивание при записи/старте"},
    {"clipboard", L"Clipboard", L"Буфер обмена"},
    {"copy_clipboard", L"Copy macro to clipboard",
     L"Копировать макрос в буфер"},
    {"paste_clipboard", L"Paste macro from clipboard",
     L"Вставить макрос из буфера"},
    {"clipboard_loaded", L"Macro loaded from clipboard",
     L"Макрос загружен из буфера"},
    {"clipboard_copied", L"Macro copied to clipboard",
     L"Макрос скопирован в буфер"},
    {"clipboard_empty", L"Clipboard has no valid macro",
     L"В буфере нет валидного макроса"},
    {"create_shortcut", L"Create Desktop shortcut",
     L"Создать ярлык на рабочем столе"},
    {"shortcut_success", L"Desktop shortcut created",
     L"Ярлык создан на рабочем столе"},
    {"shortcut_fail", L"Failed to create shortcut",
     L"Не удалось создать ярлык"},
    {"autoclick", L"Autoclick", L"Автокликер"},
    {"ac_settings", L"Autoclick settings", L"Настройки автокликера"},
    {"ac_interval", L"Click interval", L"Интервал кликов"},
    {"ac_hours", L"Hours", L"Часы"},
    {"ac_mins", L"Minutes", L"Минуты"},
    {"ac_secs", L"Seconds", L"Секунды"},
    {"ac_ms", L"Milliseconds", L"Миллисекунды"},
    {"ac_button", L"Mouse button", L"Кнопка мыши"},
    {"ac_btn_left", L"Left", L"Левая"},
    {"ac_btn_right", L"Right", L"Правая"},
    {"ac_btn_middle", L"Middle", L"Средняя"},
    {"ac_type", L"Click type", L"Тип клика"},
    {"ac_single", L"Single", L"Одиночный"},
    {"ac_double", L"Double", L"Двойной"},
    {"ac_repeat", L"Click repeat", L"Повтор кликов"},
    {"ac_repeat_until", L"Repeat until stopped", L"Повторять до остановки"},
    {"ac_repeat_count", L"Number of clicks", L"Число кликов"},
    {"prompt_ac_repeat", L"Number of clicks (1-999999)", L"Число кликов (1-999999)"},
    {"ac_position", L"Click position", L"Позиция клика"},
    {"ac_follow", L"Follow mouse cursor", L"Следовать за курсором"},
    {"ac_fixed", L"Fixed position", L"Фиксированная позиция"},
    {"ac_saved_pos", L"Saved position", L"Сохранённая позиция"},
    {"ac_not_set", L"not set", L"не задана"},
    {"hk_ac_pos", L"Set click position", L"Задать позицию клика"},
    {"status_ac_stop", L"Autoclick stopped", L"Автоклик остановлен"},
    {"status_ac_pos_saved", L"Click position saved", L"Позиция клика сохранена"},
    {"status_ac_nopos", L"Set a click position first", L"Сначала задайте позицию клика"},
    {"status_ac_busy", L"Stop the macro first", L"Сначала остановите макрос"},
    {"macros", L"Macros",
     L"Макросы"},
    {"cur_macro", L"Macro:",
     L"Макрос:"},
    {"mc_none", L"none",
     L"нет"},
    {"mc_load", L"Load",
     L"Загрузить"},
    {"mc_edit", L"Edit",
     L"Изменить"},
    {"mc_rename", L"Rename",
     L"Переименовать"},
    {"mc_delete", L"Delete",
     L"Удалить"},
    {"mc_save", L"Save current...",
     L"Сохранить..."},
    {"mc_browse", L"Browse...",
     L"Обзор..."},
    {"mc_folder", L"Open folder",
     L"Открыть папку"},
    {"mc_empty", L"No saved macros yet",
     L"Сохранённых макросов пока нет"},
    {"mc_name_prompt", L"Macro name",
     L"Название макроса"},
    {"mc_exists", L"A macro with this name already exists",
     L"Макрос с таким названием уже существует"},
    {"mc_replace_q", L"A macro with this name already exists. Replace it?",
     L"Макрос с таким названием уже существует. Заменить?"},
    {"mc_delete_q", L"Delete this macro?",
     L"Удалить этот макрос?"},
    {"mc_discard_q", L"Discard the unsaved macro changes?",
     L"Отбросить несохранённые изменения макроса?"},
    {"mc_deleted", L"Macro deleted",
     L"Макрос удалён"},
    {"mc_fail", L"Operation failed",
     L"Не удалось выполнить"},
    {"ed_title", L"Macro editor",
     L"Редактор макроса"},
    {"ed_add", L"Add key...",
     L"Добавить клавишу..."},
    {"ed_change", L"Change key...",
     L"Сменить клавишу..."},
    {"ed_delay", L"Edit delay...",
     L"Задержка..."},
    {"ed_up", L"Move up",
     L"Вверх"},
    {"ed_down", L"Move down",
     L"Вниз"},
    {"ed_press_key", L"Press the key you want",
     L"Нажмите нужную клавишу"},
    {"ed_delay_prompt", L"Wait before this action (ms)",
     L"Ожидание перед действием (мс)"},
    {"ed_kd", L"Key press",
     L"Нажатие"},
    {"ed_ku", L"Key release",
     L"Отпускание"},
    {"ed_md", L"Mouse press",
     L"Мышь: нажатие"},
    {"ed_mu", L"Mouse release",
     L"Мышь: отпускание"},
    {"ed_mv", L"Mouse move",
     L"Движение мыши"},
    {"ed_wh", L"Wheel",
     L"Колесо"},
    {"ed_hint", L"Delay = pause before the next action  |  Ins / Del / Ctrl+A",
     L"Задержка = пауза перед следующим действием  |  Ins / Del / Ctrl+A"},
    {"ed_discard_q", L"Discard your edits?",
     L"Отбросить изменения?"},
    {"rec_inputs", L"Record inputs",
     L"Записывать ввод"},
    {"rec_move", L"Mouse movement",
     L"Движение мыши"},
    {"rec_left", L"Left click",
     L"Левый клик"},
    {"rec_right", L"Right click",
     L"Правый клик"},
    {"rec_middle", L"Middle click",
     L"Средний клик"},
    {"rec_wheel_up", L"Wheel up",
     L"Колесо вверх"},
    {"rec_wheel_down", L"Wheel down",
     L"Колесо вниз"},
    {"ed_add_delay", L"Add delay...",
     L"Добавить задержку..."},
    {"ed_delay_row", L"Delay",
     L"Задержка"},
    {"ed_show", L"Show in macro editor",
     L"Показывать в редакторе"},
    {"ed_show_keys", L"Keyboard keys",
     L"Клавиши клавиатуры"}};

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
    g_tab[i] = (g_cfg.lang == 2) ? kTable[i].ru : kTable[i].en;
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
