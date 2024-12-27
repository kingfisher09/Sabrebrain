#include "CRSFforArduino.hpp"
#include "RP2040_PWM.h"
#include "SparkFun_LIS331.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>  //needed for RGB LED

LIS331 xl;
CRSFforArduino crsf = CRSFforArduino(&Serial1);


/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// settings
int deadzone = 30;
int max_head = 360;  // max heading change in deg/s
int cutoff = 0;      // for handling of spinner (making it run straight)
int oneshot_Freq = 3500;
int lightWidth = 15;
int speed_int = 300;  // miliseconds between speed measurements
int head_delay = 0;
float correct_max = -0.1;  // ± ratio for correct, 0.5 would mean a range from 0.5 to 1.5
const float gyro_fudge = 1;
int min_drive = 50;

// Robot stuff
const float accel_rad = 42.0 / 1000.0;  // input in mm, outputs m

// pins
const int MOTOR_RIGHT_PIN = 4;
const int MOTOR_LEFT_PIN = 3;
const int headPin = 2;  // LED heading pin
const int ledPin = 11;
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, 12, NEO_GRB + NEO_KHZ800);
const int accel_pow = 26;

// motors
RP2040_PWM *motor_Right;
RP2040_PWM *motor_Left;
float oneshot_Duty(int thoucentage);
void command_motors(int left, int right);

// RF stuff
// note channel 5 is used for shutdown so not in the list
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int CORRECT_CH = 6;
const int HEAD_MODE_CH = 7;
const int DIR_CH = 8;

bool stopflag = false;

int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
float correct = 1;
bool wep = false;

// rotation tracking
float angle = 0;
float zrotspd = 0;
int16_t xoff = 0;
int16_t yoff = 0;
int16_t zoff = 0;
float get_spin();

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

  pinMode(headPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // PowerLED stuff
  pixels.begin();
  digitalWrite(ledPin, HIGH);

  // initialise motors
  motor_Right = new RP2040_PWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(0));
  motor_Left = new RP2040_PWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(0));
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
    xl.setFullScale(LIS331::HIGH_RANGE);
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
}

void loop() {  // Loop 0 handles crsf receive and motor commands

  crsf.update();
  slip = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTothoucentage(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTothoucentage(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  correct = ((servoTothoucentage(crsf.rcToUs(crsf.getChannel(CORRECT_CH)), 1) / 1000.0) * correct_max) + 1;
  bool headMode = crsf.rcToUs(crsf.getChannel(HEAD_MODE_CH)) > 1500;
  int dir_in = map(crsf.rcToUs(crsf.getChannel(DIR_CH)), 1000, 2000, -10, 30);
  head_delay = dir_in;

  int left_sig, right_sig;

  // robot control modes
  if (spin > 0) {  // spinning mode
    if (!headMode) {  // spinning mode
      float cosresult = cos(radians(angle));
      float sinresult = sin(radians(angle));
      float delta = (-trans * (abs(cosresult) > cutoff ? cosresult : 0)) - (slip * (abs(sinresult) > cutoff ? sinresult : 0));  // minus trans because that seems to be flipped
      // spin = spin + delta > 1000 ? 1000 - trans : spin;  // reduce spin speed if delta would push them over 1000, removed for now as not really needed
      left_sig = spin + delta;
      right_sig = -spin + delta;
    } else {  // heading correct mode
      static float head_change = 0;               // var to hold heading change between loops while button is held
      if (abs(slip) > 200 || abs(trans) > 200) {  // make sure stick is a reasonable distance from centre. Otherwise the stick vibration when released gives the wrong result
        head_change = degrees(atan2(-slip, trans));
      } else {
        angle = angle - head_change;
        head_change = 0;
      }
      left_sig = spin;
      right_sig = -spin;
    }

  } else {  // normal robot mode
    digitalWrite(headPin, HIGH);
    slip = slip * 0.1;
    if (headMode) {
      angle = 0;  // allows user to press headmode button to set forwards when not spinning, lights will flash and drive will stop momentarily
      command_motors(0, 0);
      for (int i = 0; i <= 3; i++) {
        digitalWrite(headPin, LOW);
        delay(200);
        digitalWrite(headPin, HIGH);
        delay(200);
      }
    }

    // normal driving with minimum motor speed
    left_sig = slip + trans;
    left_sig = (abs(left_sig) < min_drive) ? 0 : left_sig;
    right_sig = -slip + trans;
    right_sig = (abs(right_sig) < min_drive) ? 0 : right_sig;
  }

  command_motors(left_sig, right_sig);
}


void command_motors(int left, int right) {

  // this has to be just before motor drive the rest of the loop!
  if (stopflag) {
    right = 0;
    left = 0;
  }
  motor_Left->setPWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(left));
  motor_Right->setPWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(right));
}

void loop1() {  // Loop 1 handles speed calculation and telemetry

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  static unsigned long before = 0;  // only runs first loop because static
  unsigned long now = micros();
  long looptime = static_cast<float>(now - before);
  before = now;
  // finished loop time measurement


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



  float zrot = zrotspd - (head / 3);                             // injecting head in here allows us to get a specific degrees/s
  angle = fmod(angle + (zrot * looptime / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop


  if (fmod(angle + 180, 360) > 180 + head_delay - lightWidth && fmod(angle + 180, 360) < 180 + head_delay + lightWidth) {  // check whether to flash heading LED
    digitalWrite(headPin, HIGH);
  } else {
    digitalWrite(headPin, LOW);
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    Serial.println(zrot / 6);
    lastGpsUpdate = now;
    // Update the GPS telemetry data with the new values.
    crsf.telemetryWriteGPS(0, head_delay, zrot * 6000 / 360, 0, accel_rad * 100 * correct, 0);
  }
}

int powerCurve(int x) {
  int power = 3;
  long divisor = (pow(1000, power - 1));
  return (pow(x, power)) / divisor;
}

float servoTothoucentage(int servoSignal, int stickmode) {
  // Map the servo signal to the range of -1000 to +1000, provide deazone = 1 for a channel with 1000 being default, deazone 2 for 1500 default
  int lower;
  if (stickmode == 0) {  // deadzones
    lower = 0;
    if (servoSignal <= 1000 + deadzone) {
      servoSignal = 1000;
    }
  } else if (stickmode == 1) {
    lower = -1000;
    if (servoSignal >= 1500 - deadzone && servoSignal <= 1500 + deadzone) {
      servoSignal = 1500;
    }
  }
  return map(servoSignal, 1000, 2000, lower, 1000);
}

float oneshot_Duty(int thoucentage) {  // function to turn thoucentages into oneshot pulses

  if (thoucentage > 1000) {  // cap to ± 1000
    thoucentage = 1000;
  }
  if (thoucentage < -1000) {
    thoucentage = -1000;
  }
  float oneshot_P = 1000000 / oneshot_Freq;
  float inMin = -1000.0f;
  float inMax = 1000.0f;
  float outMin = 125.0f;
  float outMax = 250.0f;  // these numbers reached experimentally by playing with speeds, I think 125 and 250 are the defaults
  float mic = outMin + (thoucentage - inMin) * (outMax - outMin) / (inMax - inMin);

  float dut = mic * 100 / oneshot_P;  //turn into percentage duty cycle
  return dut;
}


void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t linkStatistics) {
  /* Here is where you can read out the link statistics.
    You have access to the following data:
    - RSSI (dBm)
    - Link Quality (%)
    - Signal-to-Noise Ratio (dBm)
    - Transmitter Power (mW) */
  int lqi = linkStatistics.lqi;
  if (lqi < 10) {
    stopflag = true;
    Serial.println(lqi);
  } else {
    stopflag = false;
  }
}
