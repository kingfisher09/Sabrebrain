#pragma once

#include <FastLED.h>
#include "sabre_config.h"
#include "sabre_vid.h"

void screen_setup();

void play_video(const SabreVid& video);
void show_still(const CRGB image[NUM_ANGLES][NUM_LEDS]);
void flash_screen(CRGB colour);
void update_screen(float angle, bool spinning, bool calibration_mode);