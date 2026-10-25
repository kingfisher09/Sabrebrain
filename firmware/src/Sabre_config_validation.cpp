// Checks configuration JSON field types, motor constraints, and variable LED layouts.
#include "sabre_config_validation.h"
#include "sabre_config.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace {
bool fail(std::string* error, const std::string& path, const std::string& reason) {
  if (error) *error = path + ": " + reason;
  return false;
}

bool optional_object(JsonVariantConst value, const char* path, std::string* error) {
  return value.isUnbound() || value.is<JsonObjectConst>() ||
         fail(error, path, "must be an object");
}

bool integer(JsonVariantConst value, const char* path, long minimum, long maximum,
             std::string* error) {
  if (value.isUnbound()) return true;
  if (!value.is<long>() || value.as<long>() < minimum || value.as<long>() > maximum)
    return fail(error, path, "must be an integer from " + std::to_string(minimum) +
                            " to " + std::to_string(maximum));
  return true;
}

bool number(JsonVariantConst value, const std::string& path, double minimum,
            double maximum, std::string* error) {
  if (value.isUnbound()) return true;
  if (!value.is<double>() || !std::isfinite(value.as<double>()) ||
      value.as<double>() < minimum || value.as<double>() > maximum)
    return fail(error, path, "must be a finite number within the supported range");
  return true;
}

bool motor(JsonVariantConst value, const char* path, std::string* error) {
  if (!optional_object(value, path, error)) return false;
  const std::string prefix = std::string(path) + ".";
  if (!integer(value["pin"], (prefix + "pin").c_str(), 0, 29, error) ||
      !integer(value["channel"], (prefix + "channel").c_str(), 1, 16, error) ||
      !integer(value["direction"], (prefix + "direction").c_str(), -1, 1, error)) return false;
  if (!value["direction"].isUnbound() && value["direction"].as<int>() == 0)
    return fail(error, prefix + "direction", "must be 1 or -1");
  return true;
}

bool layout(JsonVariantConst value, const char* path, std::string* error) {
  if (value.isUnbound()) return true;
  if (!value.is<JsonArrayConst>()) return fail(error, path, "must be an array");
  if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    return fail(error, path, "has too many entries for the LED count");
  size_t index = 0;
  for (JsonVariantConst position : value.as<JsonArrayConst>()) {
    const std::string entry = std::string(path) + "[" + std::to_string(index++) + "]";
    if (!position.is<JsonObjectConst>()) return fail(error, entry, "must be an object");
    if (position["radius_mm"].isUnbound() || position["angle_deg"].isUnbound())
      return fail(error, entry, "requires radius_mm and angle_deg");
    // Positions are stored as floats; reject doubles that would overflow them.
    if (!number(position["radius_mm"], entry + ".radius_mm", 0,
                std::numeric_limits<float>::max(), error) ||
        !number(position["angle_deg"], entry + ".angle_deg", -360, 360, error)) return false;
  }
  return true;
}
}  // namespace

bool validate_config(const JsonDocument& document, std::string* error) {
  if (error) error->clear();
  if (!document.is<JsonObjectConst>()) return fail(error, "config", "must be an object");
  if (!document["type"].is<const char*>() ||
      std::strcmp(document["type"].as<const char*>(), "SCFG") != 0)
    return fail(error, "type", "must be the string SCFG");
  if (!document["schema_version"].is<unsigned int>() ||
      document["schema_version"].as<unsigned int>() != SABRE_CONFIG_VERSION)
    return fail(error, "schema_version", "missing or unsupported configuration version");
  if (!document["robot_name"].isUnbound() && !document["robot_name"].is<const char*>())
    return fail(error, "robot_name", "must be a string");
  if (!integer(document["cell_count"], "cell_count", 1,
               std::numeric_limits<int>::max(), error)) return false;

  const JsonVariantConst motors = document["motors"];
  if (!optional_object(motors, "motors", error)) return false;
  const MotorConfig defaults{};
  const char* protocol = defaults.protocol == MotorProtocol::DShot ? "dshot" :
                         defaults.protocol == MotorProtocol::CRSF ? "crsf" : "oneshot";
  if (!motors["protocol"].isUnbound()) {
    if (!motors["protocol"].is<const char*>()) return fail(error, "motors.protocol", "must be a string");
    protocol = motors["protocol"].as<const char*>();
    if (std::strcmp(protocol, "dshot") && std::strcmp(protocol, "crsf") && std::strcmp(protocol, "oneshot"))
      return fail(error, "motors.protocol", "must be dshot, crsf, or oneshot");
  }
  if (!integer(motors["count"], "motors.count", 1, 2, error) ||
      !integer(motors["startup_delay_ms"], "motors.startup_delay_ms", 0,
               std::numeric_limits<long>::max(), error) ||
      !motor(motors["left"], "motors.left", error) ||
      !motor(motors["right"], "motors.right", error)) return false;
  const bool dual = (motors["count"] | (defaults.count == MotorCount::Dual ? 2 : 1)) == 2;
  const int left_pin = motors["left"]["pin"] | defaults.left_pin;
  const int right_pin = motors["right"]["pin"] | defaults.right_pin;
  if (dual && std::strcmp(protocol, "crsf") != 0 && left_pin == right_pin)
    return fail(error, "motors", "left and right must use different pins");
  if (dual && std::strcmp(protocol, "oneshot") == 0 && left_pin % 16 == right_pin % 16)
    return fail(error, "motors", "pins must not share the same PWM slice/channel");

  const JsonVariantConst dshot = motors["dshot"];
  if (!optional_object(dshot, "motors.dshot", error) ||
      !integer(dshot["bitrate_kbps"], "motors.dshot.bitrate_kbps", 300, 300, error) ||
      !integer(dshot["update_interval_us"], "motors.dshot.update_interval_us", 1,
               std::numeric_limits<long>::max(), error) ||
      !integer(dshot["desync_detect_time_ms"], "motors.dshot.desync_detect_time_ms", 0,
               std::numeric_limits<long>::max(), error)) return false;
  if (!dshot["desync_detection"].isUnbound() && !dshot["desync_detection"].is<bool>())
    return fail(error, "motors.dshot.desync_detection", "must be true or false");

  const JsonVariantConst crsf = motors["crsf"];
  if (!optional_object(crsf, "motors.crsf", error) ||
      !integer(crsf["tx_pin"], "motors.crsf.tx_pin", 0, 29, error) ||
      !integer(crsf["baud_rate"], "motors.crsf.baud_rate", 1,
               std::numeric_limits<long>::max(), error) ||
      !integer(crsf["update_interval_us"], "motors.crsf.update_interval_us", 1,
               std::numeric_limits<long>::max(), error)) return false;
  const unsigned long baud = crsf["baud_rate"] | defaults.crsf.baud_rate;
  const unsigned long interval = crsf["update_interval_us"] | defaults.crsf.update_interval_us;
  if (interval < (260000000ULL + baud - 1) / baud)
    return fail(error, "motors.crsf.update_interval_us", "too short to transmit a 26-byte frame");
  const int left_channel = motors["left"]["channel"] | defaults.crsf.left_channel;
  const int right_channel = motors["right"]["channel"] | defaults.crsf.right_channel;
  if (dual && std::strcmp(protocol, "crsf") == 0 && left_channel == right_channel)
    return fail(error, "motors", "left and right must use different CRSF channels");

  const JsonVariantConst oneshot = motors["oneshot"];
  if (!optional_object(oneshot, "motors.oneshot", error) ||
      !number(oneshot["frequency_hz"], "motors.oneshot.frequency_hz", 0,
              std::numeric_limits<float>::max(), error) ||
      !number(oneshot["min_pulse_us"], "motors.oneshot.min_pulse_us", 0,
              std::numeric_limits<float>::max(), error) ||
      !number(oneshot["max_pulse_us"], "motors.oneshot.max_pulse_us", 0,
              std::numeric_limits<float>::max(), error)) return false;
  const double frequency = oneshot["frequency_hz"] | defaults.oneshot.frequency_hz;
  const double minimum = oneshot["min_pulse_us"] | defaults.oneshot.min_pulse_us;
  const double maximum = oneshot["max_pulse_us"] | defaults.oneshot.max_pulse_us;
  if (frequency <= 0 || minimum <= 0 || maximum <= minimum || maximum * frequency >= 1000000.0)
    return fail(error, "motors.oneshot", "positive, ordered pulses must fit inside the PWM period");

  const JsonVariantConst leds = document["leds"];
  if (!optional_object(leds, "leds", error) ||
      !layout(leds["top"], "leds.top", error) || !layout(leds["bottom"], "leds.bottom", error)) return false;
  // Missing arrays retain defaults; empty arrays disable either or both sides.
  return true;
}
