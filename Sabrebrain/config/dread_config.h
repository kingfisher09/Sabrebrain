#pragma once
#include "active_config.h"

constexpr RobotConfig CONFIG = {
  .numAngles = 150,
  .numLeds   = 23,
  .head_delay = 17; 
  .min_drive = 80;
  .accel_rad = 70.0 / 1000.0;  // input in mm, outputs m
  .flip_rot_direction = true;    // false for rotating with compass, true for against compass
  .x_scale = 1;
  .y_scale = 0;
  .z_scale = 1;
};
