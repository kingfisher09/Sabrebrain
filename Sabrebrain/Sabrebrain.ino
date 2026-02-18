// Software for melty brain robot written by Owen Fisher 2024-26

// NOTE:
// Robot configuration is selected via config/target.h
// Intended future migration to PlatformIO with per-robot build environments


// #define ROBOT_SABRE
#define ROBOT_DREAD

#include "config/active_config.h"  // this defines CONFIG

constexpr uint16_t MASK_BYTES_PER_FRAME =
  (uint32_t(CONFIG.numAngles) * uint32_t(CONFIG.numLeds) * 2u + 7u) / 8u;


// PUT THESE IN CONFIG?????????????????????????
// images here:
#include <FastLED.h>
#include "image_taunt.h"
#include "image_calibrate.h"

// videos here:
#include "videos/bouncing_pumpkin.h"
// #include "videos/haloween_vid.h"
#include "videos/sabremation.h"

#include "CRSFforArduino.hpp"
#include "RP2040_PWM.h"
#include "SparkFun_LIS331.h"
#include <Wire.h>
#include <math.h>
extern "C" {
#include <hardware/watchdog.h>
}
#include "sabre_Vid.h"  // needed for struct for videos
#include <utility>


LIS331 xl;  // accelerometer thing

CRSFforArduino crsf = CRSFforArduino(&Serial1);

/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// settings
int deadzone = 30;   // for transmitter sticks
int max_head = 360;  // max heading change in deg/s
int oneshot_Freq = 3500;
constexpr uint16_t head_delay = CONFIG.head_delay;
// THIS DOESN'T SEEM TO BE USED????????????
float correct_max = 0.0;  // ± ratio for radial correct, 0.5 would mean a range from 0.5 to 1.5  ----- to be removed with calibration
constexpr uint16_t min_drive = CONFIG.min_drive;
const int rainbow_delay = 40;
const int flash_delay = 10;
bool flash_now = false;  // whether currently doing a flash

// Robot stuff
constexpr float accel_rad = CONFIG.accel_rad;
constexpr bool flip_rot_direction = CONFIG.flip_rot_direction;  // false for rotating with compass, true for against compass
#define RIGHT_MOTOR_DIRECTION -1
#define LEFT_MOTOR_DIRECTION -1
#define TRANS_SIGN -1  // swap translate direction
#define SLIP_SIGN -1   // swap slip direction
#define HEAD_CONTROL_SCALE 0.33

// pins
const int MOTOR_RIGHT_PIN = 4;
const int MOTOR_LEFT_PIN = 3;
#define LED_POWER_PIN 11   //  builtin LED Power control pin
#define LED_PIN 12         // Data pin for NeoPixel
const int headPin = 27;    // LED heading data pin
const int headClock = 28;  // LED clock pin

// Sabrescreen stuff

constexpr uint16_t NUM_LEDS = CONFIG.numLeds;
constexpr uint16_t NUM_ANGLES = CONFIG.numAngles;

const float slice_size = 360.0f / NUM_ANGLES;
const float half_slice = slice_size / 2.0f;
int bow_pos = 0;  // for keeping track of rainbow pixel

using Row = CRGB[NUM_LEDS];
Row bufferA[NUM_ANGLES] = { 0 };
Row bufferB[NUM_ANGLES] = { 0 };

Row* current_frame = bufferA;
Row* next_frame = bufferB;

const SabreVid* current_vid = nullptr;  // pointer to whichever video is active. Const because we never write to what the pointer is pointing at

int frame_duration;
int frame_num;
int num_frames;
unsigned long frame_time;

void load_vid(const SabreVid& video);
void load_frame();

CRGB leds[NUM_LEDS];  // array to hold LED colours
void paint_screen();  // function to control LEDs
bool update_image = false;
const int hue_change = round(255 / NUM_LEDS);  // make sure you get a full rainbow along the line
void flash();
void flashing();

int sel_video = 0;

// End Sabrescreen stuff

const int accel_pow = 26;  // pin to power accelerometer, allows it to be restarted easily

// motors
RP2040_PWM* motor_Right;
RP2040_PWM* motor_Left;
float oneshot_Duty(int thoucentage, int dir_flip);
void command_motors(int left, int right);

// RF stuff
void updateCRSF();
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int CORRECT_CH = 6;  // used to correct accel radius
const int HEAD_MODE_CH = 7;
const int DIR_CH = 8;  // used to correct heading offset
const int LIGHT_CH = 9;
const int TRIM_CH = 10;   // used to trim rotation
const int EMOTE_CH = 11;  // used to trigger emote message

// Calibration stuff
bool calib_held = false;
unsigned long calib_held_start;
int calibration_mode = 0;
int calib_press_delay = 3000;

// safety stuff
unsigned long stopflag_time = 0;
int E_stop_time = 100;          // ms allowed between ELRS signals before shutting down motors
int watchdog_time = 1000;       // ms after E_stop before resetting MCU
bool watchdog_enabled = false;  // bool to record watchdog status. Watchdog will be enabled when transmitter first sends data, MCU will restart 1s after estop if no more signals are received

int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
float correct = 1;
bool headMode;
bool calibPress = false;
float head_trim = 0;
int image_mode;
bool emote;

// rotation tracking
float angle = 0;                    // current robot angle
unsigned long last_angle_time = 0;  // program time when last angle was calculated
float zrotspd = 0;                  // measured speed
float zrot = 0;                     // measured speed with heading control injected
int16_t xoff = 0;
int16_t yoff = 0;
int16_t zoff = 0;

// filter settings
float x = 0.3;
float a0 = 1 - x;
float b1 = x;
float prev_filt_val = 0;


void setup() {
  // put your setup code here, to run once:
  // Initialise CRSF for Arduino.
  Serial.begin(115200);
  delay(1000);
  Serial.println("Thread 0 starting...");
  if (!crsf.begin()) {
    Serial.println("CRSF for Arduino initialisation failed!");
    while (1) {
      delay(10);
    }
  }

  /* Set your link statistics callback. */
  crsf.setLinkStatisticsCallback(onLinkStatisticsUpdate);

  // set up LEDs

  // Builtin LED first
  pinMode(LED_POWER_PIN, OUTPUT);  // Turn on LED power
  digitalWrite(LED_POWER_PIN, HIGH);
  // CRGB builtinLED[] = { CRGB(128, 128, 128) };
  // FastLED.addLeds<WS2812B, LED_PIN, GRB>(builtinLED, 1);

  FastLED.addLeds<APA102, headPin, headClock, BGR>(leds, NUM_LEDS);  // connect to LED strip
  FastLED.clear();                                                   // ensure all LEDs start off
  FastLED.show();

  // initialise motors
  motor_Right = new RP2040_PWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(0, 1));
  motor_Left = new RP2040_PWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(0, 1));
  Serial.println("Thread 0 started");
  delay(1000);  // wait for ESCs to start up
}

void setup1() {
  Wire.begin();
  // Reset accelerometer
  pinMode(accel_pow, OUTPUT);
  digitalWrite(accel_pow, LOW);
  delay(1000);
  digitalWrite(accel_pow, HIGH);
  Serial.println("Accel has power");

  // Set up accel
  xl.setI2CAddr(0x19);  // This MUST be called BEFORE .begin() so begin() can communicate with the chip
  bool accel_active = false;

  while (!accel_active) {
    xl.begin(LIS331::USE_I2C);
    xl.setFullScale(LIS331::MED_RANGE);
    xl.setODR(LIS331::DR_1000HZ);
    delay(100);
    if (xl.newXData()) {
      accel_active = true;
    } else {
      delay(1000);
      Serial.println("Accel begin failed, trying again");
    }
  }


  Serial.println("Thread 1 started");
  xoff = 10;
  yoff = 10;

  load_vid(bouncing_pumpkin);
  // load_vid(haloween_vid);
}

void loop() {                    // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // angle calc
  zrot = zrotspd - (head * HEAD_CONTROL_SCALE) - head_trim;                     // add in head for changing angle
  angle = fmod(angle + (zrot * (now - last_angle_time) / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop
  last_angle_time = micros();

  int left_sig, right_sig;
  if (calibration_mode != 0) {

    // robot control modes
    if (spin > 0) {     // spinning mode
      if (!headMode) {  // spinning mode
        float cosresult = cos(radians(angle));
        float sinresult = sin(radians(angle));
        float delta = (TRANS_SIGN * trans * cosresult) + (SLIP_SIGN * slip * sinresult);  // calculate motor delta
        left_sig = spin + delta;
        right_sig = -spin + delta;
        paint_screen(angle);  // update screen
      } else {                // if headmode, just keep spinnin
        left_sig = spin;
        right_sig = -spin;
      }

    } else {  // normal robot mode

      rainbow_line();  // draw rainbow
      // slip = slip * 0.1;  // reduce turning speed

      // normal driving with minimum motor speed
      left_sig = slip + trans;
      left_sig = (abs(left_sig) < min_drive) ? 0 : left_sig;
      right_sig = -slip + trans;
      right_sig = (abs(right_sig) < min_drive) ? 0 : right_sig;
    }
  } else {
    // calibration here
    if (spin <= 99) {  // exit calibration mode
      calibration_mode = 0;
      continue;
    }
    int calib_throttle = (min(spin, calibration_mode * 100);
		left_sig = calib_throttle;
		right_sig = calib_throttle;
  }

  command_motors(left_sig, right_sig);
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
                // At the moment, speed will not be calculated if we always have a compass reading available, this could mean we don't get anything telemetry wise at low speed

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # timing

  updateCRSF();  // update control

  // calibration stuff
  if (calibPress) {  // calibration button currently pressed

    if (!calib_held) {
      // Button was just pressed
      calib_held_start = millis();
      calib_held = true;
    } else {
      // Button is being held
      if (millis() - calib_held_start > calib_press_delay) {
        calibration_mode = 1;
      }
    }

  } else {
    // Button released
    calib_held = false;
  }


  if (headMode) {
    if (spin == 0) {  // not spinning head mode

    } else {                                      // spinning head mode
      static float head_change = 0;               // var to hold heading change between loops while button is held
      if (abs(slip) > 200 || abs(trans) > 200) {  // make sure stick is a reasonable distance from centre. Otherwise the stick vibration when released gives the wrong result
        head_change = degrees(atan2(-slip, trans));
      } else {  // doing it this way makes is to you have to release the button after the stick

        angle = angle - head_change;
        flash();

        head_change = 0;
      }
    }
  }

  if (xl.newXData()) {
    int16_t x, y, z;
    xl.readAxes(x, y, z);
    x = x + xoff;
    y = y + yoff;
    z = z + zoff;
    float xg = xl.convertToG(200, x) * x_scale;
    float yg = xl.convertToG(200, y) * y_scale;
    float zg = xl.convertToG(200, z) * z_scale;

    float measure_accel = 9.81 * sqrt(pow(xg, 2) + pow(yg, 2) + pow(zg, 2));  // given in m/s^2

    // FILTER ACCEL
    float filtered_accel = (measure_accel * a0) + (prev_filt_val * b1);
    prev_filt_val = filtered_accel;

    zrotspd = degrees(sqrt(filtered_accel / (correct * accel_rad)));  // deg/s
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    // Serial.println(zrot / 6);
    lastGpsUpdate = now;
    // Update the GPS telemetry data with the new values.
    crsf.telemetryWriteGPS(0, head_delay, zrotspd * 6000 / 360, 0, accel_rad * 100 * correct, 0);
  }

  int sel;
  if (image_mode < 1250) {
    sel = 0;
  } else if (image_mode < 1750) {
    sel = 1;
  } else {
    sel = 2;
  }

  if (sel != sel_video) {
    // if (sel == 0) { load_vid(haloween_vid); }
    if (sel == 1) { load_vid(bouncing_pumpkin); }
    if (sel == 2) { load_vid(sabremation); }
    sel_video = sel;
  }

  if (!emote) {
    // play annimation
    load_frame();
  } else {
    memcpy(current_frame, image_taunt, sizeof(image_taunt));
    frame_num = -1;
  }

  // flash annimation
  if (flash_now) {
    flashing();
  }
}
