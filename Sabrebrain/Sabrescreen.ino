
void paint_screen(float angle_in){  // called by loop 0
  int current_line = fmod(floor((angle_in + half_slice)/slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0
  memcpy(leds, image_pointer[current_line], sizeof(leds)); // write line of LEDs to the LED array
  FastLED.show();
}
