// Software for melty brain robot written by Owen Fisher 2024-25
#include "sabre_globals.h"
#include "sabre_storage.h"
#include "sabre_calibration_control.h"
#include "sabrescreen.h"

// images here:
#include "../media/image_taunt.h"
#include "../media/sabre_cal.h"
#include "../media/dreadnought_logo.h"

// videos here:
#include "../media/sabremation.h"

LIS331 xl;  // accelerometer thing
int g_range = 100;
constexpr uint8_t ACCEL_ADDRESS = 0x19;
void check_g_range(float reading);

CRSFforArduino crsf = CRSFforArduino(&Serial1);

// functions
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// motors
BidirDShotX1* motor_Right;
BidirDShotX1* motor_Left;

// safety stuff
unsigned long stopflag_time = 0;
int E_stop_time = 100;          // ms allowed between ELRS signals before shutting down motors
int watchdog_time = 1000;       // ms after E_stop before resetting MCU
bool watchdog_enabled = false;  // bool to record watchdog status. Watchdog will be enabled when transmitter first sends data, MCU will restart 1s after estop if no more signals are received

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

int16_t xoff = 10;  // THESE NEED TO BE REMOVED AND PROPER ZEROING ADDED
int16_t yoff = 10;  // THESE NEED TO BE REMOVED
int16_t zoff = 0;
motorSpeeds motor_speeds;

// filter settings
float x = 0.3;
float a0 = 1 - x;
float b1 = x;
float prev_filt_val = 0;

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
  // Passthrough mode, keep this at the top of setup!
  if (passthrough_mode) {
    passthrough();
  }

  delay(1000);
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

  delay(500);  // experimental delay to allow time for ESC boot before we start sending packets

  // initialise motors
  motor_Right = new BidirDShotX1(MOTOR_RIGHT_PIN, 300);
  motor_Left = new BidirDShotX1(MOTOR_LEFT_PIN, 300);
  motor_Left->sendThrottle(0);
  motor_Right->sendThrottle(0);

  Serial.println("Thread 0 started");
  // send throttle 0 in loop till start delay has finished
  unsigned long start = millis();
  while (millis() - start < ESC_start_delay) {
    motor_Left->sendThrottle(0);
    motor_Right->sendThrottle(0);
    delayMicroseconds(200);
  }
}

void setup1() {
  // Passthrough mode, keep this at the top of setup!
  if (passthrough_mode) {
    // At some point add an LED indicator for this bit
    while (true) {
      delay(1000);
    }
  }
  Wire.begin();
  // Reset accelerometer
  pinMode(accel_pow, OUTPUT);
  digitalWrite(accel_pow, LOW);
  delay(1000);
  digitalWrite(accel_pow, HIGH);
  Serial.println("Accel has power");

  // Set up accel
  xl.setI2CAddr(ACCEL_ADDRESS);  // This MUST be called BEFORE .begin() so begin() can communicate with the chip
  bool accel_active = false;

  while (!accel_active) {
    xl.begin(LIS331::USE_I2C);
    xl.setFullScale(LIS331::LOW_RANGE);
    xl.setODR(LIS331::DR_1000HZ);
    delay(100);
    if (xl.newXData()) {
      accel_active = true;
    } else {
      delay(1000);
      Serial.println("Accel begin failed, trying again");
    }
  }

  // Calibration stuff
  while (!storage_setup()) {
    Serial.println("FATFS begin failed, trying again");
    delay(100);
  }
  calibration_loaded = load_calibration(global_calibration);

  if (!calibration_loaded) {
    Serial.println("Calibration load failed");
    global_calibration = SabreCalibration{};
  } else {
    Serial.println("Calibration loaded");
  }

  Serial.println("Thread 1 started");
}

void loop() {                    // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // These active variables needed ask masks when calibrating
  float active_slip = calibration_mode_active() ? 0.0f : slip;
  float active_head = calibration_mode_active() ? 0.0f : head;

  // angle calc
  zrot = zrotspd - (active_head * HEAD_CONTROL_SCALE) - head_trim;              // add in head for changing angle
  angle = fmod(angle + (zrot * (now - last_angle_time) / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop
  last_angle_time = micros();

  float left_sig, right_sig;
  bool spinning = spin > 0;

  // robot control modes
  if (spinning) {  // spinning mode

    float cosresult = cos(radians(angle));
    float sinresult = sin(radians(angle));
    float delta = (TRANS_SIGN * trans * cosresult) + (SLIP_SIGN * active_slip * sinresult);  // calculate motor delta

    // prevent over translating which flips motor direction
    float limit = spin * MAX_DELTA;
    delta = (delta > limit) ? limit : ((delta < -limit) ? -limit : delta);

    left_sig = spin + delta;
    right_sig = -spin + delta;

  } else {  // normal robot mode

    // normal driving with minimum motor speed
    left_sig = slip + trans;
    left_sig = (abs(left_sig) < min_drive) ? 0 : left_sig;
    right_sig = -slip + trans;
    right_sig = (abs(right_sig) < min_drive) ? 0 : right_sig;
  }

  update_screen(angle + global_calibration.heading_offset_deg, spinning, calibration_mode_active());
  motor_speeds = command_motors(left_sig * invert, right_sig * invert);
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # timing

  updateCRSF();  // update control
  handle_calibration_control();

  if (speed_source == SensorType::Accelerometer) {
    if (xl.newXData()) {
      int16_t x, y, z;
      xl.readAxes(x, y, z);
      x = x + xoff;
      y = y + yoff;
      float xg = xl.convertToG(g_range, x);
      float yg = xl.convertToG(g_range, y);
      float zg = xl.convertToG(g_range, z);

      float max_g = max(max(fabs(xg), fabs(yg)), fabs(zg));
      check_g_range(max_g);

      float measure_accel = 9.81 * sqrt(pow(xg, 2) + pow(yg, 2) + pow(zg, 2));  // given in m/s^2

      // FILTER ACCEL
      filtered_accel = (measure_accel * a0) + (prev_filt_val * b1);
      prev_filt_val = filtered_accel;

      float base_speed = degrees(sqrt(filtered_accel / global_calibration.accel_radius_m));

      float trim = 0;
      if (!calibration_mode_active()) {
        trim = get_trim_for_accel(filtered_accel);
      }

      zrotspd = base_speed + trim;  // deg/s
    }

  } else if (speed_source == SensorType::ERPM) {
    float average_ERPM = (motor_speeds.left + motor_speeds.right) / 2;  // this will need to change when I allow for single motors
    zrotspd = average_ERPM / (base_ERPM_cal);
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    Serial.println(global_calibration.heading_offset_deg);
    lastGpsUpdate = now;

    // Update the GPS telemetry data with the new values.

    // Telemetry depends on speed measurement mode
    float telem_calib = 0;
    if (speed_source == SensorType::Accelerometer) {
      telem_calib = global_calibration.accel_radius_m * 100;
    } else if (speed_source == SensorType::ERPM) {
      telem_calib = base_ERPM_cal;
    }
    crsf.telemetryWriteGPS(0, 0, zrotspd * 6000 / 360, 0, telem_calib, global_calibration.accel_trim_point_count);
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

static bool set_accel_range(LIS331::fs_range range) {

  uint8_t reg = xl.readReg(CTRL_REG4);

  reg &= ~0x30;          // Clear FS1 and FS0
  reg |= (range << 4);   // Set requested range

  // Write CTRL_REG4 directly over I2C
  Wire.beginTransmission(ACCEL_ADDRESS);
  Wire.write(CTRL_REG4);
  Wire.write(reg);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  // Read it back to make sure the sensor actually changed
  uint8_t check = xl.readReg(CTRL_REG4);

  return (check & 0x30) == (reg & 0x30);
}

void check_g_range(float reading) {
  // #AddLogging
  static int high_count = 0;
  static int low_count = 0;

  if (reading >= g_range * 0.9f) {
    high_count += 1;
    low_count = 0;
    if (high_count < 20) return;
    // increase range
    switch (g_range) {
      case 100:
        if (set_accel_range(LIS331::MED_RANGE)) {
          g_range = 200;
        }
        break;

      case 200:
        if (set_accel_range(LIS331::HIGH_RANGE)) {
          g_range = 400;
        }
        break;

      default:
        break;
    }

    high_count = 0;
    low_count = 0;

  } else if (reading <= g_range * 0.4f) {
    low_count += 1;
    high_count = 0;
    if (low_count < 20) return;
    // decrease range
    switch (g_range) {
      case 200:
        if (set_accel_range(LIS331::LOW_RANGE)) {
          g_range = 100;
        }
        break;

      case 400:
        if (set_accel_range(LIS331::MED_RANGE)) {
          g_range = 200;
        }
        break;

      default:
        break;
    }

    high_count = 0;
    low_count = 0;
  } else {
    // reset both counters
    high_count = 0;
    low_count = 0;
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