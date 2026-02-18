#pragma once
#include <stdint.h>

// Generic, immutable robot configuration
struct RobotConfig {
  // Geometry / layout
  uint16_t numAngles;
  uint16_t numLeds;
  uint16_t head_delay; 
  uint16_t min_drive;
  float accel_rad;  // input in mm, outputs m
  bool flip_rot_direction;         // false for rotating with compass, true for against compass
  float x_scale;
  float y_scale;
  float z_scale;
};

#if (defined(ROBOT_SABRE) + defined(ROBOT_DREAD)) != 1
  #error "Define exactly one robot target (ROBOT_SABRE or ROBOT_DREAD)"
#endif

#if defined(ROBOT_SABRE)
  #include "sabre_config.h"

#elif defined(ROBOT_DREAD)
  #include "dread_config.h"

#else
  #error "Define exactly one robot target"
#endif