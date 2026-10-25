#pragma once

#include "sabre_config.h"
#include "sabre_crsf_output.h"
#include "sabre_oneshot_output.h"
#include <limits>

// Included by src/compile_tests.cpp during every normal PlatformIO build.

constexpr MotorConfig default_motors{};

// Check default OneShot settings at build time instead of during motor setup.
static_assert(default_motors.protocol != MotorProtocol::OneShot ||
                  (default_motors.left_pin >= 0 && default_motors.left_pin <= 29 &&
                   (default_motors.count == MotorCount::Single ||
                    (default_motors.right_pin >= 0 && default_motors.right_pin <= 29 &&
                     default_motors.left_pin % 16 != default_motors.right_pin % 16))),
              "OneShot needs valid GPIOs with distinct PWM outputs");
static_assert(default_motors.protocol != MotorProtocol::OneShot ||
                  ((default_motors.left_direction == 1 || default_motors.left_direction == -1) &&
                   (default_motors.count == MotorCount::Single ||
                    default_motors.right_direction == 1 || default_motors.right_direction == -1)),
              "OneShot motor directions must be 1 or -1");
static_assert(default_motors.protocol != MotorProtocol::OneShot ||
                  (default_motors.oneshot.frequency_hz > 0 &&
                   default_motors.oneshot.min_pulse_us > 0 &&
                   default_motors.oneshot.max_pulse_us > default_motors.oneshot.min_pulse_us &&
                   default_motors.oneshot.max_pulse_us * default_motors.oneshot.frequency_hz < 1000000.0f),
              "OneShot pulses must be positive, ordered, and shorter than the PWM period");

// OneShot conversion checks.
static_assert(oneshot_duty(-1.0f, OneShotMotorConfig{}) == 43.75f,
              "OneShot reverse is 125 us at 3500 Hz");
static_assert(oneshot_duty(0.0f, OneShotMotorConfig{}) == 65.625f,
              "OneShot zero throttle is 187.5 us at 3500 Hz");
static_assert(oneshot_duty(1.0f, OneShotMotorConfig{}) == 87.5f,
              "OneShot forward is 250 us at 3500 Hz");
static_assert(oneshot_duty(-2.0f, OneShotMotorConfig{}) == 43.75f,
              "OneShot reverse clamping");

namespace motor_compile_checks {
// CRSF conversion, packing, and checksum checks.
static_assert(sabre_crsf::throttle_channel(-1.0f) == 172, "Reverse endpoint");
static_assert(sabre_crsf::throttle_channel(0.0f) == 992, "Neutral");
static_assert(sabre_crsf::throttle_channel(std::numeric_limits<float>::quiet_NaN()) ==
                  sabre_crsf::neutral,
              "NaN commands neutral");
static_assert(sabre_crsf::throttle_channel(1.0f) == 1812, "Forward endpoint");
static_assert(sabre_crsf::throttle_channel(-2.0f) == 172, "Reverse clamp");
static_assert(sabre_crsf::throttle_channel(2.0f) == 1812, "Forward clamp");

constexpr bool check_packet(bool neutral_only) {
  uint16_t channels[16] = {};
  uint8_t frame[26] = {};
  for (int i = 0; i < 16; ++i) channels[i] = neutral_only ? 992 : 172 + i * 100;
  sabre_crsf::encode_channels(channels, frame);
  if (frame[0] != 0xC8 || frame[1] != 24 || frame[2] != 0x16) return false;
  // Decode with a byte accumulator to check every channel and byte boundary.
  uint32_t accumulator = 0;
  int bits = 0;
  int channel = 0;
  for (int i = 3; i < 25; ++i) {
    accumulator |= static_cast<uint32_t>(frame[i]) << bits;
    bits += 8;
    if (bits >= 11) {
      if ((accumulator & 0x7FF) != channels[channel++]) return false;
      accumulator >>= 11;
      bits -= 11; 
    }
  }
  // Independently check CRC by polynomial division across type/payload/CRC.
  unsigned remainder = 0;
  for (int i = 2; i < 26; ++i) {
    for (int bit = 7; bit >= 0; --bit) {
      remainder = (remainder << 1) | ((frame[i] >> bit) & 1);
      if (remainder & 0x100) remainder ^= 0x1D5;
    }
  }
  return channel == 16 && bits == 0 && remainder == 0;
}

static_assert(check_packet(true), "Neutral frame packing and CRC");
static_assert(check_packet(false), "Distinct channels packing and CRC");
}  // namespace motor_compile_checks
