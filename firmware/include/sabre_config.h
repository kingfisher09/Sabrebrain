// This file will hold the code to load and apply user and defualt settings. More variables will move here soon

#pragma once

// Modes
constexpr bool passthrough_mode = false;

// Movement settings
constexpr int RIGHT_MOTOR_DIRECTION = -1;        // #usersetting (-1 dread, 1 sabre)
constexpr int LEFT_MOTOR_DIRECTION = -1;        // #usersetting (-1 dread, 1 sabre)
constexpr float min_drive = 0.03;               // #advancedusersetting
constexpr float HEAD_CONTROL_SCALE = 330;      // #advancedusersetting
constexpr int TRANS_SIGN = -1;                  // #advancedusersetting
constexpr int SLIP_SIGN = -1;                   // #advancedusersetting
constexpr float MAX_DELTA = 0.3;                // #advancedusersetting - used to limit maximum delta in motor commands when translating
constexpr int default_rot_dir = 1;              // #usersetting 1 for cw, -1 ccw
constexpr bool desync_detection = true;         // #usersetting
constexpr int desync_detect_time = 0.2 * 1000;  // #advancedusersetting
constexpr float head_trim = 0;

// Screen settings
constexpr int NUM_ANGLES = 150;
constexpr int NUM_LEDS = 23;  // #usersetting
constexpr int MASK_BYTES_PER_FRAME = (NUM_ANGLES * NUM_LEDS * 2 + 7) / 8;

// Settings
constexpr int deadzone = 30;              // #advancedusersetting for transmitter sticks
constexpr int max_head = 360;             // #advancedusersetting max heading change in deg/s
constexpr int rainbow_delay = 40;         // #usersetting
constexpr int flash_delay = 12;           // #usersetting
constexpr int ESC_start_delay = 2000;     // #advancedusersetting
constexpr int approx_accel_rad = 84;      // #usersetting
constexpr int max_dshot_send_freq = 500;  // Hz, likely not a user setting
constexpr int dshot_delay = 1000000 / max_dshot_send_freq;

// ---- Calibration ----
constexpr float RADIUS_ADJUST_RATE_M_S = 0.008f;
constexpr float HEADING_ADJUST_RATE_DEG_S = 360.0f;
constexpr float TRIM_POINT_REPLACE_RANGE = 1.0f;

// Robot stuff
enum class SensorType  {
    Accelerometer,
    ERPM
};

constexpr SensorType speed_source = SensorType::Accelerometer;  // #usersetting
constexpr float base_ERPM_cal = 5;
constexpr bool flip_rot_direction = true;   // #usersetting - false for rotating with compass, true for against

// Pins
constexpr int MOTOR_RIGHT_PIN = 3; // (4 sabre, 3 dread)
constexpr int MOTOR_LEFT_PIN = 4;  // (3 sabre, 4 dread)
constexpr int LED_POWER_PIN = 11;  // builtin LED power control pin
constexpr int LED_PIN = 12;        // data pin for NeoPixel
constexpr int headPin = 27;        // LED heading data pin
constexpr int headClock = 28;      // LED clock pin
constexpr int accel_pow = 26;      // pin to power accelerometer, allows it to be restarted easily

// RF stuff
constexpr int SLIP_CH = 1;
constexpr int TRANS_CH = 2;
constexpr int SPIN_CH = 3;
constexpr int HEAD_CH = 4;
constexpr int INVERT_CH = 5;
constexpr int CALIB_CH = 6;
constexpr int IMAGE_CH = 7;
constexpr int EMOTE_CH = 8;  // used to trigger emote message


// LED layout
struct LedPosition {
  float radius_mm;
  float angle_deg;
};

constexpr LedPosition TOP_LED_POSITIONS[] = {
  {0.0, 0.0},
  {5.0, 0.0},
  {10.0, 0.0},
  {15.0, 0.0},
  {20.0, 0.0},
  {25.0, 0.0},
  {28.0, 70},
  {30.0, 75},
};

constexpr LedPosition BOTTOM_LED_POSITIONS[] = {
  {0.0, 0.0},
  {5.0, 0.0},
  {10.0, 0.0},
  {15.0, 0.0},
  {20.0, 0.0},
  {25.0, 0.0},
  {28.0, 20},
  {30.0, 22},
};

constexpr int TOP_NUM_LEDS = sizeof(TOP_LED_POSITIONS) / sizeof(TOP_LED_POSITIONS[0]);
constexpr int BOTTOM_NUM_LEDS = sizeof(BOTTOM_LED_POSITIONS) / sizeof(BOTTOM_LED_POSITIONS[0]);