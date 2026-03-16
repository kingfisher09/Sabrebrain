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
#include "CRSFforArduino.hpp"
#include "RP2040_PWM.h"
#include "SparkFun_LIS331.h"

// Sabrebrain headers
#include "sabre_config"
#include "sabre_vid.h"

// Forward declarations 
extern int deadzone;
extern int oneshot_Freq;
extern float correct_max;
extern int head_delay;
extern const int rainbow_delay;
extern const int flash_delay;
extern bool flash_now;
extern CRGB leds[];

// Next 2 should really be removed if I create a load vid function in sabrescreen
extern CRGB (*current_frame)[NUM_LEDS];
extern int frame_num;

// ---- Robot geometry (defined in main.cpp) ----
extern bool flip_rot_direction;

// ---- Motors (defined in main.cpp) ----
extern RP2040_PWM* motor_Right;
extern RP2040_PWM* motor_Left;
extern const int MOTOR_RIGHT_PIN;
extern const int MOTOR_LEFT_PIN;

// ---- RF channels (defined in main.cpp) ----
extern CRSFforArduino crsf;
extern const int SLIP_CH;
extern const int TRANS_CH;
extern const int SPIN_CH;
extern const int HEAD_CH;
extern const int INVERT_CH;
extern const int CORRECT_CH;
extern const int HEAD_MODE_CH;
extern const int DIR_CH;
extern const int LIGHT_CH;
extern const int TRIM_CH;
extern const int EMOTE_CH;

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
extern float correct;
extern bool headMode;
extern bool trimMode;
extern float head_trim;
extern int image_mode;
extern bool emote;
extern float invert;

// ---- Function declarations ----
float oneshot_Duty(int thoucentage, int dir_flip);
void command_motors(int left, int right);
void updateCRSF();
void trim();
int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);
void paint_screen(float angle);
void load_vid(const SabreVid& video);
void load_frame();
void flash();
void flashing();
void rainbow_line();
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t linkStatistics);
float wrap360(float angle);
float angleDistance(float a, float b);