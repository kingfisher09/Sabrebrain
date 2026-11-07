#pragma once

#include <FastLED.h>
#include "sabre_config.h"
#include "sabre_vid.h"

void screen_setup();

// Media and video metadata must remain alive while selected. False leaves the
// previous selection unchanged (invalid dimensions or insufficient memory).
bool play_video(const SabreVid& video);
bool show_still(const CRGB* image, uint16_t angles, uint16_t radii);

// Existing hardcoded arrays supply their dimensions automatically.
template<size_t Angles, size_t Radii>
bool show_still(const CRGB (&image)[Angles][Radii]) {
  static_assert(Angles > 0 && Angles <= UINT16_MAX && Radii > 0 && Radii <= UINT16_MAX,
                "Screen dimensions must fit uint16_t");
  return show_still(&image[0][0], Angles, Radii);
}
void flash_screen(CRGB colour);
void update_screen(float angle, bool spinning, bool calibration_mode, bool inverted);
