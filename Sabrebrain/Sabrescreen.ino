void paint_screen(float angle_in) {  // called by loop 0
  if (flash_now) { return; }          // don't annimate while flashhing

  if (flip_rot_direction) {angle_in = 360 - angle_in;}

  static int last_line = 0;
  int current_line = fmod(floor((angle_in + half_slice) / slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0

  last_line = current_line;
  FastLED.clear();
  memcpy(leds, current_image[current_line], sizeof(leds));  // write line of LEDs to the LED array
  FastLED.show();

  bow_pos = 0;  // means rainbow will always start at centre
}

void rainbow_line() {        // called by loop 0 when not spinning
  if (flash_now) { return; }  // don't annimate while flashhing

  static unsigned long lastRainbowUpdate = 0;  // variable to time rainbow
  static int dir = 1;

  unsigned long now = millis();

  if (now - lastRainbowUpdate >= rainbow_delay) {
    leds[bow_pos] = CHSV(bow_pos * hue_change, 255, 255);
    blur1d(leds, NUM_LEDS, 172);
    fadeToBlackBy(leds, NUM_LEDS, 16);
    FastLED.show();
    bow_pos = (bow_pos + dir);

    if (bow_pos > NUM_LEDS - 1) {
      dir = -1;
    }
    if (bow_pos < 1) {
      dir = 1;
    }
    lastRainbowUpdate = now;
  }
}


void flashing() {
  static unsigned long lastFlashUpdate = 0;  // variable to time rainbow
  static int flash_pos = 0;

  if (millis() - lastFlashUpdate >= flash_delay) {
    leds[flash_pos] = CRGB::White;
    fadeToBlackBy(leds, NUM_LEDS, 85);
    FastLED.show();
    flash_pos += 1;
    lastFlashUpdate = millis();
  }
  if (flash_pos > NUM_LEDS -1) {
    flash_pos = 0;
    flash_now = false;
  }
}

void flash() {  // thunder
  if (flash_now) { return; }  // don't start a new flash yet
  flash_now = true;
  FastLED.clear();
}
