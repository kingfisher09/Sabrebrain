// Software for melty brain robot written by Owen Fisher 2024-25

#include "CRSFforArduino.hpp"
#include "RP2040_PWM.h"
#include "SparkFun_LIS331.h"
#include <Wire.h>
#include <FastLED.h>
#include <Adafruit_MMC56x3.h>
#include <math.h>

// images here:
#include "image_pointer.h"
#include "image_aircraft_lights.h"
#include "image_fast.h"

/* Assign a unique ID to this sensor at the same time */
Adafruit_MMC5603 mmc = Adafruit_MMC5603(12345);
float read_mag();
float angleDistance(float a, float b);
float wrap360(float angle);

LIS331 xl;  // accelerometer thing

CRSFforArduino crsf = CRSFforArduino(&Serial1);

/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// settings
int deadzone = 30;   // for transmitter sticks
int max_head = 360;  // max heading change in deg/s
int oneshot_Freq = 3500;
int speed_int = 300;       // miliseconds between speed measurements
int head_delay = 17;       // 17 seems good for v4, may need to be adjusted in future
float correct_max = -0.1;  // ± ratio for radial correct, 0.5 would mean a range from 0.5 to 1.5
int min_drive = 80;
const int rainbow_delay = 40;

// Robot stuff
const float accel_rad = 75.0 / 1000.0;  // input in mm, outputs m
bool flip_rot_direction = true;         // false for rotating with compass, true for against compass
#define RIGHT_MOTOR_DIRECTION -1
#define LEFT_MOTOR_DIRECTION -1
#define TRANS_SIGN -1  // swap translate direction
#define SLIP_SIGN -1   // swap slip direction

// pins
const int MOTOR_RIGHT_PIN = 4;
const int MOTOR_LEFT_PIN = 3;
#define LED_POWER_PIN 11   //  builtin LED Power control pin
#define LED_PIN 12         // Data pin for NeoPixel
const int headPin = 27;    // LED heading data pin
const int headClock = 28;  // LED clock pin

// Sabrescreen stuff
const int NUM_LEDS = 23;
const float NUM_SLICES = 150;  // the 3 slice settings all need to be float for the calculations to work
const float slice_size = 360 / NUM_SLICES;
const float half_slice = slice_size / 2;
int bow_pos = 0;  // for keeping track of rainbow pixel

CRGB LEDframe[(int)NUM_SLICES][(int)NUM_LEDS];  // 2d array to hold current LED frame
CRGB leds[NUM_LEDS];                            // array to hold LED colours
void paint_screen();                            // function to control LEDs
bool update_image = false;
const int hue_change = round(255 / NUM_LEDS);  // make sure you get a full rainbow along the line

const int accel_pow = 26;  // pin to power accelerometer, allows it to be restarted easily

// motors
RP2040_PWM *motor_Right;
RP2040_PWM *motor_Left;
float oneshot_Duty(int thoucentage, int dir_flip);
void command_motors(int left, int right);

// RF stuff

void updateCRSF();
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int MAG_CH = 5;      // used for deactivating magnetometer
const int CORRECT_CH = 6;  // used to correct accel radius
const int HEAD_MODE_CH = 7;
const int DIR_CH = 8;  // used to correct heading offset
const int LIGHT_CH = 9;
const int EMOTE_CH = 10;  // used to trigger emote message

bool stopflag = false;
bool mag_speed_calc = true;  // variable to control whether speed is calculated with magnetometer or accelerometer
float mag_offset;            // holds the difference between 0 degrees bearing and 0 degrees for robot

int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
float correct = 1;
bool headMode;

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

  FastLED.addLeds<WS2812B, headPin, GRB>(leds, NUM_LEDS);  // connect to LED strip
  FastLED.clear();                                         // ensure all LEDs start off
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

  // set up mag
  while (!mmc.begin(MMC56X3_DEFAULT_ADDRESS, &Wire)) {  // I2C mode
    Serial.println("Ooops, no MMC5603 detected ... Check your wiring!");
    delay(500);
  }

  memcpy(LEDframe, image_aircraft_lights, sizeof(image_aircraft_lights));  // set the default image in memory

  /* Display some basic information on this sensor */
  mmc.printSensorDetails();

  mmc.setDataRate(1000);  // in Hz, from 1-255 or 1000
  mmc.setContinuousMode(true);

  Serial.println("Thread 1 started");
  xoff = 10;
  yoff = 10;
}

void loop() {                    // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // angle calc
  if (mag_speed_calc) {
    zrot = zrotspd;
    mag_offset = wrap360(mag_offset - (head / 200));  // adjust magnetic offset using stick

  } else {
    zrot = zrotspd - (head / 3);  // add in head for changing angle
  }
  angle = fmod(angle + (zrot * (now - last_angle_time) / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop
  last_angle_time = micros();


  int left_sig, right_sig;

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

  command_motors(left_sig, right_sig);

  if (loopcount == 20000) {
    // Serial.println("Loop 0");
    // Serial.println(micros() - now);
    // # timing
    loopcount = 0;
  } else {
    loopcount += 1;
  }
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
                // At the moment, speed will not be calculated if we always have a compass reading available, this could mean we don't get anything telemetry wise at low speed

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # timing

  updateCRSF();  // update control

  if (headMode) {
    if (spin == 0) {             // not spinning head mode
      angle = 0;                 // allows user to press headmode button to set forwards when not spinning, lights will flash and drive will stop momentarily
      mag_offset = -read_mag();  // sets mag offset when not spinning

      for (int i = 0; i <= 3; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::White);  // FastLED built-in function
        FastLED.show();
      }
      delay(300);
      FastLED.clear();

    } else {                                      // spinning head mode
      static float head_change = 0;               // var to hold heading change between loops while button is held
      if (abs(slip) > 200 || abs(trans) > 200) {  // make sure stick is a reasonable distance from centre. Otherwise the stick vibration when released gives the wrong result
        head_change = degrees(atan2(-slip, trans));
      } else {  // doing it this way makes is to you have to release the button after the stick
        if (read_mag) {
          mag_offset = -read_mag();
        } else {
          angle = angle - head_change;
        }
        head_change = 0;
      }
    }
  }

  if (mag_speed_calc) {
    float heading = read_mag();  // update magnetometer
    static float old_mag_heading = 0;
    static unsigned long last_mag_update = 0;


    angle = heading + mag_offset;
    last_angle_time = micros();  // needed on other loop

    float heading_change = angleDistance(heading, old_mag_heading);

    if (heading_change > 20) {                                            // wait until heading has changed by at least x degrees before calc speed
      zrotspd = 1000000 * heading_change / (micros() - last_mag_update);  // calculate rotational speed from mag angle change and time
      old_mag_heading = heading;                                          // record the last magnetic angle (not just the last angle)
      last_mag_update = micros();
    }

  } else {
    if (xl.newXData()) {
      int16_t x, y, z;
      xl.readAxes(x, y, z);
      x = x + xoff;
      y = y + yoff;
      float xg = xl.convertToG(200, x);
      float yg = xl.convertToG(200, y);

      float measure_accel = 9.81 * sqrt(pow(xg, 2) + pow(yg, 2));  // given in m/s^2

      // FILTER ACCEL
      float filtered_accel = (measure_accel * a0) + (prev_filt_val * b1);
      prev_filt_val = filtered_accel;

      zrotspd = degrees(sqrt(filtered_accel / (correct * accel_rad)));  // deg/s
    }
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    // Serial.println(zrot / 6);
    lastGpsUpdate = now;
    // Update the GPS telemetry data with the new values.
    crsf.telemetryWriteGPS(0, head_delay, zrotspd * 6000 / 360, 0, accel_rad * 100 * correct, 0);
  }

  // # timing
  if (loopcount == 5000) {
    // Serial.println("Loop 1");
    // Serial.println(micros() - now);
    loopcount = 0;
  } else {
    loopcount += 1;
  }
}
