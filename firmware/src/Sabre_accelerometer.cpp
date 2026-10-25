// Handles accelerometer power/startup timing, retries, range selection, and acceleration filtering.
#include "sabre_accelerometer.h"
#include "sabre_config.h"

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_LIS331.h>
#include <cmath>

static LIS331 xl;
static constexpr uint8_t ACCEL_ADDRESS = 0x19;
static int g_range = 100;

// Existing offsets retained until proper zero calibration is added.
static constexpr int16_t xoff = 10;
static constexpr int16_t yoff = 10;
static constexpr int16_t zoff = 0;
static constexpr float previous_sample_weight = 0.3f;
static float filtered_accel_history = 0;

enum class AccelState { Stopped, PowerOff, PowerSettling, WaitingForData, RetryWait, Ready };
static AccelState state = AccelState::Stopped;
static unsigned long step_started_ms = 0;
static constexpr unsigned long RESET_TIME_MS = 1000;
static constexpr unsigned long POWER_SETTLE_MS = 10;
static constexpr unsigned long DATA_SETTLE_MS = 100;
static constexpr unsigned long RETRY_TIME_MS = 1000;

static bool set_accel_range(LIS331::fs_range range);
static void check_g_range(float reading);
static void initialise_sensor();

void setup_accelerometer() {
  Wire.begin();
  pinMode(global_config.accel_pow, OUTPUT);
  digitalWrite(global_config.accel_pow, LOW);
  filtered_accel_history = 0;
  step_started_ms = millis();
  state = AccelState::PowerOff;
}

bool accelerometer_ready() {
  return state == AccelState::Ready;
}

bool update_accelerometer(float& filtered_acceleration) {
  const unsigned long elapsed = millis() - step_started_ms;
  switch (state) {
    case AccelState::Stopped:
      return false;
    case AccelState::PowerOff:
      if (elapsed >= RESET_TIME_MS) {
        digitalWrite(global_config.accel_pow, HIGH);
        Serial.println("Accel has power");
        step_started_ms = millis();
        state = AccelState::PowerSettling;
      }
      return false;
    case AccelState::PowerSettling:
      if (elapsed >= POWER_SETTLE_MS) initialise_sensor();
      return false;
    case AccelState::WaitingForData:
      if (elapsed < DATA_SETTLE_MS) return false;
      if (xl.newXData()) {
        state = AccelState::Ready;
        Serial.println("Accel ready");
      } else {
        Serial.println("Accel begin failed, trying again");
        step_started_ms = millis();
        state = AccelState::RetryWait;
        return false;
      }
      break;
    case AccelState::RetryWait:
      if (elapsed >= RETRY_TIME_MS) initialise_sensor();
      return false;
    case AccelState::Ready:
      if (!xl.newXData()) return false;
      break;
  }

  int16_t x, y, z;
  xl.readAxes(x, y, z);
  x += xoff;
  y += yoff;
  z += zoff;
  const float xg = xl.convertToG(g_range, x);
  const float yg = xl.convertToG(g_range, y);
  const float zg = xl.convertToG(g_range, z);
  check_g_range(max(max(fabs(xg), fabs(yg)), fabs(zg)));

  const float acceleration = 9.81f * sqrt(xg * xg + yg * yg + zg * zg); // returns m/s^2
  filtered_accel_history = acceleration * (1.0f - previous_sample_weight) + filtered_accel_history * previous_sample_weight;
  filtered_acceleration = filtered_accel_history;
  return true;
}

static void initialise_sensor() {
  // Avoid relying on the sensor library's data-status read when no device ACKs.
  Wire.beginTransmission(ACCEL_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("Accel not responding, trying again");
    state = AccelState::RetryWait;
  } else {
    xl.setI2CAddr(ACCEL_ADDRESS);  // Must precede begin().
    xl.begin(LIS331::USE_I2C);
    xl.setFullScale(LIS331::LOW_RANGE);
    xl.setODR(LIS331::DR_1000HZ);
    g_range = 100;
    state = AccelState::WaitingForData;
  }
  step_started_ms = millis();
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

static void check_g_range(float reading) {
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

