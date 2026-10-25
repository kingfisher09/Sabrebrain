#pragma once

#include <ArduinoJson.h>
#include <string>

// Validates the JSON format before converting it to Sabre_Config. Optional
// omissions use firmware defaults; present null/wrong-type values are errors.
bool validate_config(const JsonDocument& document, std::string* error = nullptr);
