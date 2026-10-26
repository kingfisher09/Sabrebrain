// Converts validated JSON into robot settings, preserving defaults for omitted fields.

#include "sabre_config.h"
#include "sabre_config_validation.h"

#include <cstring>
#include <utility>
#include <ArduinoJson.h>
#include <string>

namespace {
void unpack_led_positions(JsonVariantConst array, std::vector<LedPosition>& positions) {
  if (array.isUnbound()) return;  // Keep this side's default layout.
  positions.clear();
  positions.reserve(array.size());
  for (JsonObjectConst led : array.as<JsonArrayConst>()) {
    positions.push_back({led["radius_mm"].as<float>(), led["angle_deg"].as<float>()});
  }
}
}  // namespace

bool config_from_json(const JsonDocument& document, Sabre_Config& config, std::string* error) {
  if (!validate_config(document, error)) return false;
  Sabre_Config loaded{};
  // type/schema_version were checked by validation; SCFG remains the struct's numeric tag.
  loaded.schema_version = document["schema_version"].as<uint16_t>();
  loaded.cell_count = document["cell_count"] | loaded.cell_count;
  if (!document["robot_name"].isUnbound()) {
    loaded.robot_name = document["robot_name"].as<std::string>();
  }

  const JsonVariantConst motors = document["motors"];
  auto& motor = loaded.motor_config;
  if (!motors["protocol"].isUnbound()) {
    const char* protocol = motors["protocol"].as<const char*>();
    if (std::strcmp(protocol, "dshot") == 0)
      motor.protocol = MotorProtocol::DShot;
    else if (std::strcmp(protocol, "crsf") == 0)
      motor.protocol = MotorProtocol::CRSF;
    else
      motor.protocol = MotorProtocol::OneShot;
  }

  const int count = motors["count"] | (motor.count == MotorCount::Single ? 1 : 2);
  motor.count = count == 1 ? MotorCount::Single : MotorCount::Dual;
  motor.startup_delay_ms = motors["startup_delay_ms"] | motor.startup_delay_ms;
  motor.pinA = motors["A"]["pin"] | motor.pinA;
  motor.pinB = motors["B"]["pin"] | motor.pinB;
  motor.left_direction = motors["A"]["direction"] | motor.left_direction;
  motor.right_direction = motors["B"]["direction"] | motor.right_direction;
  motor.crsf.channelA = motors["A"]["channel"] | motor.crsf.channelA;
  motor.crsf.channelB = motors["B"]["channel"] | motor.crsf.channelB;

  const JsonVariantConst selection = motors["selection"];
  motor.left_output = std::strcmp(selection["left"]["output"] | "A", "A") == 0
      ? MotorOutput::A : MotorOutput::B;
  motor.right_output = std::strcmp(selection["right"]["output"] | "B", "A") == 0
      ? MotorOutput::A : MotorOutput::B;
  motor.left_direction = selection["left"]["direction"] | motor.left_direction;
  motor.right_direction = selection["right"]["direction"] | motor.right_direction;

  const JsonVariantConst dshot = motors["dshot"];
  motor.dshot.bitrate_kbps = dshot["bitrate_kbps"] | motor.dshot.bitrate_kbps;
  motor.dshot.update_interval_us = dshot["update_interval_us"] | motor.dshot.update_interval_us;
  motor.dshot.desync_detection = dshot["desync_detection"] | motor.dshot.desync_detection;
  motor.dshot.desync_detect_time_ms = dshot["desync_detect_time_ms"] | motor.dshot.desync_detect_time_ms;

  const JsonVariantConst crsf = motors["crsf"];
  motor.crsf.tx_pin = crsf["tx_pin"] | motor.crsf.tx_pin;
  motor.crsf.baud_rate = crsf["baud_rate"] | motor.crsf.baud_rate;
  motor.crsf.update_interval_us = crsf["update_interval_us"] | motor.crsf.update_interval_us;

  const JsonVariantConst oneshot = motors["oneshot"];
  motor.oneshot.frequency_hz = oneshot["frequency_hz"] | motor.oneshot.frequency_hz;
  motor.oneshot.min_pulse_us = oneshot["min_pulse_us"] | motor.oneshot.min_pulse_us;
  motor.oneshot.max_pulse_us = oneshot["max_pulse_us"] | motor.oneshot.max_pulse_us;

  unpack_led_positions(document["leds"]["top"], loaded.TOP_LED_POSITIONS);
  unpack_led_positions(document["leds"]["bottom"], loaded.BOTTOM_LED_POSITIONS);

  // Publish only the complete result. Moving transfers vector/string storage
  // without making an additional copy of the layouts.
  config = std::move(loaded);
  return true;
}
