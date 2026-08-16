// This file should be included in any CPP file in the project
// Anything in this file will be accessible to all cpp files in the project.

#pragma once

// Standard libraries
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <utility>

// Hardware
extern "C" {  // tels compiler this is C, not C++
#include <hardware/watchdog.h>
}

// External libraries
#include <FastLED.h>
#include <PIO_DShot.h>
#include "CRSFforArduino.hpp"
#include "SparkFun_LIS331.h"

// Sabrebrain headers
#include "sabre_config.h"
#include "config/sabre_calibration.h"
#include "sabre_vid.h"

// Forward declarations
extern const int rainbow_delay;
extern const int flash_delay;
extern bool flash_now;
extern CRGB leds[];

// Next 2 should really be removed if I create a load vid function in
// sabrescreen
extern CRGB (*current_frame)[NUM_LEDS];
extern int frame_num;

// ---- Motors (defined in main.cpp) ----
extern BidirDShotX1* motor_Left;
extern BidirDShotX1* motor_Right;
extern uint32_t erpm_left;
extern uint32_t erpm_right;
extern const int MOTOR_RIGHT_PIN;
extern const int MOTOR_LEFT_PIN;

// ---- RF channels (defined in main.cpp) ---- I
extern CRSFforArduino crsf;

// ---- Safety (defined in main.cpp) ----
extern unsigned long stopflag_time;
extern int E_stop_time;
extern int watchdog_time;
extern bool watchdog_enabled;

// ---- Movement (defined in main.cpp) ----
extern float slip;
extern float trans;
extern float head;
extern float spin;
extern bool headMode;
extern int image_mode;
extern bool emote;
extern float invert;
extern bool save_button;

// ---- Structures ----
extern SabreCalibration calibration;
extern bool calibration_loaded;
struct motorSpeeds {
    uint32_t left;
    uint32_t right;
};

// ---- Function declarations ----
motorSpeeds command_motors(float left, float right);
void updateCRSF();
void paint_screen(float angle);
void load_vid(const SabreVid& video);
void load_frame();
void flash();
void flashing();
void rainbow_line();
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t linkStatistics);
void passthrough();
// float wrap360(float angle);
// float angleDistance(float a, float b);