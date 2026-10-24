#pragma once

#include <stdint.h>

struct motorSpeeds {
  uint32_t left;
  uint32_t right;
};

struct MotorConfig;

void setup_motors(const MotorConfig& config);
motorSpeeds command_motors(float left, float right);
