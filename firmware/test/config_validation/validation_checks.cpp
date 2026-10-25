// Host-side behavioural tests. See README.md for the build/run command.
#include "sabre_config_validation.h"
#include "sabre_config.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

static void expect(const char* json, bool valid, const char* error_path = "") {
  JsonDocument doc;
  assert(!deserializeJson(doc, json));
  std::string error;
  assert(validate_config(doc, &error) == valid);
  if (valid) assert(error.empty());
  else assert(error.find(error_path) == 0);
}

int main() {
  expect(R"({"type":"SCFG","schema_version":1})", true);
  expect(R"([])", false, "config");
  expect(R"({"schema_version":1})", false, "type");
  expect(R"({"type":1396917831,"schema_version":1})", false, "type");
  expect(R"({"type":"SCAL","schema_version":1})", false, "type");
  expect(R"({"type":"SCFG","schema_version":2})", false, "schema_version");
  expect(R"({"type":"SCFG","schema_version":"1"})", false, "schema_version");
  expect(R"({"type":"SCFG","schema_version":1,"motors":null})", false, "motors");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"count":0}})", false, "motors.count");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"count":1.5}})", false, "motors.count");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"protocol":"banana"}})", false, "motors.protocol");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"left":{"direction":0}}})", false, "motors.left.direction");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"left":{"pin":"4"}}})", false, "motors.left.pin");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"left":{"pin":30}}})", false, "motors.left.pin");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"left":{"pin":3}}})", false, "motors");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"count":1,"left":{"pin":3}}})", true);
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"protocol":"oneshot","left":{"pin":4},"right":{"pin":20}}})", false, "motors");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"protocol":"crsf","left":{"channel":2}}})", false, "motors");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"crsf":{"baud_rate":0}}})", false, "motors.crsf.baud_rate");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"crsf":{"update_interval_us":100}}})", false, "motors.crsf.update_interval_us");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"oneshot":{"frequency_hz":4000}}})", false, "motors.oneshot");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"oneshot":{"min_pulse_us":250}}})", false, "motors.oneshot");
  expect(R"({"type":"SCFG","schema_version":1,"motors":{"dshot":{"desync_detection":0}}})", false, "motors.dshot.desync_detection");
  expect(R"({"type":"SCFG","schema_version":1,"leds":{"top":[],"bottom":[]}})", true);
  expect(R"({"type":"SCFG","schema_version":1,"leds":{"bottom":[]}})", true);
  expect(R"({"type":"SCFG","schema_version":1,"leds":{"top":null}})", false, "leds.top");
  expect(R"({"type":"SCFG","schema_version":1,"leds":{"top":[{}]}})", false, "leds.top[0]");
  expect(R"({"type":"SCFG","schema_version":1,"leds":{"top":[{"radius_mm":-1,"angle_deg":0}]}})", false, "leds.top[0].radius_mm");

  // More LEDs than the compiled defaults must be accepted.
  JsonDocument doc;
  doc["type"] = "SCFG";
  doc["schema_version"] = 1;
  JsonArray top = doc["leds"]["top"].to<JsonArray>();
  for (int i = 0; i < 40; ++i) {
    JsonObject position = top.add<JsonObject>();
    position["radius_mm"] = i * 7;
    position["angle_deg"] = 0;
  }
  assert(validate_config(doc));
  Sabre_Config config;
  assert(config_from_json(doc, config));
  assert(config.TOP_LED_POSITIONS.size() == 40);
  assert(config.TOP_LED_POSITIONS[39].radius_mm == 273);
  assert(config.BOTTOM_LED_POSITIONS.empty());  // Missing bottom layout keeps the empty default.
  top[39]["radius_mm"] = std::numeric_limits<double>::infinity();
  assert(!validate_config(doc));
  assert(!config_from_json(doc, config));
  assert(config.TOP_LED_POSITIONS.size() == 40);  // Invalid input leaves the old config intact.
  top[39]["radius_mm"] = std::numeric_limits<double>::quiet_NaN();
  assert(!validate_config(doc));

  std::ifstream file("config/robot.example.json");
  assert(file);
  const std::string example((std::istreambuf_iterator<char>(file)), {});
  assert(!deserializeJson(doc, example));
  assert(validate_config(doc));
  assert(config_from_json(doc, config));
  assert(config.robot_name == "My robot");
  assert(config.motor_config.protocol == MotorProtocol::DShot);
  assert(config.motor_config.left_pin == 4 && config.motor_config.right_pin == 3);
  assert(config.motor_config.dshot.update_interval_us == 2000);
  assert(config.TOP_LED_POSITIONS.size() == 26 && config.BOTTOM_LED_POSITIONS.size() == 9);

  assert(!deserializeJson(doc, R"({"type":"SCFG","schema_version":1,"robot_name":"Test robot","motors":{"protocol":"crsf","count":1,"startup_delay_ms":0,"left":{"channel":7,"direction":-1},"dshot":{"desync_detection":false,"update_interval_us":3000},"crsf":{"tx_pin":8,"baud_rate":400000,"update_interval_us":1500},"oneshot":{"frequency_hz":3000,"min_pulse_us":130,"max_pulse_us":240}},"leds":{"top":[],"bottom":[]}})"));
  assert(config_from_json(doc, config));
  assert(config.robot_name == "Test robot");
  assert(config.motor_config.protocol == MotorProtocol::CRSF);
  assert(config.motor_config.count == MotorCount::Single);
  assert(config.motor_config.startup_delay_ms == 0);
  assert(config.motor_config.left_direction == -1);
  assert(config.motor_config.crsf.left_channel == 7);
  assert(config.motor_config.crsf.tx_pin == 8 && config.motor_config.crsf.baud_rate == 400000);
  assert(config.motor_config.crsf.update_interval_us == 1500);
  assert(!config.motor_config.dshot.desync_detection);
  assert(config.motor_config.dshot.update_interval_us == 3000);
  assert(config.motor_config.oneshot.frequency_hz == 3000);
  assert(config.motor_config.oneshot.min_pulse_us == 130);
  assert(config.motor_config.oneshot.max_pulse_us == 240);
  assert(config.TOP_LED_POSITIONS.size() == 0 && config.BOTTOM_LED_POSITIONS.size() == 0);
  assert(config.TOP_LED_POSITIONS.empty() && config.BOTTOM_LED_POSITIONS.empty());

  assert(!deserializeJson(doc, R"({"type":"SCFG","schema_version":1})"));
  assert(config_from_json(doc, config));  // Fresh defaults, not the previous file's values.
  assert(config.motor_config.protocol == MotorConfig{}.protocol);
  assert(config.motor_config.crsf.left_channel == 1);
  assert(config.TOP_LED_POSITIONS.empty() && config.BOTTOM_LED_POSITIONS.empty());
  std::cout << "Configuration validation checks passed\n";
}
