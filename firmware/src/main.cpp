// Software for melty brain robot written by Owen Fisher 2024-25-26

// Initialises both cores and runs movement mixing, rotation estimation, and display/telemetry updates.
#include "sabre_globals.h"
#include "sabre_motor_control.h"
#include "sabre_accelerometer.h"
#include "sabre_storage.h"
#include "sabre_calibration_control.h"
#include "sabrescreen.h"
#include "sabre_config.h"
#include "sabre_usb_storage.h"
#include "sabre_measure_battery.h"

// images here:
#include "../media/image_taunt.h"
#include "../media/sabre_cal.h"
#include "../media/dreadnought_logo.h"

// videos here:
#include "../media/sabremation.h"

CRSFforArduino crsf = CRSFforArduino(&Serial1);

// functions
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// safety stuff
unsigned long stopflag_time = 0;

Sabre_Config global_config;
SabreCalibration global_calibration;

bool calibration_loaded = false;

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
bool headMode = false;  // remove this ASAP
int image_mode;
bool emote;
float invert = 1;

// rotation tracking
float angle = 0;                    // current robot angle
unsigned long last_angle_time = 0;  // program time when last angle was calculated
float zrotspd = 0;                  // measured speed
float zrot = 0;                     // measured speed with heading control injected
float filtered_accel = 0;           // used for trimming

motorSpeeds motor_speeds;

enum class DisplaySlot {
  None,
  Low,
  Mid,
  High,
  Emote,
  Calibration
};

DisplaySlot active_display_slot = DisplaySlot::None;

// This is where displays are selected
static void select_display(DisplaySlot slot) {
  switch (slot) {
    case DisplaySlot::Low:
      show_still(dreadnought_logo);
      break;

    case DisplaySlot::Mid:
      show_still(dreadnought_logo);
      break;

    case DisplaySlot::High:
      play_video(sabremation);
      break;

    case DisplaySlot::Emote:
      show_still(image_taunt);
      break;

    case DisplaySlot::Calibration:
      show_still(sabre_cal);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!storage_setup()) {
    Serial.println("FATFS begin failed, trying again");
    delay(100);
  }
  load_config_and_select_boot_mode();
  if (boot_mode() == BootMode::USBStorage) {
    if (!setup_usb_storage()) Serial.println("USB drive startup failed");
    while (true) delay(1);
  }
  if (boot_mode() == BootMode::MotorSelection) {
    Serial.println("Motor selection is not implemented yet; motor outputs disabled");
    while (true) delay(1);
  }
  // Passthrough mode, keep this at the top of setup!
  if (boot_mode() == BootMode::Passthrough) {
    passthrough();
  }

  Serial.println("Thread 0 starting...");
  // Initialise CRSF for Arduino.
  if (!crsf.begin()) {
    Serial.println("CRSF for Arduino initialisation failed!");
    while (1) {
      delay(10);
    }
  }

  /* Set your link statistics callback. */
  crsf.setLinkStatisticsCallback(onLinkStatisticsUpdate);

  screen_setup();

  setup_motors(global_config.motor_config);
  Serial.println("Thread 0 started");
}

void setup1() {
  while (!boot_mode_selected()) delay(1);
  if (boot_mode() != BootMode::Robot) {
    // Core 0 handles special modes; the onboard NeoPixel shows the selection.
    while (true) delay(1);
  }
  
  // Core 0 has already mounted FatFS and loaded a valid robot configuration.
  setup_accelerometer();  // Start the reset timer using the configured power pin.

  calibration_loaded = load_calibration(global_calibration);
  
  if (!calibration_loaded) {
    Serial.println("Calibration load failed");
    global_calibration = SabreCalibration{};
  } else {
    Serial.println("Calibration loaded");
  }
  
  settup_measure_bat(global_config);
  Serial.println("Thread 1 started");
}

void loop() { // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // These active variables needed ask masks when calibrating
  float active_slip = calibration_mode_active() ? 0.0f : slip;
  float active_head = calibration_mode_active() ? 0.0f : head;

  // angle calc
  zrot = zrotspd - (active_head * global_config.HEAD_CONTROL_SCALE) - global_config.head_trim;              // add in head for changing angle
  angle = fmod(angle + (zrot * (now - last_angle_time) / 1000000) + 360, 360);  // will not work if rotate more than 360Â° negative per loop
  last_angle_time = micros();

  float left_sig, right_sig;
  bool spinning = spin > 0;

  // robot control modes
  if (spinning) {  // spinning mode

    float cosresult = cos(radians(angle));
    float sinresult = sin(radians(angle));
    float delta = (global_config.TRANS_SIGN * trans * cosresult) + (global_config.SLIP_SIGN * active_slip * sinresult);  // calculate motor delta

    // prevent over translating which flips motor direction
    float limit = spin * global_config.MAX_DELTA;
    delta = (delta > limit) ? limit : ((delta < -limit) ? -limit : delta);

    left_sig = spin + delta;
    right_sig = -spin + delta;

  } else {  // normal robot mode

    // normal driving with minimum motor speed
    left_sig = slip + trans;
    left_sig = (abs(left_sig) < global_config.min_drive) ? 0 : left_sig;
    right_sig = -slip + trans;
    right_sig = (abs(right_sig) < global_config.min_drive) ? 0 : right_sig;
  }

  update_screen(angle + global_calibration.heading_offset_deg, spinning, calibration_mode_active(), invert == 1);
  motor_speeds = command_motors(left_sig * invert, right_sig * invert);
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
  serial_update_boot_mode();  // Core 1 owns serial commands and runtime filesystem writes.
  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # tiÃºming

  const bool new_accel_sample = update_accelerometer(filtered_accel);
  // Preserve the startup interlock: no radio motor commands before sensor readiness.
  if (!accelerometer_ready()) return;

  updateCRSF();  // update control
  handle_calibration_control();

  if (global_config.speed_source == SensorType::Accelerometer) {
    if (new_accel_sample) {
      float base_speed = degrees(sqrt(filtered_accel / global_calibration.accel_radius_m));

      float trim = 0;
      if (!calibration_mode_active()) {
        trim = get_trim_for_accel(filtered_accel);
      }

      zrotspd = base_speed + trim;  // deg/s
    }

  } else if (global_config.speed_source == SensorType::ERPM) {
    float average_ERPM = global_config.motor_config.count == MotorCount::Single
                             ? motor_speeds.left
                             : (motor_speeds.left + motor_speeds.right) / 2;
    zrotspd = average_ERPM / (global_config.base_ERPM_cal);
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    lastGpsUpdate = now;
    float bat_volt = measure_bat();

    // Update the GPS telemetry data with the new values.

    // Telemetry depends on speed measurement mode
    float telem_calib = 0;
    if (global_config.speed_source == SensorType::Accelerometer) {
      telem_calib = global_calibration.accel_radius_m * 100;
    } else if (global_config.speed_source == SensorType::ERPM) {
      telem_calib = global_config.base_ERPM_cal;
    }
    crsf.telemetryWriteGPS(0, 0, zrotspd * 6000 / 360, 0, telem_calib, global_calibration.accel_trim_point_count);
    crsf.telemetryWriteBattery(bat_volt * 100.0f, 0, 0, 0);
  }

  DisplaySlot current_display;

  if (calibration_mode_active()) {
    current_display = DisplaySlot::Calibration;

  } else if (emote) {
    current_display = DisplaySlot::Emote;

  } else if (image_mode < 1250) {
    current_display = DisplaySlot::Low;

  } else if (image_mode < 1750) {
    current_display = DisplaySlot::Mid;

  } else {
    current_display = DisplaySlot::High;
  }

  if (current_display != active_display_slot) {
    select_display(current_display);
    active_display_slot = current_display;
  }
}

float get_trim_for_accel(float accel) {
  uint8_t count = global_calibration.accel_trim_point_count;

  if (count == 0) {
    return 0.0f;
  }

  // Below the lowest point: use the lowest trim
  if (accel <= global_calibration.accel_trim_points[0].measured_accel) {
    return global_calibration.accel_trim_points[0].trim_spin_speed_deg_s;
  }

  // Above the highest point: use the highest trim
  if (accel >= global_calibration.accel_trim_points[count - 1].measured_accel) {
    return global_calibration.accel_trim_points[count - 1].trim_spin_speed_deg_s;
  }

  // Find the two points either side of the current acceleration
  for (uint8_t i = 0; i < count - 1; i++) {
    const AccelTrimPoint& lower = global_calibration.accel_trim_points[i];
    const AccelTrimPoint& upper = global_calibration.accel_trim_points[i + 1];

    if (accel >= lower.measured_accel &&
        accel <= upper.measured_accel) {
      float fraction =
          (accel - lower.measured_accel) /
          (upper.measured_accel - lower.measured_accel);

      return lower.trim_spin_speed_deg_s +
             fraction *
                 (upper.trim_spin_speed_deg_s - lower.trim_spin_speed_deg_s);
    }
  }

  return 0.0f;  // should never get here
}
