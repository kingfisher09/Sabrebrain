#pragma once

#include "sabre_config.h"
#include <cmath>

// Definition shared with compile-time tests; PWM hardware stays private.
constexpr float oneshot_duty(float throttle, const OneShotMotorConfig& config) {
  if (std::isnan(throttle)) throttle = 0.0f;
  if (throttle < -1.0f) throttle = -1.0f;
  if (throttle > 1.0f) throttle = 1.0f;
  const float pulse_us = config.min_pulse_us +
      (throttle + 1.0f) * 0.5f * (config.max_pulse_us - config.min_pulse_us);
  // Floating-point period avoids the integer truncation in the old code.
  return pulse_us * config.frequency_hz / 10000.0f;
}
