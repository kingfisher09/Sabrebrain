
void paint_screen(float angle_in) {                                                  // called by loop 0
  int current_line = fmod(floor((angle_in + half_slice) / slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0
  memcpy(leds, image_pointer[current_line], sizeof(leds));                           // write line of LEDs to the LED array

  if (test_mode) {
    Serial.print("current line: ");
    Serial.println(current_line);
  }
  // FastLED.clear();
  FastLED.show();
}

void rainbow_line() {                          // called by loop 0 when not spinning
  static unsigned long lastRainbowUpdate = 0;  // variable to time rainbow
  static int bow_pos = 0;                      // for keeping track of rainbow pixel
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
