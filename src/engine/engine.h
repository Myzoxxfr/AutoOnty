#pragma once
#include "engine/image.h"
#include <atomic>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

enum { A_MD = 0, A_MU, A_KD, A_KU, A_MOVE, A_WH, A_WAIT, A_IMAGE };
enum { HK_CTRL = 1, HK_SHIFT = 2, HK_ALT = 4, HK_WIN = 8 };
enum { ST_IDLE = 0, ST_REC = 1, ST_PLAY = 2 };

enum ImageAction { IA_CLICK, IA_AUTOCLICK, IA_RUN_MACRO, IA_WAIT };
enum ImageClickTarget { IT_IMAGE, IT_CURSOR, IT_FIXED };
enum ImageRepeat { IR_ONCE, IR_REAPPEAR, IR_REPEAT, IR_LOCATIONS };
enum ImageOutcome { IO_CONTINUE, IO_PAUSE, IO_STOP };
struct ImageOptions {
  int action = IA_CLICK, target = IT_IMAGE, repeat = IR_ONCE;
  int onFound = IO_CONTINUE, onTimeout = IO_CONTINUE;
  DWORD interval = 100, cooldown = 300, duration = 0, autoDuration = 0;
  unsigned maxMatches = 0, clicks = 10;
  LONG targetX = 0, targetY = 0;
  bool differentSizes = false, hideApp = true, standalone = false;
};
enum ImageStage {
  IS_IDLE,
  IS_SEARCHING,
  IS_WAIT_CLEAR,
  IS_CLICKING,
  IS_RUNNING,
  IS_TIMED_OUT,
  IS_CAPTURE_FAILED,
  IS_PAUSE_FOUND,
  IS_PAUSE_TIMEOUT,
  IS_CLICK_FAILED,
  IS_MATCH_FAILED
};
extern std::atomic<int> g_imageStage, g_imageMatches, g_imageScore;
extern std::atomic<DWORD> g_imageRemaining;
std::wstring Engine_ImageStatus();
bool Engine_ImageHideApp();
struct MAc {
  std::shared_ptr<image::Pixels> image;
  std::wstring imageFile;
  ImageOptions imageOptions;
  std::shared_ptr<const std::vector<MAc>> imageMacro;
  std::wstring imageMacroName;
  int sensitivity = 85;
  DWORD imageTimeout = 3000;
  int type;
  DWORD delay;
  LONG x, y;
  bool hasPosition = false;
  BYTE btn;
  BYTE vk;
  DWORD flags;
  bool mDown;
  bool mUp;
  bool relative;
  std::vector<std::pair<WORD, WORD>> path;
  std::vector<WORD> pdelays;
  SHORT wheel;
  bool horizontal; // absent in old files: vertical wheel
  MAc()
      : type(0), delay(0), x(0), y(0), btn(0), vk(0), flags(0), mDown(false),
        mUp(false), relative(false), wheel(0), horizontal(false) {}
  bool HasPosition() const { return hasPosition || x != 0 || y != 0; }
};

extern std::vector<MAc> g_events;
extern std::atomic<int> g_state;
extern std::atomic<int> g_playLoop;
extern std::atomic<bool> g_playStopped;
extern std::atomic<bool> g_mouseMovedDuringPlay;
extern DWORD g_startTick;
extern std::atomic<bool> g_playPaused;
extern std::atomic<bool> g_playPreview;
extern std::atomic<int> g_playCountdown;
extern std::atomic<DWORD> g_playElapsed;
extern std::atomic<int> g_playTotal; // 0 means continuous

// Playback settings stored in the last loaded macro file. speed < -5 / loops < 0
// / continuous < 0 mean "the file didn't say".
struct FileSettings {
  int speed = -10;
  double speedCustom = 1.0;
  int loops = -1;
  int continuous = -1;
};
extern FileSettings g_fileSettings;
// Copies them into g_cfg (used for command-line runs only).
void Engine_ApplyFileSettings();

bool HotkeyDown(int mods, int vk);
bool HotkeyHeld(int mods, int vk);
bool HotkeyReleaseEnds(int mods, int vk, DWORD released);
// UI-thread view of the current run snapshot; replaced only after joining the worker.
const std::wstring &Engine_FocusTargets();
std::wstring FmtHk(int mods, int vk);

void Engine_InitRawInput();
void Engine_RawInput(LPARAM l);
void Engine_FlushMoves();

bool Engine_Record();
bool Engine_RecordStop();
bool Engine_Play();
// Plays an immutable editor selection once without changing the loaded macro.
bool Engine_PlaySelection(const std::vector<MAc> &events, int countdown = -1);
void Engine_TogglePause();
void Engine_PlayStop();
void Engine_Shutdown();
bool Engine_Save(const wchar_t *path);
// Library metadata may retain missing assets; ordinary macro reads/writes stay strict.
bool Engine_WriteFile(const wchar_t *path, const std::vector<MAc> &events,
                       const wchar_t *imageDirectory = nullptr, bool allowMissingImages = false);
bool Engine_Load(const wchar_t *path);
// Read a selected action macro without changing the macro currently being
// edited.
bool Engine_ReadFile(const wchar_t *path, std::vector<MAc> &events,
                      const wchar_t *imageDirectory = nullptr, bool allowMissingImages = false);
// Writes all (including nested) BMP assets to a destination directory.
bool Engine_SaveImages(const wchar_t *directory);
bool Engine_ToJson(std::string &outJson);
bool Engine_FromJson(const std::string &jsonStr,
                     const wchar_t *imageDirectory = nullptr);
