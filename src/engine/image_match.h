#pragma once
#include <array>
#include <cmath>
#include <numeric>
namespace image {
struct Match {
  bool found = false, cancelled = false;
  int x = 0, y = 0;
  double score = 0;
  int w = 0, h = 0;
  bool failed = false;
};
inline const wchar_t *Backend() {
  return L"Portable";
}
inline bool Excluded(int x, int y, int w, int h,
                     const std::vector<RECT> &regions) {
  POINT p{x + w / 2, y + h / 2};
  for (const auto &r : regions)
    if (PtInRect(&r, p))
      return true;
  return false;
}
struct Comparison {
  std::array<double, 3> t{}, s{}, tt{}, ss{}, ts{};
  double error = 0;
  size_t n = 0;
  void Add(uint32_t a, uint32_t b) {
    ++n;
    for (int c = 0; c < 3; ++c) {
      double x = (a >> (c * 8)) & 255, y = (b >> (c * 8)) & 255;
      t[c] += x;
      s[c] += y;
      tt[c] += x * x;
      ss[c] += y * y;
      ts[c] += x * y;
      error += std::abs(x - y);
    }
  }
  double Score() const {
    double tv = 0, sv = 0, cov = 0;
    for (int c = 0; c < 3; ++c) {
      tv += tt[c] - t[c] * t[c] / n;
      sv += ss[c] - s[c] * s[c] / n;
      cov += ts[c] - t[c] * s[c] / n;
    }
    double color = error / (765.0 * n);
    if (tv < 0.001)
      return (std::max)(0.0, 1.0 - color);
    if (sv < 0.001)
      return 0;
    // Correlation recognizes the shape through small brightness/color changes;
    // a light RGB penalty still ranks an exact copy above a recolored copy.
    return (std::clamp)(cov / std::sqrt(tv * sv), 0.0, 1.0) *
           (1.0 - 0.15 * color);
  }
};
inline std::vector<size_t> Samples(const Pixels &p) {
  if (p.rgb.size() <= 40) {
    std::vector<size_t> out(p.rgb.size());
    std::iota(out.begin(), out.end(), 0);
    return out;
  }
  struct Point {
    size_t index;
    int detail;
  };
  std::array<Point, 40> cells{};
  for (auto &c : cells)
    c.detail = -1;
  auto distance = [](uint32_t a, uint32_t b) {
    int d = 0;
    for (int c = 0; c < 3; ++c)
      d += std::abs(int((a >> (c * 8)) & 255) - int((b >> (c * 8)) & 255));
    return d;
  };
  for (int y = 0; y < p.h; ++y)
    for (int x = 0; x < p.w; ++x) {
      size_t i = size_t(y) * p.w + x;
      int detail = 0;
      if (x + 1 < p.w)
        detail += distance(p.rgb[i], p.rgb[i + 1]);
      if (y + 1 < p.h)
        detail += distance(p.rgb[i], p.rgb[i + p.w]);
      int cell = (y * 5 / p.h) * 8 + x * 8 / p.w;
      if (detail > cells[cell].detail)
        cells[cell] = {i, detail};
    }
  std::sort(cells.begin(), cells.end(),
            [](auto a, auto b) { return a.detail > b.detail; });
  std::vector<size_t> out;
  for (auto c : cells)
    if (c.detail >= 0)
      out.push_back(c.index);
  return out;
}
inline Match NativeFind(const Pixels &screen, const Pixels &snippet,
                        int sensitivity,
                        const std::function<bool()> &checkpoint,
                        const std::vector<RECT> &excluded) {
  Match best;
  auto sample = Samples(snippet);
  double threshold = (std::clamp)(sensitivity, 0, 100) / 100.0;
  double highestSample = -1;
  unsigned tick = 0;
  for (int y = 0; y <= screen.h - snippet.h; ++y)
    for (int x = 0; x <= screen.w - snippet.w; ++x) {
      if ((++tick & 127) == 0 && !checkpoint()) {
        best.cancelled = true;
        best.found = false;
        return best;
      }
      if (Excluded(x, y, snippet.w, snippet.h, excluded))
        continue;
      Comparison quick;
      bool reject = false;
      size_t k = 0;
      for (auto i : sample) {
        quick.Add(snippet.rgb[i],
                  screen.rgb[size_t(y + i / snippet.w) * screen.w + x +
                             i % snippet.w]);
        ++k;
        if ((k == 8 || k == 16) &&
            quick.Score() < (std::max)(0.0, threshold - 0.25)) {
          reject = true;
          break;
        }
      }
      if (reject)
        continue;
      double score = quick.Score();
      if (score < threshold - 0.12 && score <= highestSample)
        continue;
      highestSample = (std::max)(highestSample, score);
      Comparison full;
      for (int sy = 0; sy < snippet.h; ++sy)
        for (int sx = 0; sx < snippet.w; ++sx) {
          size_t i = size_t(sy) * snippet.w + sx;
          if ((i & 2047) == 0 && !checkpoint()) {
            best.cancelled = true;
            best.found = false;
            return best;
          }
          full.Add(snippet.rgb[i],
                   screen.rgb[size_t(y + sy) * screen.w + x + sx]);
        }
      score = full.Score() * 100.0;
      if (score > best.score || (!best.found && score >= sensitivity))
        best = {score + 1e-8 >= sensitivity,
                false,
                x,
                y,
                score,
                snippet.w,
                snippet.h};
      if (best.found && best.score >= 100.0 - 1e-8) {
        best.score = 100;
        return best;
      }
    }
  return best;
}
inline Match Find(
    const Pixels &screen, const Pixels &snippet, int sensitivity,
    const std::function<bool()> &checkpoint = [] { return true; },
    const std::vector<RECT> &excluded = {}) {
  if (!Dimensions(snippet.w, snippet.h) ||
      snippet.rgb.size() != size_t(snippet.w) * snippet.h ||
      !ScreenDimensions(screen.w, screen.h) ||
      screen.rgb.size() != size_t(screen.w) * screen.h ||
      snippet.w > screen.w || snippet.h > screen.h)
    return {};
  if (!checkpoint())
    return {false, true};
  return NativeFind(screen, snippet, sensitivity, checkpoint, excluded);
}
inline Pixels ResizeTo(const Pixels &p, int width, int height) {
  Pixels out;
  out.w = width;
  out.h = height;
  if (!Dimensions(out.w, out.h))
    return {};
  out.rgb.resize(size_t(out.w) * out.h);
  for (int y = 0; y < out.h; ++y)
    for (int x = 0; x < out.w; ++x) {
      double sx = (x + 0.5) * p.w / out.w - 0.5,
             sy = (y + 0.5) * p.h / out.h - 0.5;
      int x0 = (std::clamp)(int(std::floor(sx)), 0, p.w - 1),
          y0 = (std::clamp)(int(std::floor(sy)), 0, p.h - 1);
      int x1 = (std::min)(x0 + 1, p.w - 1), y1 = (std::min)(y0 + 1, p.h - 1);
      double fx = (std::clamp)(sx - x0, 0.0, 1.0),
             fy = (std::clamp)(sy - y0, 0.0, 1.0);
      uint32_t color = 0;
      for (int c = 0; c < 3; ++c) {
        double a =
            ((p.rgb[size_t(y0) * p.w + x0] >> (c * 8)) & 255) * (1 - fx) +
            ((p.rgb[size_t(y0) * p.w + x1] >> (c * 8)) & 255) * fx;
        double b =
            ((p.rgb[size_t(y1) * p.w + x0] >> (c * 8)) & 255) * (1 - fx) +
            ((p.rgb[size_t(y1) * p.w + x1] >> (c * 8)) & 255) * fx;
        color |= uint32_t(std::lround(a * (1 - fy) + b * fy)) << (c * 8);
      }
      out.rgb[size_t(y) * out.w + x] = color;
    }
  return out;
}
inline Pixels Resize(const Pixels &p, double factor) {
  return ResizeTo(p, (std::max)(1, int(std::lround(p.w * factor))),
                  (std::max)(1, int(std::lround(p.h * factor))));
}
inline Match Scan(
    const Pixels &screen, const Pixels &snippet, int sensitivity, bool sizes,
    const std::function<bool()> &checkpoint = [] { return true; },
    const std::vector<RECT> &excluded = {}) {
  try {
    auto best = Find(screen, snippet, sensitivity, checkpoint, excluded);
    if (best.failed)
      return best;
    if (best.cancelled || !sizes || best.score >= 100.0 - 1e-8)
      return best;
    for (double factor : {0.9, 1.1, 0.75, 1.25, 1.5, 2.0, 0.5}) {
      if (!checkpoint())
        return {false, true};
      auto resized = Resize(snippet, factor);
      if (resized.rgb.empty())
        continue;
      auto match = Find(screen, resized, sensitivity, checkpoint, excluded);
      if (match.cancelled)
        return match;
      if (match.failed)
        return match;
      if (match.score > best.score)
        best = match;
      if (best.score >= 100.0 - 1e-8)
        return best;
    }
    return best;
  } catch (const std::bad_alloc &) {
    Match error;
    error.failed = true;
    return error;
  }
}
} // namespace image
