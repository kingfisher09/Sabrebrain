#include "sabrescreen.h"

enum class ScreenMode {
  Video,
  Still,
  Flash
};

static ScreenMode screen_mode = ScreenMode::Video;
static ScreenMode mode_before_flash = ScreenMode::Video;

const float slice_size = 360.0f / NUM_ANGLES;
const float half_slice = slice_size / 2.0f;
int bow_pos = 0;  // for keeping track of rainbow pixel

using Row = CRGB[NUM_SOURCE_RINGS];

// Create two full frames worth of LED data in memory - all black
Row bufferA[NUM_ANGLES] = {0};
Row bufferB[NUM_ANGLES] = {0};

// The * means we are creating pointers that point to addresses in memory rather than copying the contents. Later on this allows fast switching
Row* current_frame = bufferA;
Row* next_frame = bufferB;

const SabreVid* current_vid = nullptr;  // pointer to whichever video is active. Const because we never write to what the pointer is pointing at

int frame_duration;
int frame_num;
int num_frames;
unsigned long frame_time;

CRGB leds[MAX_PHYSICAL_LEDS];                        // array to hold LED colours
constexpr int hue_change = 255 / MAX_PHYSICAL_LEDS;  // integer division is fine here, round() not needed

// Flash state
static CRGB flash_colour = CRGB::White;
static int flash_pos = 0;
static unsigned long lastFlashUpdate = 0;

// Private functions
static void paint_screen(float angle_in, bool inverted);
static void rainbow_line(bool calibration_mode);
static void load_frame();
static void update_flash();

// LED MAPPING
static uint8_t top_radial_mapping[TOP_NUM_LEDS];
static uint8_t bottom_radial_mapping[BOTTOM_NUM_LEDS];

static void create_radial_mapping(const LedPosition* positions, uint8_t* mapping, int num_leds) {
  float max_radius = 0;

  for (int i = 0; i < num_leds; i++) {
    if (positions[i].radius_mm > max_radius) {
      max_radius = positions[i].radius_mm;
    }
  }

  for (int i = 0; i < num_leds; i++) {
    mapping[i] = static_cast<uint8_t>(round((positions[i].radius_mm / max_radius) * (NUM_SOURCE_RINGS - 1)));
  }
}

void screen_setup() {
  create_radial_mapping(TOP_LED_POSITIONS, top_radial_mapping, TOP_NUM_LEDS);
  create_radial_mapping(BOTTOM_LED_POSITIONS, bottom_radial_mapping, BOTTOM_NUM_LEDS);

  // Builtin LED first
  pinMode(LED_POWER_PIN, OUTPUT);  // Turn on LED power
  digitalWrite(LED_POWER_PIN, HIGH);

  FastLED.addLeds<APA102, headPin, headClock, BGR>(leds, 26);  // connect to LED strip
  FastLED.clear();                                             // ensure all LEDs start off
  FastLED.show();

  fill_solid(leds, MAX_PHYSICAL_LEDS, CRGB::White);
  FastLED.show();
  delay(3000);
}

void update_screen(float angle, bool spinning, bool calibration_mode, bool inverted) {
  if (screen_mode == ScreenMode::Flash) {
    update_flash();
    return;
  }

  if (!spinning) {
    rainbow_line(calibration_mode);
    return;
  }

  if (screen_mode == ScreenMode::Video) {
    load_frame();
  }

  paint_screen(angle, inverted);
}

void play_video(const SabreVid& video) {  // ampersand in arguments makes it a reference rather than copying the video
  current_vid = &video;
  frame_duration = video.frameDurationMs;
  frame_num = -1;
  frame_time = millis();
  num_frames = video.numFrames;

  screen_mode = ScreenMode::Video;
}

void show_still(const CRGB image[NUM_ANGLES][NUM_SOURCE_RINGS]) {
  memcpy(current_frame, image, sizeof(bufferA));
  screen_mode = ScreenMode::Still;
}

void flash_screen(CRGB colour) {
  if (screen_mode != ScreenMode::Flash) {
    mode_before_flash = screen_mode;
    FastLED.clear();
  }

  flash_colour = colour;
  flash_pos = 0;
  lastFlashUpdate = millis();
  screen_mode = ScreenMode::Flash;
}

static void paint_screen(float angle_in, bool inverted) {  // called by loop 0

  if (flip_rot_direction) {
    angle_in = -angle_in;
  }

  const LedPosition* positions;
  const uint8_t* radial_mapping;
  int num_leds;

  if (inverted) {
    positions = BOTTOM_LED_POSITIONS;
    radial_mapping = bottom_radial_mapping;
    num_leds = BOTTOM_NUM_LEDS;
  } else {
    positions = TOP_LED_POSITIONS;
    radial_mapping = top_radial_mapping;
    num_leds = TOP_NUM_LEDS;
  }

  FastLED.clear();

  for (int i = 0; i < num_leds; i++) {
    float led_angle = angle_in + positions[i].angle_deg;

    int current_line = fmod(
        floor((led_angle + half_slice) / slice_size),
        NUM_ANGLES);

    if (current_line < 0) {
      current_line += NUM_ANGLES;
    }

    leds[i] = current_frame[current_line][radial_mapping[i]];
  }

  FastLED.show();

  bow_pos = 0;  // means rainbow will always start at centre
}

static void rainbow_line(bool calibration_mode) {  // called by loop 0 when not spinning
  static unsigned long lastRainbowUpdate = 0;
  static int dir = 1;

  unsigned long now = millis();

  if (now - lastRainbowUpdate >= rainbow_delay) {
    if (calibration_mode) {
      leds[bow_pos] = CRGB::Green;
    } else {
      leds[bow_pos] = CHSV(bow_pos * hue_change, 255, 255);
    }

    blur1d(leds, MAX_PHYSICAL_LEDS, 172);
    fadeToBlackBy(leds, MAX_PHYSICAL_LEDS, 16);
    FastLED.show();

    bow_pos += dir;

    if (bow_pos > MAX_PHYSICAL_LEDS - 1) {
      dir = -1;
    }

    if (bow_pos < 1) {
      dir = 1;
    }

    lastRainbowUpdate = now;
  }
}

static void load_frame() {
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
      for (int r = 0; r < NUM_SOURCE_RINGS; r++) {
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

static void update_flash() {
  if (millis() - lastFlashUpdate >= flash_delay) {
    leds[flash_pos] = flash_colour;
    blur1d(leds, MAX_PHYSICAL_LEDS, 172);
    fadeToBlackBy(leds, MAX_PHYSICAL_LEDS, 20);
    FastLED.show();
    flash_pos += 1;
    lastFlashUpdate = millis();
  }

  if (flash_pos > MAX_PHYSICAL_LEDS - 1) {
    flash_pos = 0;
    screen_mode = mode_before_flash;
  }
}