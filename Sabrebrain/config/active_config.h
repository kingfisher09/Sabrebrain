#pragma once
#include <stdint.h>

// Generic, immutable robot configuration
struct RobotConfig {
  // Geometry / layout
  uint16_t numAngles;
  uint16_t numLeds;
  uint16_head_delay; 
  uint16_t min_drive;
  const float accel_rad;  // input in mm, outputs m
  bool flip_rot_direction;         // false for rotating with compass, true for against compass
  float x_scale;
  float y_scale;
  float z_scale;
};
