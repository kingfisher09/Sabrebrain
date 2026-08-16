#pragma once
#include "sabre_calibration_control.h"
#include <Arduino.h>

static bool calibration_mode = false;  // this is private, only exposed by the getter function

static bool previous_save_button = false;
static unsigned long save_button_press_start = 0;

static void commit_calibration();
static void update_trim(float head);

// Temporary working values while calibrating
static float working_base_calibration = 0.0f;
static float working_heading_offset_deg = 0.0f;

void handle_calibration_control(bool save_button, float spin, float slip, float head) {
  bool just_pressed = save_button && !previous_save_button;
  bool just_released = !save_button && previous_save_button;

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
        update_trim(head);
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
    // adjust values according to slip and head
  }
  previous_save_button = save_button;
}

bool calibration_mode_active() {
  return calibration_mode;
}