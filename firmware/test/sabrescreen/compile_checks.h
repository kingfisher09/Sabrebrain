#pragma once

#include "sabre_screen_frame.h"
#include "sabrescreen.h"
#include <type_traits>
#include <utility>

namespace screen_checks {
static_assert(sabre_screen::mask_bytes(150, 23) == 863, "Existing video mask stride");
static_assert(sabre_screen::mask_bytes(3, 5) == 4, "Partial mask bytes round up");
static_assert(sabre_screen::radial_pixel(0, 100, 23) == 0, "Centre radius");
static_assert(sabre_screen::radial_pixel(50, 100, 23) == 11, "Default radial midpoint");
static_assert(sabre_screen::radial_pixel(100, 100, 600) == 599, "Mappings exceed eight bits");
static_assert(sabre_screen::radial_pixel(100, 100, 1) == 0, "Single radial sample");
static_assert(sabre_screen::radial_pixel(0, 0, 600) == 0, "Centre-only LED layout");
static_assert(sabre_screen::angular_pixel(90, 4) == 1, "Media angular resolution");
static_assert(sabre_screen::angular_pixel(180, 150) == 75, "Existing angular resolution");
static_assert(sabre_screen::angular_pixel(359.9f, 150) == 0, "Nearest sample wraps at 360 degrees");
static_assert(sabre_screen::angular_pixel(180, 1) == 0, "Single angular sample");

constexpr bool rectangular_frame() {
  const uint8_t previous[] = {1, 2, 3, 4, 5, 6};
  const uint8_t mask[] = {0xD8, 0x08};  // New, previous pixel, previous frame, previous row, new, previous pixel.
  const uint8_t diffs[] = {10, 20};
  uint8_t next[6] = {};
  sabre_screen::decode_frame(mask, diffs, previous, next, 2, 3);
  return next[0] == 10 && next[1] == 10 && next[2] == 3 &&
         next[3] == 10 && next[4] == 20 && next[5] == 20;
}
static_assert(rectangular_frame(), "All four video modes use media dimensions");

constexpr uint8_t masks[2][4] = {};
constexpr const CRGB* diffs[2] = {nullptr, nullptr};
constexpr SabreVid video{3, 5, 2, 20, masks, diffs};
static_assert(video.numAngles == 3 && video.numRadii == 5, "Video retains its own dimensions");
static_assert(video.maskBytesPerFrame == 4, "Generated header accepts nondefault mask width");
static_assert(video.maskFrameCount == 2, "Generated mask array frame count is retained");
static_assert(std::is_same<decltype(show_still(std::declval<const CRGB (&)[7][11]>())), bool>::value,
              "Hardcoded still image dimensions are inferred");
}  // namespace screen_checks
