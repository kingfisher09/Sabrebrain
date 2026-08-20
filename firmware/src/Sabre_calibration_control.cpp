#include "sabre_calibration_control.h"
#include "sabre_globals.h"
#include "sabre_storage.h"
#include "sabrescreen.h"

static bool calibration_mode = false;  // this is private, only exposed by the getter function
static SabreCalibration calibration_backup;

static bool previous_save_button = false;
static bool ignore_next_release = false;
static unsigned long save_button_press_start = 0;
static unsigned long last_calibration_update_ms = 0;

static void enter_calibration();
static void cancel_calibration();
static bool commit_calibration();
static void update_trim();
static void sort_trim_points();
static void adjust_calibration();
static bool calibration_save_pending = false;

void handle_calibration_control() {
  bool just_pressed = calib_button && !previous_save_button;
  bool just_released = !calib_button && previous_save_button;

  if (calibration_save_pending && spin == 0) {
    if (save_calibration(global_calibration)) {
      flash_screen(CRGB::Green);
    } else {
      flash_screen(CRGB::Red);
    }
    calibration_save_pending = false;
  }

  if (just_pressed) {
    save_button_press_start = millis();
  }

  // Enter calibration mode immediately after holding the button for 3 seconds while stationary
  if (!calibration_mode && calib_button && spin == 0) {
    unsigned long press_duration = millis() - save_button_press_start;

    if (press_duration >= 3000) {
      enter_calibration();
      ignore_next_release = true;
    }
  }

  if (just_released) {
    if (ignore_next_release) {
      ignore_next_release = false;
    } else {
      if (!calibration_mode) {
        if (spin > 0) {
          update_trim();
        }
      } else {
        if (spin == 0) {
          cancel_calibration();
        }

        if (spin > 0) {
          if (commit_calibration()) {
            calibration_mode = false;
          }
        }
      }
    }
  }

  if (calibration_mode) {
    if (spin > 0) {
      adjust_calibration();
    } else {
      last_calibration_update_ms = millis();
    }
  }

  previous_save_button = calib_button;
}

static void enter_calibration() {
  calibration_backup = global_calibration;
  last_calibration_update_ms = millis();
  calibration_mode = true;

  Serial.println("Calibration mode entered");  // remove after debug
}

static void cancel_calibration() {
  global_calibration = calibration_backup;
  calibration_mode = false;

  Serial.println("Calibration reverted");  // remove after debug
}

static void adjust_calibration() {
  unsigned long now = millis();
  float dt = (now - last_calibration_update_ms) / 1000.0f;
  last_calibration_update_ms = now;

  global_calibration.accel_radius_m += head * RADIUS_ADJUST_RATE_M_S * dt;
  global_calibration.heading_offset_deg -= slip * HEADING_ADJUST_RATE_DEG_S * dt;

  global_calibration.accel_radius_m = constrain(global_calibration.accel_radius_m, 0.005f, 0.500f);

  if (global_calibration.heading_offset_deg > 180.0f) {
    global_calibration.heading_offset_deg -= 360.0f;
  } else if (global_calibration.heading_offset_deg < -180.0f) {
    global_calibration.heading_offset_deg += 360.0f;
  }
}

static bool commit_calibration() {
  global_calibration.accel_trim_point_count = 0;

  for (uint8_t i = 0; i < MAX_ACCEL_TRIM_POINTS; i++) {
    global_calibration.accel_trim_points[i] = {};
  }

  if (save_calibration(global_calibration)) {
    calibration_loaded = true;
    Serial.println("Calibration saved");
    flash_screen(CRGB::White);
    return true;
  } else {
    Serial.println("Calibration save failed");
    flash_screen(CRGB::Red);
    return false;
  }
}

static void update_trim() {
  AccelTrimPoint new_point;

  float current_trim = get_trim_for_accel(filtered_accel);

  new_point.measured_accel = filtered_accel;
  new_point.trim_spin_speed_deg_s = current_trim - (head * HEAD_CONTROL_SCALE);

  bool point_replaced = false;

  // If a point already exists within 1 m/s², overwrite it
  for (uint8_t i = 0; i < global_calibration.accel_trim_point_count; i++) {
    float distance =
        fabs(global_calibration.accel_trim_points[i].measured_accel - filtered_accel);

    if (distance <= TRIM_POINT_REPLACE_RANGE) {
      global_calibration.accel_trim_points[i] = new_point;
      point_replaced = true;
      break;
    }
  }

  if (!point_replaced) {
    // If there's still space, add a new point
    if (global_calibration.accel_trim_point_count < MAX_ACCEL_TRIM_POINTS) {
      uint8_t index = global_calibration.accel_trim_point_count;

      global_calibration.accel_trim_points[index] = new_point;
      global_calibration.accel_trim_point_count++;
    }

    // Otherwise overwrite whichever existing point is closest in acceleration
    else {
      uint8_t closest_index = 0;
      float closest_distance = INFINITY;

      for (uint8_t i = 0; i < MAX_ACCEL_TRIM_POINTS; i++) {
        float distance =
            fabs(global_calibration.accel_trim_points[i].measured_accel - filtered_accel);

        if (distance < closest_distance) {
          closest_distance = distance;
          closest_index = i;
        }
      }

      global_calibration.accel_trim_points[closest_index] = new_point;
    }
  }

  sort_trim_points();

  calibration_save_pending = true;
  flash_screen(CRGB::White);
}

static void sort_trim_points() {
  for (uint8_t i = 1; i < global_calibration.accel_trim_point_count; i++) {
    AccelTrimPoint point_to_move = global_calibration.accel_trim_points[i];
    int j = i - 1;

    while (j >= 0 &&
           global_calibration.accel_trim_points[j].measured_accel > point_to_move.measured_accel) {
      global_calibration.accel_trim_points[j + 1] = global_calibration.accel_trim_points[j];

      j--;
    }

    global_calibration.accel_trim_points[j + 1] = point_to_move;
  }
}

bool calibration_mode_active() {
  return calibration_mode;
}