#pragma once
#include <array>
#include <cmath>
#include <limits>
#include <emmintrin.h>
#include <queue>
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
inline Match NativeFind(const Pixels &screen, const Pixels &snippet,
                        int sensitivity,
                        const std::function<bool()> &checkpoint,
                        const std::vector<RECT> &excluded, bool candidatesOnly = false) {
  Match best;
  const size_t n = snippet.rgb.size();
  auto verify = [&](int x, int y) {
    Comparison full;
    for (int py = 0; py < snippet.h; ++py)
      for (int px = 0; px < snippet.w; ++px) {
        size_t i = size_t(py) * snippet.w + px;
        if ((i & 2047) == 0 && !checkpoint()) {
          Match cancelled = best;
          cancelled.cancelled = true; cancelled.found = false;
          return cancelled;
        }
        full.Add(snippet.rgb[i], screen.rgb[size_t(y + py) * screen.w + x + px]);
      }
    double score = full.Score() * 100;
    return Match{score + 1e-8 >= sensitivity, false, x, y, score, snippet.w, snippet.h};
  };
  if (n >= 256 || candidatesOnly) {
    // Prioritize likely locations, but verify every pixel and retain the exhaustive fallback.
    using Candidate = std::pair<double, std::pair<int, int>>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> candidates;
    std::array<size_t, 64> templateOffsets{}, screenOffsets{};
    // Spread the first eight samples across the image instead of along one row.
    for (size_t i = 0; i < templateOffsets.size(); ++i) {
      int cell = int(i * 17 % 64);
      int px = (2 * (cell % 8) + 1) * snippet.w / 16;
      int py = (2 * (cell / 8) + 1) * snippet.h / 16;
      templateOffsets[i] = size_t(py) * snippet.w + px;
      screenOffsets[i] = size_t(py) * screen.w + px;
    }
    unsigned tick = 0;
    unsigned immediateChecks = 0;
    for (int y = 0; y <= screen.h - snippet.h; ++y)
      for (int x = 0; x <= screen.w - snippet.w; ++x) {
        if ((++tick & 127) == 0 && !checkpoint()) return {false, true};
        if (Excluded(x, y, snippet.w, snippet.h, excluded)) continue;
        Comparison quick;
        size_t origin = size_t(y) * screen.w + x;
        bool promising = true;
        for (size_t i = 0; i < templateOffsets.size(); ++i) {
          quick.Add(snippet.rgb[templateOffsets[i]], screen.rgb[origin + screenOffsets[i]]);
          if (i == 7 && quick.Score() < (std::max)(0.0, sensitivity / 100.0 - 0.25)) {
            promising = false;
            break;
          }
        }
        if (!promising) continue;
        Candidate candidate{quick.Score(), {x, y}};
        double variance = 0;
        if (quick.error == 0 && immediateChecks < 8) {
          for (int c = 0; c < 3; ++c)
            variance += quick.tt[c] - quick.t[c] * quick.t[c] / quick.n;
          if (variance >= .001 || n <= 64) {
            ++immediateChecks;
            auto match = verify(x, y);
            if (match.cancelled) return match;
            if (match.score >= 100.0 - 1e-8) { match.score = 100; return match; }
          }
        }
        if (candidates.size() < 32) candidates.push(candidate);
        else if (candidate.first > candidates.top().first) {
          candidates.pop();
          candidates.push(candidate);
        }
      }
    std::array<Candidate, 32> ranked{};
    size_t count = candidates.size();
    for (size_t i = count; i > 0; --i) {
      ranked[i - 1] = candidates.top();
      candidates.pop();
    }
    for (size_t candidate = 0; candidate < count; ++candidate) {
      auto [x, y] = ranked[candidate].second;
      auto match = verify(x, y);
      if (match.cancelled) return match;
      if (match.score > best.score || (!best.found && match.found)) best = match;
      if (best.score >= 100.0 - 1e-8) {
        best.score = 100;
        return best;
      }
    }
    if (best.found) return best;
  }
  if (candidatesOnly) return best;
  struct Moments {
    double sum[3]{}, squares = 0;
    void Pixel(uint32_t pixel, int sign) {
      for (int c = 0; c < 3; ++c) {
        double value = (pixel >> (c * 8)) & 255;
        sum[c] += sign * value;
        squares += sign * value * value;
      }
    }
    void Shift(const Moments &other, int sign) {
      for (int c = 0; c < 3; ++c) sum[c] += sign * other.sum[c];
      squares += sign * other.squares;
    }
    double Variance(size_t n) const {
      double value = squares;
      for (double channel : sum) value -= channel * channel / n;
      return (std::max)(0.0, value);
    }
  } pattern;
  std::vector<Moments> prefixes;
  prefixes.reserve(n / 64);
  for (size_t i = 0; i < n; ++i) {
    if ((i & 2047) == 0 && !checkpoint()) return {false, true};
    pattern.Pixel(snippet.rgb[i], 1);
    if ((i & 63) == 63) prefixes.push_back(pattern);
  }
  const double tv = pattern.Variance(n);
  auto sum4 = [](__m128i value) {
    int lanes[4];
    _mm_storeu_si128((__m128i *)lanes, value);
    return double(lanes[0]) + lanes[1] + lanes[2] + lanes[3];
  };
  std::vector<Moments> columns(screen.w);
  for (int y = 0; y < snippet.h; ++y) {
    if ((y & 15) == 0 && !checkpoint()) return {false, true};
    for (int x = 0; x < screen.w; ++x)
      columns[x].Pixel(screen.rgb[size_t(y) * screen.w + x], 1);
  }
  unsigned tick = 0;
  for (int y = 0; y <= screen.h - snippet.h; ++y) {
    if (y) {
      if (!checkpoint()) return {false, true};
      for (int x = 0; x < screen.w; ++x) {
        columns[x].Pixel(screen.rgb[size_t(y - 1) * screen.w + x], -1);
        columns[x].Pixel(screen.rgb[size_t(y + snippet.h - 1) * screen.w + x], 1);
      }
    }
    Moments window;
    for (int x = 0; x < snippet.w; ++x) window.Shift(columns[x], 1);
    for (int x = 0; x <= screen.w - snippet.w; ++x) {
      if (x) {
        window.Shift(columns[x - 1], -1);
        window.Shift(columns[x + snippet.w - 1], 1);
      }
      if ((++tick & 127) == 0 && !checkpoint()) return {false, true};
      if (Excluded(x, y, snippet.w, snippet.h, excluded)) continue;
      const double sv = window.Variance(n);
      double score = 0;
      if (tv < .001 && sv < .001) {
        double error = 0;
        for (int c = 0; c < 3; ++c)
          error += std::abs(pattern.sum[c] - window.sum[c]);
        score = (std::max)(0.0, 1.0 - error / (765.0 * n)) * 100;
      } else if (tv < .001) {
        double error = 0;
        bool rejected = false;
        size_t done = 0;
        const auto rgb = _mm_set1_epi32(0xffffff);
        for (int sy = 0; sy < snippet.h && !rejected; ++sy)
          for (int sx = 0; sx < snippet.w;) {
            if ((done & 2047) == 0 && !checkpoint()) {
              best.cancelled = true; best.found = false; return best;
            }
            auto ti = size_t(sy) * snippet.w + sx;
            auto si = size_t(y + sy) * screen.w + x + sx;
            if (sx + 4 <= snippet.w) {
              auto t = _mm_and_si128(_mm_loadu_si128((const __m128i *)&snippet.rgb[ti]), rgb);
              auto s = _mm_and_si128(_mm_loadu_si128((const __m128i *)&screen.rgb[si]), rgb);
              error += sum4(_mm_sad_epu8(t, s));
              done += 4; sx += 4;
            } else {
              for (int c = 0; c < 3; ++c)
                error += std::abs(int((snippet.rgb[ti] >> (c * 8)) & 255) -
                                  int((screen.rgb[si] >> (c * 8)) & 255));
              ++done; ++sx;
            }
            if (best.score > 0 && (1.0 - error / (765.0 * n)) * 100 + 1e-7 <
                                  (std::max)(double(sensitivity), best.score)) {
              rejected = true;
              break;
            }
          }
        if (rejected) continue;
        score = (std::max)(0.0, 1.0 - error / (765.0 * n)) * 100;
      } else if (sv >= .001) {
        bool rejected = false;
        const double rounding = 128 * std::numeric_limits<double>::epsilon() *
                                (pattern.squares + window.squares);
        const double denominator = std::sqrt((std::max)(0.0, tv - rounding) *
                                             (std::max)(0.0, sv - rounding));
        if (tv >= .001 && denominator > 0) {
          Moments partial;
          double dot = 0;
          size_t done = 0;
          const auto zero = _mm_setzero_si128(), mask = _mm_set1_epi32(255),
                     rgb = _mm_set1_epi32(0xffffff);
          __m128i sums[3] = {zero, zero, zero}, squares = zero, products = zero;
          for (int sy = 0; sy < snippet.h && !rejected; ++sy)
            for (int sx = 0; sx < snippet.w;) {
              if ((done & 2047) == 0 && !checkpoint()) {
                best.cancelled = true; best.found = false; return best;
              }
              auto ti = size_t(sy) * snippet.w + sx;
              auto si = size_t(y + sy) * screen.w + x + sx;
              if (sx + 4 <= snippet.w && (done & 63) <= 60) {
                auto t = _mm_and_si128(_mm_loadu_si128((const __m128i *)&snippet.rgb[ti]), rgb);
                auto s = _mm_and_si128(_mm_loadu_si128((const __m128i *)&screen.rgb[si]), rgb);
                sums[0] = _mm_add_epi32(sums[0], _mm_and_si128(s, mask));
                sums[1] = _mm_add_epi32(sums[1], _mm_and_si128(_mm_srli_epi32(s, 8), mask));
                sums[2] = _mm_add_epi32(sums[2], _mm_srli_epi32(s, 16));
                auto sl = _mm_unpacklo_epi8(s, zero), sh = _mm_unpackhi_epi8(s, zero);
                squares = _mm_add_epi32(squares, _mm_add_epi32(_mm_madd_epi16(sl, sl),
                                                                            _mm_madd_epi16(sh, sh)));
                products = _mm_add_epi32(products,
                    _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(t, zero), sl),
                                  _mm_madd_epi16(_mm_unpackhi_epi8(t, zero), sh)));
                done += 4; sx += 4;
              } else {
                partial.Pixel(screen.rgb[si], 1);
                for (int c = 0; c < 3; ++c)
                  dot += double((snippet.rgb[ti] >> (c * 8)) & 255) *
                         ((screen.rgb[si] >> (c * 8)) & 255);
                ++done; ++sx;
              }
              if ((done & 63) != 0) continue;
              for (int c = 0; c < 3; ++c) { partial.sum[c] += sum4(sums[c]); sums[c] = zero; }
              partial.squares += sum4(squares); squares = zero;
              dot += sum4(products); products = zero;
              const auto &prefix = prefixes[done / 64 - 1];
              double usedT = prefix.squares, usedS = partial.squares, covariance = dot;
              for (int c = 0; c < 3; ++c) {
                double tm = pattern.sum[c] / n, sm = window.sum[c] / n;
                usedT += -2 * tm * prefix.sum[c] + done * tm * tm;
                usedS += -2 * sm * partial.sum[c] + done * sm * sm;
                covariance += -tm * partial.sum[c] - sm * prefix.sum[c] + done * tm * sm;
              }
              // Integer SIMD sums retain the conservative bound without slow per-pixel doubles.
              double upper = (std::clamp)((covariance + rounding +
                  std::sqrt((std::max)(0.0, tv - usedT + rounding) *
                            (std::max)(0.0, sv - usedS + rounding))) /
                  denominator, 0.0, 1.0);
              if (upper * 100 + 1e-7 < (std::max)(double(sensitivity), best.score)) {
                rejected = true;
                break;
              }
            }
        }
        if (rejected) continue;
        auto match = verify(x, y);
        if (match.cancelled) return match;
        score = match.score;
      }
      if (score > best.score || (!best.found && score >= sensitivity))
        best = {score + 1e-8 >= sensitivity, false, x, y, score,
                snippet.w, snippet.h};
      if (best.found && best.score >= 100.0 - 1e-8) {
        best.score = 100;
        return best;
      }
    }
  }
  return best;
}
inline Match Find(
    const Pixels &screen, const Pixels &snippet, int sensitivity,
    const std::function<bool()> &checkpoint = [] { return true; },
    const std::vector<RECT> &excluded = {}, bool candidatesOnly = false) {
  if (!Dimensions(snippet.w, snippet.h) ||
      snippet.rgb.size() != size_t(snippet.w) * snippet.h ||
      !ScreenDimensions(screen.w, screen.h) ||
      screen.rgb.size() != size_t(screen.w) * screen.h ||
      snippet.w > screen.w || snippet.h > screen.h)
    return {};
  if (!checkpoint())
    return {false, true};
  return NativeFind(screen, snippet, sensitivity, checkpoint, excluded, candidatesOnly);
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
    const std::vector<RECT> &excluded = {}, bool candidatesOnly = false) {
  try {
    auto best = Find(screen, snippet, sensitivity, checkpoint, excluded, candidatesOnly);
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
      auto match = Find(screen, resized, sensitivity, checkpoint, excluded, candidatesOnly);
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
