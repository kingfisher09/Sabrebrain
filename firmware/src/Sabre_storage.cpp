#include "sabre_storage.h"

#include <FatFS.h>
#include <stdint.h>
#include <stddef.h>
#include <cmath>

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
  return calculate_crc16(reinterpret_cast<const uint8_t*>(&cal), sizeof(SabreCalibration));
}

bool validate_calibration(const SabreCalibration& cal) {
  if (cal.type != SABRE_TYPE_CALIBRATION) return false;
  if (cal.version != SABRE_CALIBRATION_VERSION) return false;
  if (cal.crc != calibration_crc(cal)) return false;

  if (cal.heading_offset_deg < -360.0f || cal.heading_offset_deg > 360.0f) return false;

  for (uint8_t i = 0; i < MAX_ACCEL_TRIM_POINTS; i++) {
    if (!isfinite(cal.accel_trim_points[i].measured_accel)) return false;
    if (!isfinite(cal.accel_trim_points[i].trim_spin_speed_deg_s)) return false;

    if (cal.accel_trim_points[i].measured_accel < 0.0f) return false;
    if (cal.accel_trim_points[i].trim_spin_speed_deg_s < 0.0f) return false;
  }

  return true;
}

bool storage_setup() {
  return FatFS.begin();
}

bool save_calibration(const SabreCalibration& cal) {
  SabreCalibration cal_to_save = cal;

  cal_to_save.type = SABRE_TYPE_CALIBRATION;
  cal_to_save.version = SABRE_CALIBRATION_VERSION;
  cal_to_save.crc = calibration_crc(cal_to_save);

  File file = FatFS.open(CALIBRATION_PATH, "w");

  if (!file) {
    return false;
  }

  size_t bytes_written = file.write(
    reinterpret_cast<const uint8_t*>(&cal_to_save),
    sizeof(cal_to_save)
  );

  file.close();

  return bytes_written == sizeof(cal_to_save);
}

bool load_calibration(SabreCalibration& cal) {
  File file = FatFS.open(CALIBRATION_PATH, "r");

  if (!file) {
    return false;
  }

  SabreCalibration loaded;

  size_t bytes_read = file.read(
    reinterpret_cast<uint8_t*>(&loaded),
    sizeof(loaded)
  );

  file.close();

  if (bytes_read != sizeof(loaded)) {
    return false;
  }

  if (!validate_calibration(loaded)) {
    return false;
  }

  cal = loaded;
  return true;
}