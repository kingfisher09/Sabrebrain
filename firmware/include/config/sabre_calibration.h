#pragma once

#include <stdint.h>
#include "config/sabre_file_types.h"

#define SABRE_CALIBRATION_VERSION 1

constexpr uint8_t ACCEL_CAL_POINT_COUNT = 4;

struct AccelCalPoint {
  float measured_accel;        // g
  float cal_spin_speed_deg_s;  // deg/s
};

struct SabreCalibration {
  uint32_t type = SABRE_TYPE_CALIBRATION;
  uint16_t version = SABRE_CALIBRATION_VERSION;
  uint16_t crc = 0;

  // Graphics/movement phase alignment
  float heading_offset_deg = 0.0f;

  // Accel calibration lookup table
  AccelCalPoint accel_cal_points[ACCEL_CAL_POINT_COUNT] = {};
};
#pragma pack(pop)