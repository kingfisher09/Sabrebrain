// Configuration types and defaults. The active settings live in global_config.
#pragma once

#include <ArduinoJson.h>
#include "sabre_boot_mode.h"
#include <stdint.h>
#include <vector>
#include <string>
#include "config/sabre_file_types.h"

#define SABRE_CONFIG_VERSION 1

enum class SensorType { Accelerometer, ERPM };
enum class MotorProtocol { DShot, CRSF, OneShot };
enum class MotorCount { Single, Dual };

struct DShotMotorConfig {
  int bitrate_kbps = 300;
  unsigned long update_interval_us = 2000UL;
  bool desync_detection = true;  // #usersetting
  unsigned long desync_detect_time_ms = 200UL;  // #advancedusersetting
};

struct CrsfMotorConfig {
  int tx_pin = 4;  // Shared serial output; independent of per-motor PWM/DShot pins.
  int left_channel = 1;
  int right_channel = 2;
  unsigned long baud_rate = 420000UL;
  unsigned long update_interval_us = 1000UL;
};

struct OneShotMotorConfig {
  float frequency_hz = 3500.0f;
  float min_pulse_us = 125.0f;
  float max_pulse_us = 250.0f;
};

struct MotorConfig {
  MotorProtocol protocol = MotorProtocol::DShot;
  MotorCount count = MotorCount::Dual;
  int left_pin = 4;
  int right_pin = 3;
  int left_direction = 1;
  int right_direction = -1;
  unsigned long startup_delay_ms = 1000UL;
  DShotMotorConfig dshot{};
  CrsfMotorConfig crsf{};
  OneShotMotorConfig oneshot{};
};

struct LedPosition {
  float radius_mm;
  float angle_deg;
};

struct Sabre_Config {
  uint32_t type = SABRE_TYPE_CONFIG;
  uint16_t schema_version = SABRE_CONFIG_VERSION;
  std::string robot_name = "My robot";

  // Boot-time mode, selected before stored configuration can be loaded.
  static constexpr BootMode boot_mode = BootMode::Robot;

  // Movement settings
  float min_drive = 0.03;               // #advancedusersetting
  float HEAD_CONTROL_SCALE = 330;       // #advancedusersetting
  int TRANS_SIGN = -1;                  // #advancedusersetting
  int SLIP_SIGN = -1;                   // #advancedusersetting
  float MAX_DELTA = 0.3;                // #advancedusersetting - used to limit maximum delta in motor commands when translating
  int default_rot_dir = 1;              // #usersetting 1 for cw, -1 ccw
  float head_trim = 0;

  // Compiled image/video dimensions: these must remain compile-time constants.
  static constexpr int NUM_ANGLES = 150;
  static constexpr int NUM_SOURCE_RINGS = 23;  // #usersetting
  static constexpr int MASK_BYTES_PER_FRAME = (NUM_ANGLES * NUM_SOURCE_RINGS * 2 + 7) / 8;

  // Settings
  int deadzone = 30;              // #advancedusersetting for transmitter sticks
  int max_head = 360;             // #advancedusersetting max heading change in deg/s
  int rainbow_delay = 40;         // #usersetting
  int flash_delay = 12;           // #usersetting
  int approx_accel_rad = 84;      // #usersetting

  // ---- Calibration ----
  float RADIUS_ADJUST_RATE_M_S = 0.008f;
  float HEADING_ADJUST_RATE_DEG_S = 360.0f;
  float TRIM_POINT_REPLACE_RANGE = 1.0f;

  // Robot stuff
  SensorType speed_source = SensorType::Accelerometer;  // #usersetting
  float base_ERPM_cal = 5;
  bool flip_rot_direction = true;  // #usersetting - false for rotating with compass, true for against

  // Pins
  int LED_POWER_PIN = 11;   // builtin LED power control pin
  int LED_PIN = 12;         // data pin for NeoPixel
  // FastLED uses these pins as template arguments, so they remain compile-time.
  static constexpr int headPin = 27;         // LED heading data pin
  static constexpr int headClock = 28;       // LED clock pin
  int accel_pow = 26;       // pin to power accelerometer, allows it to be restarted easily

  // Batery
  int bat_pin = 2;
  float bat_read_ratio = 11.0;
  int cell_count = 1;

  // Safety timeouts in milliseconds.
  int E_stop_time = 100;
  int watchdog_time = 1000;

  MotorConfig motor_config{};

  // RF stuff
  int SLIP_CH = 1;
  int TRANS_CH = 2;
  int SPIN_CH = 3;
  int HEAD_CH = 4;
  int INVERT_CH = 5;
  int CALIB_CH = 6;
  int IMAGE_CH = 7;
  int EMOTE_CH = 8;  // used to trigger emote message

  // Physical LED counts and layouts are runtime configuration.
  std::vector<LedPosition> TOP_LED_POSITIONS = {
  };

  std::vector<LedPosition> BOTTOM_LED_POSITIONS = {
  };

};

extern Sabre_Config global_config;
// Loads supported settings from JSON; failure leaves the supplied config unchanged.
bool load_config(Sabre_Config& config);
bool config_from_json(const JsonDocument& document, Sabre_Config& config, std::string* error = nullptr);
