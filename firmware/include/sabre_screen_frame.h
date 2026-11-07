#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sabre_screen {
constexpr size_t mask_bytes(size_t angles, size_t radii) {
  return (angles * radii + 3) / 4;
}

constexpr uint16_t radial_pixel(float radius, float max_radius, uint16_t radii) {
  if (radii <= 1 || max_radius <= 0 || radius <= 0) return 0;
  if (radius >= max_radius) return radii - 1;
  return static_cast<uint16_t>(radius / max_radius * (radii - 1) + 0.5f);
}

// Caller normalises the angle to [0, 360); nearest sample wraps at the seam.
constexpr uint16_t angular_pixel(float angle, uint16_t angles) {
  if (!angles) return 0;
  return static_cast<uint32_t>(angle * angles / 360.0f + 0.5f) % angles;
}

// Row-major pixels: angle first, radius second. The previous frame remains
// untouched until all four diff modes have been decoded into the next frame.
template<class Pixel>
constexpr void decode_frame(const uint8_t* mask, const Pixel* diffs,
                            const Pixel* previous, Pixel* next,
                            size_t angles, size_t radii) {
  size_t diff = 0;
  for (size_t a = 0; a < angles; ++a) {
    for (size_t r = 0; r < radii; ++r) {
      const size_t pixel = a * radii + r;
      const uint8_t mode = (mask[pixel / 4] >> ((pixel % 4) * 2)) & 3;
      switch (mode) {
        case 0: next[pixel] = diffs[diff++]; break;
        case 1: next[pixel] = previous[pixel]; break;
        case 2: next[pixel] = r ? next[pixel - 1] : previous[pixel]; break;
        case 3: next[pixel] = a ? next[pixel - radii] : previous[pixel]; break;
      }
    }
  }
}
}  // namespace sabre_screen
