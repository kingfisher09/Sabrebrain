void paint_screen(float angle_in) {  // called by loop 0
  if (flash_now) { return; }         // don't annimate while flashhing

  if (flip_rot_direction) { angle_in = 360 - angle_in; }

  static int last_line = 0;
  int current_line = fmod(floor((angle_in + half_slice) / slice_size), NUM_ANGLES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0

  last_line = current_line;
  FastLED.clear();
  memcpy(leds, current_frame[current_line], sizeof(leds));  // write line of LEDs to the LED array
  FastLED.show();

  bow_pos = 0;  // means rainbow will always start at centre
}

void rainbow_line() {         // called by loop 0 when not spinning
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

void load_vid(const SabreVid& video) {  // ampersand in arguments makes it a reference rather than copying the video
  current_vid = &video;
  frame_duration = video.frameDurationMs;
  frame_num = -1;
  frame_time = millis();
  num_frames = video.numFrames;
}

void load_frame() {
  unsigned long nowish = millis();
  if (nowish - frame_time >= frame_duration) {
    frame_num += 1;
    frame_num = frame_num % num_frames;  // loops the animation

    // --- create next frame from current frame ---
    const uint8_t* mask = current_vid->getMask(frame_num);
    const CRGB* diffs = current_vid->getDiff(frame_num);
    int diffIndex = 0;
    int bitPos = 0;
    uint8_t currentMaskByte = 0;
    int maskByteIndex = 0;

    // Decode mask bits (2 per pixel)
    for (int a = 0; a < NUM_ANGLES; a++) {
      for (int r = 0; r < NUM_LEDS; r++) {
        if (bitPos == 0)
          currentMaskByte = pgm_read_byte(&mask[maskByteIndex]);
        uint8_t mode = (currentMaskByte >> bitPos) & 0x03;  // extract 2 bits
        bitPos += 2;
        if (bitPos >= 8) {
          bitPos = 0;
          maskByteIndex++;
        }

        switch (mode) {
          case 0b00:  // new value
            next_frame[a][r] = diffs[diffIndex++];
            break;
          case 0b01:  // same as previous frame
            next_frame[a][r] = current_frame[a][r];
            break;
          case 0b10:  // same as previous pixel
            next_frame[a][r] = (r > 0) ? next_frame[a][r - 1] : current_frame[a][r];
            break;
          case 0b11:  // same as pixel in previous row
            next_frame[a][r] = (a > 0) ? next_frame[a - 1][r] : current_frame[a][r];
            break;
        }
      }
    }

    // --- swap the buffers ---
    std::swap(current_frame, next_frame);
    frame_time = nowish;  // keep at end of if statement
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
  if (flash_pos > NUM_LEDS - 1) {
    flash_pos = 0;
    flash_now = false;
  }
}

void flash() {                // thunder
  if (flash_now) { return; }  // don't start a new flash yet
  flash_now = true;
  FastLED.clear();
}
