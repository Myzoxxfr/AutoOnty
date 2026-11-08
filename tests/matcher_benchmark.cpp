#include "engine/image.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
int wmain(int argc, wchar_t **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc == 3) {
    auto reference = image::Load(argv[1]), screenshot = image::Load(argv[2]);
    assert(reference && screenshot);
    auto begin = std::chrono::steady_clock::now();
    auto match = image::Find(*screenshot, *reference, 85, [&] {
      return std::chrono::steady_clock::now() - begin < std::chrono::seconds(3);
    });
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();
    printf("Supplied image pair: %lld ms; found=%d cancelled=%d score=%.4f%% at %d,%d\n",
           (long long)ms, match.found, match.cancelled, match.score, match.x, match.y);
    assert(match.found && !match.cancelled);
    return 0;
  }
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
  std::mt19937 random(1234);
  for (auto &pixel : screen.rgb) pixel = random() & 0xffffff;
  for (auto &pixel : snippet.rgb) pixel = random() & 0xffffff;
  for (int y = 0; y < snippet.h; ++y)
    for (int x = 0; x < snippet.w; ++x)
      screen.rgb[(900 + y) * screen.w + 1710 + x] = snippet.rgb[y * snippet.w + x];
  begin = std::chrono::steady_clock::now();
  match = image::Find(screen, snippet, 85, [&] {
    return std::chrono::steady_clock::now() - begin < std::chrono::milliseconds(750);
  });
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - begin).count();
  wprintf(L"Textured desktop: %lld ms; found=%d cancelled=%d score=%.2f%%\n",
          ms, match.found, match.cancelled, match.score);
  assert(match.found && !match.cancelled && match.x == 1710 && match.y == 900);
  for (int height : {64, 256}) {
    image::Pixels larger{96, height, std::vector<uint32_t>(96 * height)};
    for (auto &pixel : larger.rgb) pixel = random() & 0xffffff;
    for (int y = 0; y < larger.h; ++y)
      for (int x = 0; x < larger.w; ++x)
        screen.rgb[size_t(700 + y) * screen.w + 1710 + x] = larger.rgb[size_t(y) * larger.w + x];
    begin = std::chrono::steady_clock::now();
    auto detected = image::Find(screen, larger, 85, [&] {
      return std::chrono::steady_clock::now() - begin < std::chrono::milliseconds(750);
    });
    assert(detected.found && !detected.cancelled && detected.score == 100 &&
           detected.x == 1710 && detected.y == 700);
  }
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

  image::Pixels large{128, 64, std::vector<uint32_t>(128 * 64)},
      desktop{256, 96, std::vector<uint32_t>(256 * 96)};
  for (auto &pixel : large.rgb) pixel = random() & 0xffffff;
  for (auto &pixel : desktop.rgb) pixel = random() & 0xffffff;
  for (int y = 0; y < large.h; ++y)
    for (int x = 0; x < large.w; ++x)
      desktop.rgb[size_t(y + 20) * desktop.w + x + 100] = large.rgb[size_t(y) * large.w + x];
  found = image::Find(desktop, large, 100);
  assert(found.found && found.score == 100 && found.x == 100 && found.y == 20);
  assert(!image::Find(desktop, large, 100, [] { return false; }).found);
  assert(!image::Find(desktop, large, 100, [] { return true; }, {{100, 20, 228, 84}}).found);

  // Misleading samples must still reach the full-search fallback.
  for (int gy = 0; gy < 8; ++gy)
    for (int gx = 0; gx < 8; ++gx) {
      int x = (2 * gx + 1) * large.w / 16;
      int y = (2 * gy + 1) * large.h / 16;
      desktop.rgb[size_t(y + 20) * desktop.w + x + 100] ^= 0xffffff;
    }
  found = image::Find(desktop, large, 95);
  assert(found.found && found.score > 98 && found.x == 100 && found.y == 20);
}
