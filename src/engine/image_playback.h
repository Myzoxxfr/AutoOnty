#pragma once
// Internal worker implementation. All input, pause/focus and held-input cleanup
// remain on the existing playback worker; nested action macros never start a
// second worker.
static bool
PlayImageAction(const MAc &a, double &base, double &pausedMs,
                const std::function<bool()> &checkpoint,
                const std::function<bool(const std::vector<MAc> &)> &runChild) {
  const auto &o = a.imageOptions;
  double pauseAtStart = pausedMs, began = NowMs(),
         waitBegan = NowMs() - pausedMs;
  auto clock = [&] { return NowMs() - pausedMs; };
  double activeBegan = clock();
  struct AccountTime {
    double &base, &paused;
    double began, pause;
    ~AccountTime() { base += NowMs() - began - (paused - pause); }
  } account{base, pausedMs, began, pauseAtStart};
  unsigned matches = 0;
  bool waitingClear = false, waiting = true;
  std::vector<RECT> seen;
  auto stage = [&](int value) {
    g_imageStage = value;
    g_imageMatches = (int)(std::min)(matches, unsigned(INT_MAX));
  };
  auto sleep = [&](double ms) {
    double end = clock() + ms;
    while (clock() < end) {
      if (!checkpoint())
        return false;
      if ((o.duration && clock() - activeBegan >= o.duration) ||
          (waiting && a.imageTimeout && clock() - waitBegan >= a.imageTimeout))
        return true;
      Sleep(5);
    }
    return true;
  };
  auto outcome = [&](int value, bool timeout) {
    if (value == IO_STOP) {
      g_playStop = true;
      return false;
    }
    if (value == IO_PAUSE) {
      stage(timeout ? IS_PAUSE_TIMEOUT : IS_PAUSE_FOUND);
      g_pauseRequested = true;
      if (!checkpoint())
        return false;
    }
    return true;
  };
  auto finished = [&] {
    return o.duration && clock() - activeBegan >= o.duration;
  };
  auto click = [&](POINT imagePoint) -> int {
    POINT point = imagePoint;
    if (o.target == IT_CURSOR) {
      if (!GetCursorPos(&point))
        return 2;
    } else if (o.target == IT_FIXED)
      point = {o.targetX, o.targetY};
    double pause = pausedMs;
    if (!checkpoint())
      return -1;
    if (o.target != IT_CURSOR && !SetCursorPos(point.x, point.y))
      return 2;
    if (!checkpoint())
      return -1;
    if (pausedMs != pause)
      return 0; // the next scan must confirm fresh pixels
    POINT placed{};
    if (!GetCursorPos(&placed) || placed.x != point.x || placed.y != point.y)
      return 2;
    BtnDown(0);
    BtnUp(0);
    return 1;
  };
  if (!a.image) {
    stage(IS_CAPTURE_FAILED);
    return outcome(o.onTimeout, true);
  }
  for (;;) {
    waiting = true;
    if (!checkpoint())
      return false;
    if (finished())
      return true;
    double waited = clock() - waitBegan;
    if (a.imageTimeout && waited >= a.imageTimeout) {
      stage(IS_TIMED_OUT);
      return outcome(o.onTimeout, true);
    }
    g_imageRemaining =
        a.imageTimeout ? (DWORD)(std::max)(0.0, a.imageTimeout - waited) : 0;
    stage(waitingClear ? IS_WAIT_CLEAR : IS_SEARCHING);
    int ox = 0, oy = 0;
    double scanPause = pausedMs;
    auto screen = image::Screen(ox, oy);
    if (!screen) {
      stage(IS_CAPTURE_FAILED);
      if (!sleep(150))
        return false;
      continue;
    }
    std::vector<RECT> excluded;
    if (o.repeat == IR_LOCATIONS)
      for (auto r : seen) {
        OffsetRect(&r, -ox, -oy);
        excluded.push_back(r);
      }
    bool expired = false, restart = false;
    auto scanCheckpoint = [&] {
      if (!checkpoint())
        return false;
      restart = pausedMs != scanPause;
      expired = finished() ||
                (a.imageTimeout && clock() - waitBegan >= a.imageTimeout);
      return !expired && !restart;
    };
    auto match = image::Scan(*screen, *a.image, a.sensitivity, o.differentSizes,
                             scanCheckpoint, excluded);
    g_imageScore = (int)std::floor(match.score + 1e-8);
    if (restart)
      continue;
    if (expired) {
      if (finished())
        return true;
      stage(IS_TIMED_OUT);
      return outcome(o.onTimeout, true);
    }
    if (match.cancelled)
      return false;
    if (match.failed) {
      stage(IS_MATCH_FAILED);
      return outcome(o.onTimeout, true);
    }
    if (!checkpoint())
      return false;
    if (pausedMs != scanPause)
      continue;
    if (waitingClear) {
      if (!match.found) {
        waitingClear = false;
        waitBegan = clock();
      }
      if (!sleep(150))
        return false;
      continue;
    }
    if (!match.found) {
      if (!sleep(
              (std::min)(150.0, a.imageTimeout
                                    ? (std::max)(0.0, a.imageTimeout -
                                                          (clock() - waitBegan))
                                    : 150.0)))
        return false;
      continue;
    }
    int width = match.w ? match.w : a.image->w,
        height = match.h ? match.h : a.image->h;
    POINT center{ox + match.x + width / 2, oy + match.y + height / 2};
    waiting = false;
    if (o.action == IA_CLICK) {
      stage(IS_CLICKING);
      int result = click(center);
      if (result < 0)
        return false;
      if (result == 0)
        continue;
      if (result == 2) {
        stage(IS_CLICK_FAILED);
        return outcome(o.onFound, false);
      }
    } else if (o.action == IA_RUN_MACRO) {
      stage(IS_RUNNING);
      if (!a.imageMacro || !runChild(*a.imageMacro))
        return false;
    } else if (o.action == IA_AUTOCLICK) {
      stage(IS_CLICKING);
      double started = clock(), next = clock(), verify = clock() + 150;
      unsigned clicks = 0;
      image::Pixels scaled;
      if (width != a.image->w || height != a.image->h)
        scaled = image::ResizeTo(*a.image, width, height);
      const auto &pattern = scaled.rgb.empty() ? *a.image : scaled;
      for (;;) {
        if (!checkpoint())
          return false;
        if (finished() || (o.clicks && clicks >= o.clicks) ||
            (o.autoDuration && clock() - started >= o.autoDuration))
          break;
        if (clock() >= verify) {
          double pausedBefore = pausedMs;
          int sx, sy;
          auto current = image::Screen(sx, sy);
          if (!current)
            break;
          int x = center.x - width / 2 - sx, y = center.y - height / 2 - sy;
          if (x < 0 || y < 0 || x + pattern.w > current->w ||
              y + pattern.h > current->h)
            break;
          image::Comparison comparison;
          bool cancelled = false;
          for (int py = 0; py < pattern.h; ++py) {
            if ((py & 15) == 0 && !checkpoint()) {
              cancelled = true;
              break;
            }
            for (int px = 0; px < pattern.w; ++px)
              comparison.Add(
                  pattern.rgb[size_t(py) * pattern.w + px],
                  current->rgb[size_t(y + py) * current->w + x + px]);
          }
          if (cancelled)
            return false;
          if (pausedMs != pausedBefore)
            continue;
          if (comparison.Score() * 100 + 1e-8 < a.sensitivity)
            break;
          verify = clock() + 150;
        }
        if (clock() >= next) {
          int result = click(center);
          if (result < 0)
            return false;
          if (result == 0) {
            verify = clock();
            continue;
          }
          if (result == 2) {
            stage(IS_CLICK_FAILED);
            break;
          }
          ++clicks;
          next = clock() + o.interval;
        }
        if (!sleep((std::min)(5.0, (std::max)(1.0, next - clock()))))
          return false;
      }
    }
    ++matches;
    g_imageMatches = (int)(std::min)(matches, unsigned(INT_MAX));
    if (!outcome(o.onFound, false))
      return false;
    if (o.repeat == IR_ONCE || (o.maxMatches && matches >= o.maxMatches) ||
        finished())
      return true;
    if (o.repeat == IR_LOCATIONS) {
      if (seen.size() >= 100000)
        return true;
      seen.push_back({center.x - width / 2, center.y - height / 2,
                      center.x + (width + 1) / 2, center.y + (height + 1) / 2});
    }
    waitingClear = o.repeat == IR_REAPPEAR;
    waitBegan = clock();
    waiting = true;
    if (!sleep((std::max)(o.cooldown, DWORD(100))))
      return false;
  }
}
