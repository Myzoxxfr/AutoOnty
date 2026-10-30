#pragma once
#include <cstdlib>
#include <cmath>
#include <cerrno>
#include <locale.h>
#include <cwchar>
#include <string>
#include <windows.h>

namespace json {

struct Js {
  const wchar_t *p;
  const wchar_t *e;
};

inline void Ws(Js &j) {
  while (j.p < j.e &&
         (*j.p == L' ' || *j.p == L'\t' || *j.p == L'\r' || *j.p == L'\n'))
    ++j.p;
}

inline bool Eat(Js &j, wchar_t c) {
  Ws(j);
  if (j.p < j.e && *j.p == c) {
    ++j.p;
    return true;
  }
  return false;
}

inline bool Num(Js &j, double &out) {
  Ws(j);
  if (j.p >= j.e)
    return false;
  const wchar_t *p = j.p;
  if (*p == L'-' && ++p == j.e) return false;
  auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
  if (*p == L'0') ++p;
  else {
    if (*p < L'1' || *p > L'9') return false;
    do { ++p; } while (p < j.e && digit(*p));
  }
  if (p < j.e && *p == L'.') {
    ++p;
    if (p == j.e || !digit(*p)) return false;
    do { ++p; } while (p < j.e && digit(*p));
  }
  if (p < j.e && (*p == L'e' || *p == L'E')) {
    ++p;
    if (p < j.e && (*p == L'+' || *p == L'-')) ++p;
    if (p == j.e || !digit(*p)) return false;
    do { ++p; } while (p < j.e && digit(*p));
  }
  if (p < j.e && *p != L',' && *p != L'}' && *p != L']' &&
      *p != L' ' && *p != L'\t' && *p != L'\r' && *p != L'\n') return false;
  std::wstring token(j.p, p);
  wchar_t *end = nullptr;
  static _locale_t locale = _create_locale(LC_NUMERIC, "C");
  errno = 0;
  double v = _wcstod_l(token.c_str(), &end, locale);
  if (errno == ERANGE || !std::isfinite(v) || end != token.c_str() + token.size())
    return false;
  out = v;
  j.p = p;
  return true;
}

inline bool Integer(Js &j, double &out, double lo, double hi) {
  return Num(j, out) && out >= lo && out <= hi && std::trunc(out) == out;
}

inline bool Bool(Js &j, bool &out) {
  Ws(j);
  if (j.e - j.p >= 4 && wcsncmp(j.p, L"true", 4) == 0) {
    out = true;
    j.p += 4;
    return true;
  }
  if (j.e - j.p >= 5 && wcsncmp(j.p, L"false", 5) == 0) {
    out = false;
    j.p += 5;
    return true;
  }
  return false;
}

inline bool Hex4(Js &j, wchar_t &out) {
  if (j.e - j.p < 4) return false;
  unsigned value = 0;
  for (int i = 0; i < 4; ++i) {
    wchar_t c = j.p[i];
    unsigned digit;
    if (c >= L'0' && c <= L'9') digit = c - L'0';
    else if (c >= L'a' && c <= L'f') digit = c - L'a' + 10;
    else if (c >= L'A' && c <= L'F') digit = c - L'A' + 10;
    else return false;
    value = value * 16 + digit;
  }
  j.p += 4;
  out = (wchar_t)value;
  return true;
}

inline bool Str(Js &j, std::wstring &out) {
  Ws(j);
  if (j.p >= j.e || *j.p != L'"')
    return false;
  ++j.p;
  out.clear();
  while (j.p < j.e) {
    wchar_t c = *j.p++;
    if (c == L'"')
      return true;
    if (c != L'\\') {
      if (c < 0x20) return false;
      out += c;
      continue;
    }
    if (j.p >= j.e)
      return false;
    wchar_t e = *j.p++;
    switch (e) {
    case L'"':
      out += L'"';
      break;
    case L'\\':
      out += L'\\';
      break;
    case L'/':
      out += L'/';
      break;
    case L'b':
      out += L'\b';
      break;
    case L'f':
      out += L'\f';
      break;
    case L'n':
      out += L'\n';
      break;
    case L'r':
      out += L'\r';
      break;
    case L't':
      out += L'\t';
      break;
    case L'u': {
      wchar_t v;
      if (!Hex4(j, v)) return false;
      out += v; // Preserve UTF-16 code units, including surrogate pairs.
      break;
    }
    default:
      return false;
    }
  }
  return false;
}

// Validate mixed nested values, bounding recursion for untrusted files.
inline bool SkipVal(Js &j, unsigned depth = 0) {
  Ws(j);
  if (j.p == j.e || depth >= 128) return false;
  if (*j.p == L'"') {
    std::wstring str;
    return Str(j, str);
  }
  if (*j.p == L'{' || *j.p == L'[') {
    bool object = *j.p++ == L'{';
    wchar_t close = object ? L'}' : L']';
    if (Eat(j, close)) return true;
    for (;;) {
      if (object) {
        std::wstring key;
        if (!Str(j, key) || !Eat(j, L':')) return false;
      }
      if (!SkipVal(j, depth + 1)) return false;
      if (Eat(j, close)) return true;
      if (!Eat(j, L',')) return false;
    }
  }
  bool boolean;
  if (Bool(j, boolean)) return true;
  if (j.e - j.p >= 4 && wcsncmp(j.p, L"null", 4) == 0) {
    j.p += 4;
    return true;
  }
  double number;
  return Num(j, number);
}

inline std::string ToUtf8(const std::wstring &w) {
  if (w.empty())
    return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                              nullptr, nullptr);
  std::string s((size_t)n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr,
                      nullptr);
  return s;
}

inline std::wstring ToWide(const std::string &u) {
  if (u.empty())
    return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), nullptr, 0);
  std::wstring w((size_t)n, 0);
  MultiByteToWideChar(CP_UTF8, 0, u.c_str(), (int)u.size(), &w[0], n);
  return w;
}

inline bool ReadTextFile(const wchar_t *path, std::string &out) {
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  DWORD sz = GetFileSize(h, nullptr);
  if (sz == INVALID_FILE_SIZE || sz > 32 * 1024 * 1024) {
    CloseHandle(h);
    return false;
  }
  out.resize(sz);
  DWORD rd = 0;
  BOOL ok = sz == 0 || ReadFile(h, &out[0], sz, &rd, nullptr);
  CloseHandle(h);
  return ok && rd == sz;
}

inline bool WriteTextFile(const wchar_t *path, const std::string &data) {
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  DWORD w = 0;
  BOOL ok = data.empty() ||
            WriteFile(h, data.data(), (DWORD)data.size(), &w, nullptr);
  CloseHandle(h);
  return ok && w == data.size();
}

} // namespace json
