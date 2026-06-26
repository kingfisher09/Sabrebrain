#include "sabre_storage.h"
#include <stdint.h>
#include <stddef.h>

static constexpr const char* CALIBRATION_PATH = "/SABREBRAIN/calibration.bin";


// using static here makes this function private to this cpp
static uint16_t calculate_crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

static uint16_t calibration_crc(SabreCalibration cal) {
  cal.crc = 0;
  return calculate_crc16(reinterpret_cast<const uint8_t*>(&cal), sizeof(SabreCalibration)
  );
}

bool validate_calibration(const SabreCalibration& cal) {
  if (cal.type != SABRE_TYPE_CALIBRATION) return false;
  if (cal.version != SABRE_CALIBRATION_VERSION) return false;
  if (cal.crc != calibration_crc(cal)) return false;

  if (cal.heading_offset_deg < -360.0f || cal.heading_offset_deg > 360.0f) return false;

  for (uint8_t i = 0; i < ACCEL_CAL_POINT_COUNT; i++) {
    if (cal.accel_cal_points[i].measured_accel < 0.0f) return false;
    if (cal.accel_cal_points[i].cal_spin_speed_deg_s < 0.0f) return false;
  }

  return true;
}

bool save_calibration(const SabreCalibration& cal) {
  SabreCalibration cal_to_save = cal;
  cal_to_save.type = SABRE_TYPE_CALIBRATION;
  cal_to_save.version = SABRE_CALIBRATION_VERSION;
  cal_to_save.crc = calibration_crc(cal_to_save);

  // TODO:
  // open CALIBRATION_PATH for write
  // write sizeof(SabreCalibration) bytes
  // close file
  // return true if write succeeded

  return false;
}

bool load_calibration(SabreCalibration& cal) {
  SabreCalibration loaded;

  // TODO:
  // open CALIBRATION_PATH for read
  // read sizeof(SabreCalibration) bytes into loaded
  // close file

  if (!validate_calibration(loaded)) {
    return false;
  }

  cal = loaded;
  return true;
}