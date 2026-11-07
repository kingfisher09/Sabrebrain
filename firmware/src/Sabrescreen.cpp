// Renders dimension-aware media on core 0; core 1 queues selection/flash requests.
#include "sabrescreen.h"
#include "sabre_screen_frame.h"
#include <atomic>
#include <cmath>
#include <memory>
#include <new>
#include <pico/mutex.h>

namespace {
struct ScreenContent {
  uint16_t angles;
  uint16_t radii;
  std::unique_ptr<CRGB[]> current;
  std::unique_ptr<CRGB[]> next;  // Only videos need a second frame.
  std::unique_ptr<uint16_t[]> top_mapping;
  std::unique_ptr<uint16_t[]> bottom_mapping;
  const SabreVid* video = nullptr;
  uint16_t frame = 0;
  unsigned long frame_time = 0;
};

// Only core 0 touches active_content or the physical display buffers.
std::unique_ptr<ScreenContent> active_content;
std::unique_ptr<ScreenContent> pending_content;
auto_init_mutex(request_mutex);
bool flash_requested = false;
CRGB requested_flash_colour;

std::vector<CRGB> leds;
CLEDController* display_controller = nullptr;
int active_led_count = 0;
int hue_change = 0;
std::atomic<bool> screen_initialised{false};
int bow_pos = 0;
bool flashing = false;
CRGB flash_colour;
int flash_pos = 0;
unsigned long last_flash_update = 0;

void create_radial_mapping(const std::vector<LedPosition>& positions,
                           uint16_t* mapping, uint16_t radii) {
  float max_radius = 0;
  for (const auto& led : positions) max_radius = max(max_radius, led.radius_mm);
  for (size_t i = 0; i < positions.size(); ++i) {
    mapping[i] = sabre_screen::radial_pixel(positions[i].radius_mm, max_radius, radii);
  }
}

std::unique_ptr<ScreenContent> create_content(uint16_t angles, uint16_t radii, bool video) {
  if (!angles || !radii) {
    Serial.println("Screen media dimensions must be nonzero");
    return nullptr;
  }
  const size_t pixels = static_cast<size_t>(angles) * radii;
  if (pixels > static_cast<size_t>(PTRDIFF_MAX) / sizeof(CRGB)) {
    Serial.println("Screen media dimensions are too large");
    return nullptr;
  }
  std::unique_ptr<ScreenContent> content(new (std::nothrow) ScreenContent);
  if (content) {
    content->angles = angles;
    content->radii = radii;
    content->current.reset(new (std::nothrow) CRGB[pixels]);
    if (video) content->next.reset(new (std::nothrow) CRGB[pixels]);
    const size_t top_count = global_config.TOP_LED_POSITIONS.size();
    const size_t bottom_count = global_config.BOTTOM_LED_POSITIONS.size();
    if (top_count) content->top_mapping.reset(new (std::nothrow) uint16_t[top_count]);
    if (bottom_count) content->bottom_mapping.reset(new (std::nothrow) uint16_t[bottom_count]);
    if (content->current && (!video || content->next) &&
        (!top_count || content->top_mapping) && (!bottom_count || content->bottom_mapping)) {
      for (size_t i = 0; i < pixels; ++i) {
        content->current[i] = CRGB::Black;
        if (video) content->next[i] = CRGB::Black;
      }
      create_radial_mapping(global_config.TOP_LED_POSITIONS, content->top_mapping.get(), radii);
      create_radial_mapping(global_config.BOTTOM_LED_POSITIONS, content->bottom_mapping.get(), radii);
      return content;
    }
  }
  Serial.println("Not enough memory to select screen media; keeping previous selection");
  return nullptr;
}

void queue_content(std::unique_ptr<ScreenContent> content) {
  // Swap ownership only; allocation, decoding, and old-buffer destruction stay
  // outside the lock, so core 0 never waits for them while driving the robot.
  mutex_enter_blocking(&request_mutex);
  pending_content.swap(content);
  mutex_exit(&request_mutex);
}

void apply_requests() {
  std::unique_ptr<ScreenContent> replacement;
  if (!mutex_try_enter(&request_mutex, nullptr)) return;
  replacement.swap(pending_content);
  if (flash_requested) {
    flash_requested = false;
    flashing = true;
    flash_colour = requested_flash_colour;
    flash_pos = 0;
    last_flash_update = millis();
    fill_solid(leds.data(), active_led_count, CRGB::Black);
  }
  mutex_exit(&request_mutex);
  if (replacement) active_content.swap(replacement);
}

void paint_screen(float angle_in, bool inverted) {
  fill_solid(leds.data(), active_led_count, CRGB::Black);
  if (active_content) {
    if (global_config.flip_rot_direction) angle_in = -angle_in;
    const auto& positions = inverted ? global_config.BOTTOM_LED_POSITIONS
                                     : global_config.TOP_LED_POSITIONS;
    const uint16_t* mapping = inverted ? active_content->bottom_mapping.get()
                                      : active_content->top_mapping.get();
    for (size_t i = 0; i < positions.size(); ++i) {
      float angle = fmodf(angle_in + positions[i].angle_deg, 360.0f);
      if (angle < 0) angle += 360.0f;
      const size_t column = sabre_screen::angular_pixel(angle, active_content->angles);
      leds[i] = active_content->current[column * active_content->radii + mapping[i]];
    }
  }
  // Do not inherit the onboard NeoPixel's global FastLED refresh limit.
  display_controller->showLeds(FastLED.getBrightness());
  bow_pos = 0;
}

void rainbow_line(bool calibration_mode) {
  static unsigned long last_update = 0;
  static int direction = 1;
  const unsigned long now = millis();
  if (now - last_update < static_cast<unsigned long>(global_config.rainbow_delay)) return;
  leds[bow_pos] = calibration_mode ? CRGB(CRGB::Green) : CRGB(CHSV(bow_pos * hue_change, 255, 255));
  blur1d(leds.data(), active_led_count, 172);
  fadeToBlackBy(leds.data(), active_led_count, 16);
  display_controller->showLeds(FastLED.getBrightness());
  bow_pos = constrain(bow_pos + direction, 0, active_led_count - 1);
  if (bow_pos >= active_led_count - 1) direction = -1;
  if (bow_pos < 1) direction = 1;
  last_update = now;
}

void load_frame() {
  ScreenContent& content = *active_content;
  const unsigned long now = millis();
  if (now - content.frame_time < content.video->frameDurationMs) return;
  content.frame = (content.frame + 1) % content.video->numFrames;
  sabre_screen::decode_frame(content.video->getMask(content.frame), content.video->getDiff(content.frame),
                            content.current.get(), content.next.get(), content.angles, content.radii);
  content.current.swap(content.next);
  content.frame_time = now;
}

void update_flash() {
  if (millis() - last_flash_update < static_cast<unsigned long>(global_config.flash_delay)) return;
  leds[flash_pos] = flash_colour;
  blur1d(leds.data(), active_led_count, 172);
  fadeToBlackBy(leds.data(), active_led_count, 20);
  display_controller->showLeds(FastLED.getBrightness());
  ++flash_pos;
  last_flash_update = millis();
  if (flash_pos >= active_led_count) flashing = false;
}
}  // namespace

void screen_setup() {
  // These hardware buffers must not resize after the controller keeps their pointer.
  active_led_count = static_cast<int>(max(global_config.TOP_LED_POSITIONS.size(),
                                          global_config.BOTTOM_LED_POSITIONS.size()));
  if (!active_led_count) return;
  leds.resize(active_led_count);
  hue_change = 255 / active_led_count;
  pinMode(global_config.LED_POWER_PIN, OUTPUT);
  digitalWrite(global_config.LED_POWER_PIN, HIGH);
  display_controller = &FastLED.addLeds<APA102, Sabre_Config::headPin, Sabre_Config::headClock, BGR>(
      leds.data(), active_led_count);
  fill_solid(leds.data(), active_led_count, CRGB::Black);
  display_controller->showLeds(FastLED.getBrightness());
  screen_initialised = true;
}

void update_screen(float angle, bool spinning, bool calibration_mode, bool inverted) {
  if (!screen_initialised) return;
  apply_requests();
  if (flashing) {
    update_flash();
    return;
  }
  if (!spinning) {
    rainbow_line(calibration_mode);
    return;
  }
  if (active_content && active_content->video) load_frame();
  paint_screen(angle, inverted);
}

bool play_video(const SabreVid& video) {
  if (!video.numFrames || !video.frameDurationMs || !video.masks || !video.diffs ||
      video.maskBytesPerFrame != sabre_screen::mask_bytes(video.numAngles, video.numRadii) ||
      video.maskFrameCount < video.numFrames) {
    Serial.println("Invalid screen video metadata");
    return false;
  }
  auto content = create_content(video.numAngles, video.numRadii, true);
  if (!content) return false;
  content->video = &video;
  // Display frame zero immediately rather than leaving stale pixels until its timer expires.
  sabre_screen::decode_frame(video.getMask(0), video.getDiff(0), content->current.get(),
                            content->next.get(), video.numAngles, video.numRadii);
  content->current.swap(content->next);
  content->frame_time = millis();
  queue_content(std::move(content));
  return true;
}

bool show_still(const CRGB* image, uint16_t angles, uint16_t radii) {
  if (!image) return false;
  auto content = create_content(angles, radii, false);
  if (!content) return false;
  memcpy(content->current.get(), image, static_cast<size_t>(angles) * radii * sizeof(CRGB));
  queue_content(std::move(content));
  return true;
}

void flash_screen(CRGB colour) {
  if (!screen_initialised) return;
  mutex_enter_blocking(&request_mutex);
  requested_flash_colour = colour;
  flash_requested = true;
  mutex_exit(&request_mutex);
}
