#pragma once
#include <atomic>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

enum { A_MD = 0, A_MU, A_KD, A_KU, A_MOVE, A_WH, A_WAIT };
enum { HK_CTRL = 1, HK_SHIFT = 2, HK_ALT = 4, HK_WIN = 8 };
enum { ST_IDLE = 0, ST_REC = 1, ST_PLAY = 2 };

struct MAc {
  int type;
  DWORD delay;
  LONG x, y;
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
bool Engine_Load(const wchar_t *path);
bool Engine_ToJson(std::string &outJson);
bool Engine_FromJson(const std::string &jsonStr);
