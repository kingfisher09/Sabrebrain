void paint_screen(float angle_in) {  // called by loop 0
  static int last_line = 0;
  int current_line = fmod(floor((angle_in + half_slice) / slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0
  bool array_changed = true;

  for (size_t i = 0; i < NUM_LEDS; i++) {
    if (image_pointer[current_line][i].red != image_pointer[last_line][i].red || image_pointer[current_line][i].blue != image_pointer[last_line][i].blue || image_pointer[current_line][i].green != image_pointer[last_line][i].green) {
      array_changed = true;
      break;
    }
  }

  last_line = current_line;
  if (array_changed) {  // only run if array has changed. This will mess with timing a bit so remove with new LED strip
    FastLED.clear();
    memcpy(leds, image_fast[current_line], sizeof(leds));  // write line of LEDs to the LED array
    FastLED.show();
  }
  bow_pos = 0; // means rainbow will always start at centre
}

void rainbow_line() {                          // called by loop 0 when not spinning
  static unsigned long lastRainbowUpdate = 0;  // variable to time rainbow
  static int dir = 1;

  unsigned long now = millis();

  if (now - lastRainbowUpdate >= rainbow_delay) {
    leds[bow_pos] = CHSV(bow_pos * hue_change, 255, 255);
    blur1d(leds, NUM_LEDS, 172);
    fadeToBlackBy(leds, NUM_LEDS, 16);
    FastLED.show();
    bow_pos = (bow_pos + dir);

    if (bow_pos > NUM_LEDS) {
      dir = -1;
    }
    if (bow_pos < 0) {
      dir = 1;
    }
    lastRainbowUpdate = now;
  }
}
