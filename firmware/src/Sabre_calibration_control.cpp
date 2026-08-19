#include "sabre_calibration_control.h"
#include "sabre_globals.h"
#include "sabre_storage.h"
#include "sabrescreen.h"

static bool calibration_mode = false;  // this is private, only exposed by the getter function

static bool previous_save_button = false;
static unsigned long save_button_press_start = 0;

static void commit_calibration();
static void update_trim();
static void adjust_calibration();

void handle_calibration_control() {
  bool just_pressed = calib_button && !previous_save_button;
  bool just_released = !calib_button && previous_save_button;

  if (just_pressed) {
    save_button_press_start = millis();
  }

  if (just_released) {
    unsigned long press_duration = millis() - save_button_press_start;

    bool long_press = press_duration >= 3000;
    if (!calibration_mode) {
      if (spin == 0 && long_press) {
        calibration_mode = true;
      }

      if (spin > 0) {
        update_trim();
      }
    } else {
      if (spin == 0) {
        calibration_mode = false;
      }
      if (spin > 0) {
        commit_calibration();
        calibration_mode = false;
      }
    }
  }

  if (calibration_mode) {
    adjust_calibration();
  }

  previous_save_button = calib_button;
}

static void adjust_calibration() {
  static unsigned long last_update_ms = millis();

  unsigned long now = millis();
  float dt = (now - last_update_ms) / 1000.0f;
  last_update_ms = now;

  global_calibration.accel_radius_m += slip * RADIUS_ADJUST_RATE_M_S * dt;
  global_calibration.heading_offset_deg += head * HEADING_ADJUST_RATE_DEG_S * dt;

  global_calibration.accel_radius_m = constrain(global_calibration.accel_radius_m, 0.005f, 0.300f);

  global_calibration.heading_offset_deg = constrain(global_calibration.heading_offset_deg, -180.0f, 180.0f);
}


static void commit_calibration() {
  global_calibration.accel_trim_point_count = 0;

  for (uint8_t i = 0; i < MAX_ACCEL_TRIM_POINTS; i++) {
    global_calibration.accel_trim_points[i] = {};
  }

  if (save_calibration(global_calibration)) {
    calibration_loaded = true;
    Serial.println("Calibration saved");
    flash_screen(CRGB::White);
  } else {
    Serial.println("Calibration save failed");
    flash_screen(CRGB::Red);
  }
}

void update_trim() {
  return;
}

bool calibration_mode_active() {
  return calibration_mode;
}
