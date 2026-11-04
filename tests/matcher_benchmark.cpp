#include "engine/image.h"
#include <cassert>
#include <chrono>
#include <cstdio>
int main() {
  image::Pixels snippet{96, 40, std::vector<uint32_t>(96 * 40, 0xf0f0f0)};
  for (int y = 6; y < 34; ++y)
    for (int x = 7; x < 89; ++x)
      if ((x % 13 < 3 && y < 30) || (y % 11 < 2 && x < 65))
        snippet.rgb[y * 96 + x] = 0x202030;
  image::Pixels screen{1920, 1080,
                       std::vector<uint32_t>(1920 * 1080, 0xf0f0f0)};
  for (int y = 0; y < snippet.h; ++y)
    for (int x = 0; x < snippet.w; ++x) {
      auto color = snippet.rgb[y * 96 + x];
      if (color != 0xf0f0f0)
        color += 0x040404;
      screen.rgb[(900 + y) * 1920 + 1710 + x] = color;
    }
  auto begin = std::chrono::steady_clock::now();
  auto match = image::Find(screen, snippet, 95);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin)
                .count();
  wprintf(L"%ls: 1920x1080 screenshot, 96x40 image: %lld ms, %.2f%%, (%d,%d)\n",
          image::Backend(), ms, match.score, match.x, match.y);
  assert(match.found && match.x == 1710 && match.y == 900);
  screen.rgb.assign(screen.rgb.size(), 0xf0f0f0);
  assert(!image::Find(screen, snippet, 95).found);
  image::Pixels tinyTemplate{
      3, 2, {0x102030, 0x506070, 0x8090a0, 0x405060, 0x708090, 0xaabbcc}};
  assert(image::ResizeTo(tinyTemplate, tinyTemplate.w, tinyTemplate.h).rgb ==
         tinyTemplate.rgb);
  auto scaled = image::Resize(tinyTemplate, 2);
  image::Pixels enlarged{
      scaled.w + 4, scaled.h + 3,
      std::vector<uint32_t>((scaled.w + 4) * (scaled.h + 3), 0xffffff)};
  for (int y = 0; y < scaled.h; ++y)
    for (int x = 0; x < scaled.w; ++x)
      enlarged.rgb[(y + 1) * enlarged.w + x + 2] = scaled.rgb[y * scaled.w + x];
  auto found = image::Scan(enlarged, tinyTemplate, 95, true);
  assert(found.found);
}
