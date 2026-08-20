#pragma once

#include <stdint.h>
#include "config/sabre_file_types.h"
#include "sabre_config.h"

#define SABRE_CALIBRATION_VERSION 1

constexpr uint8_t MAX_ACCEL_TRIM_POINTS = 4;

#pragma pack(push, 1)

struct AccelTrimPoint {
  float measured_accel;        // m/s²
  float trim_spin_speed_deg_s; // correction relative to base calibration
};

struct SabreCalibration {
  uint32_t type = SABRE_TYPE_CALIBRATION;
  uint16_t version = SABRE_CALIBRATION_VERSION;
  uint16_t crc = 0;

  // Main accelerometer calibration
  float accel_radius_m = approx_accel_rad / 1000.0f;

  // Graphics/movement phase alignment
  float heading_offset_deg = 0.0f;

  // Valid trim points are always stored in order of increasing acceleration
  uint8_t accel_trim_point_count = 0;
  AccelTrimPoint accel_trim_points[MAX_ACCEL_TRIM_POINTS] = {};
};

#pragma pack(pop)