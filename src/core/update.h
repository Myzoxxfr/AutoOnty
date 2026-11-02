#pragma once
#include <limits>
#include <string_view>

// The published version file is plain ASCII. Accept UTF-8 BOMs too, so an
// editor encoding change cannot break update checks. Reject partial/HTML data.
inline bool Update_ParseVersion(std::string_view data, int &out) {
  if (data.starts_with("\xEF\xBB\xBF")) data.remove_prefix(3);
  auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!data.empty() && space(data.front())) data.remove_prefix(1);
  while (!data.empty() && space(data.back())) data.remove_suffix(1);
  if (data.empty()) return false;
  int value = 0;
  for (char c : data) {
    if (c < '0' || c > '9') return false;
    int digit = c - '0';
    if (value > ((std::numeric_limits<int>::max)() - digit) / 10) return false;
    value = value * 10 + digit;
  }
  if (value <= 0) return false;
  out = value;
  return true;
}

enum { UR_NONE = 0, UR_CHECKING, UR_LATEST, UR_FOUND, UR_FAIL };

void Update_AutoCheck();
void Update_ManualCheck();
bool Update_Found();
int Update_State();

extern const wchar_t *URL_RELEASES;
extern const wchar_t *URL_GITHUB;
extern const wchar_t *URL_ISSUE;
extern const wchar_t *URL_LICENSE;
extern const wchar_t *APP_VERSION;
