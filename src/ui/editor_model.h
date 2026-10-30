#pragma once
#include "core/editorprefs.h"
#include "engine/engine.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace macroedit {
struct Item {
  bool isDelay = false;
  DWORD ms = 0;
  MAc a;
  std::vector<MAc> pre; // Hidden events remain untouched by bulk edits.
};

enum Field {
  Key,
  KeyDown,
  KeyUp,
  Button,
  MouseDown,
  MouseUp,
  Coordinates,
  Wheel
};

inline bool Compatible(const Item &item, Field field) {
  if (item.isDelay)
    return false;
  bool key = item.a.type == A_KD || item.a.type == A_KU;
  bool mouse = item.a.type == A_MD || item.a.type == A_MU;
  switch (field) {
  case Key:
  case KeyDown:
  case KeyUp:
    return key;
  case Button:
  case MouseDown:
  case MouseUp:
    return mouse;
  case Coordinates:
    return mouse || item.a.type == A_MOVE || item.a.type == A_WH;
  case Wheel:
    return item.a.type == A_WH;
  }
  return false;
}

inline bool Apply(Item &item, Field field, LONG value, LONG second = 0,
                  bool ext = false) {
  if (!Compatible(item, field))
    return false;
  switch (field) {
  case Key:
    item.a.vk = (BYTE)value;
    item.a.flags = ext ? KEYEVENTF_EXTENDEDKEY : 0;
    break;
  case KeyDown:
    item.a.type = A_KD;
    break;
  case KeyUp:
    item.a.type = A_KU;
    break;
  case Button:
    item.a.btn = (BYTE)value;
    break;
  case MouseDown:
    item.a.type = A_MD;
    break;
  case MouseUp:
    item.a.type = A_MU;
    break;
  case Coordinates:
    item.a.x = value;
    item.a.y = second;
    break;
  case Wheel:
    item.a.wheel = (SHORT)value;
    break;
  }
  return true;
}

// Explicit Delay rows and existing delays directly before selected actions.
// Hidden event timing lives in pre/path and is deliberately not targeted.
inline std::vector<int> UniversalDelayRows(const std::vector<Item> &items,
                                           const std::vector<int> &selection) {
  std::vector<int> rows;
  size_t start = 0;
  for (size_t i = 0; i <= items.size(); ++i) {
    if (i < items.size() && items[i].isDelay)
      continue;
    bool actionSelected =
        i < items.size() &&
        std::binary_search(selection.begin(), selection.end(), (int)i);
    for (size_t row = start; row < i; ++row)
      if (selection.empty() || actionSelected ||
          std::binary_search(selection.begin(), selection.end(), (int)row))
        rows.push_back((int)row);
    start = i + 1;
  }
  return rows;
}

inline size_t UniversalDelay(std::vector<Item> &items,
                             const std::vector<int> &selection, DWORD ms) {
  size_t changed = 0;
  for (int row : UniversalDelayRows(items, selection)) {
    if (items[(size_t)row].ms != ms) {
      items[(size_t)row].ms = ms;
      ++changed;
    }
  }
  return changed;
}

inline uint64_t MovementTime(const MAc &a) {
  if (a.type != A_MOVE || a.path.empty())
    return 0;
  uint64_t ms = a.mUp ? 10 : 0;
  for (size_t i = 0; i < a.path.size(); ++i)
    ms += i < a.pdelays.size() ? a.pdelays[i] : 15;
  return ms;
}

inline bool Shown(const MAc &a, unsigned mask) {
  switch (a.type) {
  case A_KD:
  case A_KU:
    return (mask & EV_KEYS) != 0;
  case A_MD:
  case A_MU:
    return a.btn < 5 && (mask & (EV_LEFT << a.btn)) != 0;
  case A_MOVE:
    return (mask & EV_MOVES) != 0;
  case A_WH:
    if (a.wheel == 0)
      return true;
    if (a.horizontal)
      return (mask & (a.wheel < 0 ? EV_WHEELLEFT : EV_WHEELRIGHT)) != 0;
    return (mask & (a.wheel > 0 ? EV_WHEELUP : EV_WHEELDOWN)) != 0;
  default:
    return true;
  }
}

inline std::vector<Item> FromEvents(const std::vector<MAc> &events,
                                    std::vector<MAc> &tail,
                                    unsigned visibility) {
  std::vector<Item> items;
  std::vector<MAc> hidden;
  for (const auto &a : events) {
    if (!Shown(a, visibility)) {
      hidden.push_back(a);
      continue;
    }
    if (a.delay) {
      Item delay;
      delay.isDelay = true;
      delay.ms = a.delay;
      items.push_back(delay);
    }
    Item item;
    item.a = a;
    item.a.delay = 0;
    item.pre.swap(hidden);
    items.push_back(std::move(item));
  }
  tail.swap(hidden);
  return items;
}
inline std::vector<MAc> ToEvents(const std::vector<Item> &items,
                                 const std::vector<MAc> &tail) {
  std::vector<MAc> events;
  uint64_t pending = 0;
  for (const auto &item : items) {
    if (item.isDelay) {
      pending += item.ms;
      continue;
    }
    events.insert(events.end(), item.pre.begin(), item.pre.end());
    auto a = item.a;
    a.delay = (DWORD)(std::min)(pending, uint64_t(MAXDWORD));
    pending = 0;
    events.push_back(std::move(a));
  }
  events.insert(events.end(), tail.begin(), tail.end());
  return events;
}

// Change the projection, retaining edits, hidden events and surviving
// selection.
inline void Reproject(std::vector<Item> &items, std::vector<MAc> &tail,
                      std::vector<int> &selection, unsigned visibility) {
  // Uncommitted trailing Delay rows have no following action yet. Keep them
  // as editor rows so changing filters cannot discard an Add delay edit.
  size_t trailingStart = items.size();
  while (trailingStart && items[trailingStart - 1].isDelay)
    --trailingStart;
  std::vector<Item> trailing(items.begin() + trailingStart, items.end());
  std::vector<bool> trailingSelected;
  for (size_t i = trailingStart; i < items.size(); ++i)
    trailingSelected.push_back(
        std::binary_search(selection.begin(), selection.end(), (int)i));
  std::vector<size_t> selectedEvents, selectedDelays;
  std::vector<int> delays;
  size_t ordinal = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    bool selected =
        std::binary_search(selection.begin(), selection.end(), (int)i);
    if (items[i].isDelay) {
      if (selected)
        delays.push_back((int)i);
      continue;
    }
    ordinal += items[i].pre.size();
    if (selected)
      selectedEvents.push_back(ordinal);
    if (!delays.empty())
      selectedDelays.push_back(ordinal);
    delays.clear();
    ++ordinal;
  }
  auto events = ToEvents(items, tail);
  items = FromEvents(events, tail, visibility);
  selection.clear();
  ordinal = 0;
  delays.clear();
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].isDelay) {
      delays.push_back((int)i);
      continue;
    }
    ordinal += items[i].pre.size();
    if (std::binary_search(selectedDelays.begin(), selectedDelays.end(),
                           ordinal))
      selection.insert(selection.end(), delays.begin(), delays.end());
    if (std::binary_search(selectedEvents.begin(), selectedEvents.end(),
                           ordinal))
      selection.push_back((int)i);
    delays.clear();
    ++ordinal;
  }
  size_t base = items.size();
  items.insert(items.end(), trailing.begin(), trailing.end());
  for (size_t i = 0; i < trailingSelected.size(); ++i)
    if (trailingSelected[i])
      selection.push_back((int)(base + i));
}

// Only selected visible actions, plus hidden movement strictly between the
// first/last selected actions. Exclusion removes input, never elapsed time.
inline std::vector<MAc> Selection(const std::vector<Item> &items,
                                  const std::vector<int> &selection,
                                  const PreviewExclusions &exclude = {}) {
  size_t first = items.size(), last = 0;
  for (int row : selection)
    if (row >= 0 && (size_t)row < items.size() && !items[(size_t)row].isDelay) {
      first = (std::min)(first, (size_t)row);
      last = (size_t)row;
    }
  if (first == items.size())
    return {};
  std::vector<MAc> events;
  uint64_t pending = 0, ownDelay = 0, leadingSelected = 0;
  auto append = [&](MAc a, bool play, bool hidden) {
    pending += a.delay;
    if (!play || exclude.Blocked(a)) {
      pending += MovementTime(a);
      return;
    }
    bool hadRelease = a.type == A_MOVE && a.mUp && !a.path.empty();
    if (a.type == A_MOVE) {
      // Hidden movements must not sneak in unselected button actions.
      if (hidden || exclude.ButtonBlocked(a.btn, false))
        a.mDown = false;
      if (hidden || exclude.ButtonBlocked(a.btn, true))
        a.mUp = false;
      // Keep the release dwell even when its button action is excluded.
      if (a.path.empty())
        return;
    }
    if (exclude.movement)
      a.x = a.y = 0; // also suppress implicit cursor positioning
    while (pending > MAXDWORD) {
      MAc wait;
      wait.type = A_WAIT;
      wait.delay = MAXDWORD;
      events.push_back(wait);
      pending -= MAXDWORD;
    }
    a.delay = (DWORD)pending;
    pending = 0;
    bool removedRelease = hadRelease && !a.mUp;
    events.push_back(std::move(a));
    if (removedRelease)
      pending += 10;
  };
  for (size_t i = 0; i <= last; ++i) {
    const auto &item = items[i];
    bool selected =
        std::binary_search(selection.begin(), selection.end(), (int)i);
    if (item.isDelay) {
      ownDelay += item.ms;
      if (i < first && selected)
        leadingSelected += item.ms;
      continue;
    }
    if (i > first)
      for (const auto &hidden : item.pre)
        append(hidden, hidden.type == A_MOVE, true);
    if (i >= first) {
      MAc a = item.a;
      pending += i == first ? leadingSelected : ownDelay;
      a.delay = 0;
      append(a, selected, false);
    }
    ownDelay = 0;
  }
  while (pending) { // internal timing-only event; never written to .onty files
    MAc wait;
    wait.type = A_WAIT;
    wait.delay = (DWORD)(std::min)(pending, uint64_t(MAXDWORD));
    events.push_back(wait);
    pending -= wait.delay;
  }
  return events;
}
} // namespace macroedit
