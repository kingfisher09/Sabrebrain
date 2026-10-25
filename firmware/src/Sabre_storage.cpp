// Mounts FatFS, loads JSON configuration, and stores calibration with a CRC integrity check.
#include "sabre_storage.h"
#include "sabre_config.h"

#include <FatFS.h>
#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include <ArduinoJson.h>

static constexpr const char* CALIBRATION_PATH = "/SABREBRAIN/calibration.bin";
static constexpr const char* CONFIG_DIRECTORY = "/SABREBRAIN/config";
static constexpr const char* ERROR_PATH = "/SABREBRAIN/errors.txt";

// Save the latest load failure before USB storage hands the filesystem to the PC.
static bool config_error(const String& message) {
  Serial.println(message);
  if (!FatFS.exists("/SABREBRAIN") && !FatFS.mkdir("/SABREBRAIN")) {
    Serial.println("Could not create error report folder");
    return false;
  }
  File report = FatFS.open(ERROR_PATH, "w");
  if (!report) {
    Serial.println("Could not open errors.txt for writing");
    return false;
  }
  const String contents = "Configuration load failed\r\n" + message +
      "\r\nFix the configuration, eject the USB drive, then power cycle to retry.\r\n";
  const size_t written = report.print(contents);
  report.close();
  if (written != contents.length()) Serial.println("Error report write incomplete");
  return false;
}

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

  if (cal.accel_trim_point_count > MAX_ACCEL_TRIM_POINTS) return false;

  for (uint8_t i = 0; i < MAX_ACCEL_TRIM_POINTS; i++) {
    if (!isfinite(cal.accel_trim_points[i].measured_accel)) return false;
    if (!isfinite(cal.accel_trim_points[i].trim_spin_speed_deg_s)) return false;

    if (cal.accel_trim_points[i].measured_accel < 0.0f) return false;
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
      sizeof(cal_to_save));

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
      sizeof(loaded));

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

bool load_config(Sabre_Config& config) {
  String config_name;
  Dir directory = FatFS.openDir(CONFIG_DIRECTORY);
  while (directory.next()) {
    if (!directory.isFile()) continue;
    String name = directory.fileName();
    String lowercase_name = name;
    lowercase_name.toLowerCase();
    if (!lowercase_name.endsWith(".json")) continue;
    if (config_name.length()) {
      return config_error("Multiple JSON files in /SABREBRAIN/config: " + config_name +
                          ", " + name + ". Keep exactly one JSON file in this folder.");
    }
    config_name = name;
  }
  if (!config_name.length()) {
    return config_error("No JSON file found in /SABREBRAIN/config");
  }

  const String config_path = String(CONFIG_DIRECTORY) + "/" + config_name;
  File file = FatFS.open(config_path, "r");

  if (!file) {
    return config_error("Could not open config file: " + config_path);
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(document, file);
  file.close();

  if (error) {
    return config_error(config_path + ": JSON error: " + error.c_str());
  }

  std::string validation_error;
  if (!config_from_json(document, config, &validation_error)) {
    return config_error(config_path + ": validation error: " + validation_error.c_str());
  }

  // A successful load resolves the previous configuration error.
  if (FatFS.exists(ERROR_PATH) && !FatFS.remove(ERROR_PATH)) {
    Serial.println("Could not remove old errors.txt");
  }
  Serial.print("Loaded config: ");
  Serial.println(config_name);
  return true;
}
