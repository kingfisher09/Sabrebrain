// This file should be included in any CPP file in the project
// Anything in this file will be accessible to all cpp files in the project.

#pragma once

// Standard libraries
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <utility>
#include <atomic>

// Hardware
extern "C" {  // tels compiler this is C, not C++
#include <hardware/watchdog.h>
}

// External libraries
#include <PIO_DShot.h>
#include "CRSFforArduino.hpp"
#include "SparkFun_LIS331.h"

// Sabrebrain headers
#include "sabre_config.h"
#include "config/sabre_calibration.h"
#include "sabre_vid.h"

// ---- Movement (defined in main.cpp) ----
extern bool headMode;
extern float filtered_accel;

// ---- Structures ----
extern SabreCalibration global_calibration;
extern bool calibration_loaded;

// ---- Function declarations ----

float get_trim_for_accel(float accel);
