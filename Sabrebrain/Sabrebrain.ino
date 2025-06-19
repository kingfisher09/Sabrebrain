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

/* Assign a unique ID to this sensor at the same time */
Adafruit_MMC5603 mmc = Adafruit_MMC5603(12345);

LIS331 xl;  // accelerometer thing

CRSFforArduino crsf = CRSFforArduino(&Serial1);

/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// settings
int deadzone = 30;   // for transmitter sticks
int max_head = 360;  // max heading change in deg/s
int cutoff = 0;      // for handling of spinner (making it run straight), this seems to be unused
int oneshot_Freq = 3500;
int speed_int = 300;       // miliseconds between speed measurements
int head_delay = 17;       // 17 seems good for v4, may need to be adjusted in future
float correct_max = -0.2;  // ± ratio for radial correct, 0.5 would mean a range from 0.5 to 1.5
int min_drive = 80;
const int rainbow_delay = 40;

// Robot stuff
const float accel_rad = 74.0 / 1000.0;  // input in mm, outputs m
#define RIGHT_MOTOR_DIRECTION -1
#define LEFT_MOTOR_DIRECTION -1

// pins
const int MOTOR_RIGHT_PIN = 4;
const int MOTOR_LEFT_PIN = 3;
#define LED_POWER_PIN 11  //  builtin LED Power control pin
#define LED_PIN 12        // Data pin for NeoPixel
const int headPin = 27;   // LED heading pin

// Sabrescreen stuff
const int NUM_LEDS = 23;
const float NUM_SLICES = 150;  // the 3 slice settings all need to be float for the calculations to work
const float slice_size = 360 / NUM_SLICES;
const float half_slice = slice_size / 2;

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
// note channel 5 is used for shutdown so not in the list
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int CORRECT_CH = 6;
const int HEAD_MODE_CH = 7;
const int DIR_CH = 8;
const int MAG_CH = 9;     // used for deactivating magnetometer
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
float zrot = 0;
float spin = 0;
float correct = 1;
bool wep = false;

// rotation tracking
float angle = 0;  // current robot angle
float zrotspd = 0;
int16_t xoff = 0;
int16_t yoff = 0;
int16_t zoff = 0;

// filter settings
float x = 0.3;
float a0 = 1 - x;
float b1 = x;
float prev_filt_val = 0;

// testing stuff:
float test_angle = 0;
bool test_mode = false;


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
  if (!mmc.begin(MMC56X3_DEFAULT_ADDRESS, &Wire)) {  // I2C mode
    Serial.println("Ooops, no MMC5603 detected ... Check your wiring!");
    while (1) delay(10);

    memcpy(LEDframe, image_pointer, sizeof(image_pointer));  // set the default image in memory
  }

  /* Display some basic information on this sensor */
  mmc.printSensorDetails();

  mmc.setDataRate(1000);  // in Hz, from 1-255 or 1000
  mmc.setContinuousMode(true);

  Serial.println("Thread 1 started");
  xoff = 10;
  yoff = 10;
}

void loop() {                           // Loop 0 handles crsf receive and motor commands, also updating pixels
  unsigned long start_time = micros();  // # timing
  unsigned long first_time = start_time;
  unsigned long newtime;  // # timing

  unsigned long after_calcs = 0;  // # timing
  unsigned long after_paint = 0;  // # timing
  unsigned long after_copy = 0;         // # timing
  static int loopcount = 0;       // # timing

  crsf.update();

  newtime = micros();  // # <timing
  unsigned long after_crsf = newtime - start_time;
  start_time = newtime;  // # timing>

  slip = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTothoucentage(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTothoucentage(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  correct = ((servoTothoucentage(crsf.rcToUs(crsf.getChannel(CORRECT_CH)), 1) / 1000.0) * correct_max) + 1;
  bool headMode = crsf.rcToUs(crsf.getChannel(HEAD_MODE_CH)) > 1500;
  int dir_in = map(crsf.rcToUs(crsf.getChannel(DIR_CH)), 1000, 2000, -5, 5);
  head_delay = dir_in;

  newtime = micros();  // # <timing
  unsigned long after_crsfprocess = newtime - start_time;
  start_time = newtime;  // # timing>

  int left_sig, right_sig;

  // robot control modes
  if (spin > 0) {     // spinning mode
    if (!headMode) {  // spinning mode
      float cosresult = cos(radians(angle));
      float sinresult = sin(radians(angle));
      float delta = (-trans * (abs(cosresult) > cutoff ? cosresult : 0)) - (slip * (abs(sinresult) > cutoff ? sinresult : 0));  // minus trans because that seems to be flipped
      left_sig = spin + delta;
      right_sig = -spin + delta;

      newtime = micros();  // # <timing
      after_calcs = newtime - start_time;
      start_time = newtime;  // # timing>

      // paint_screen(angle);  // update screen


      // <<<<<<<<< paint screen here
      int current_line = fmod(floor((angle + half_slice) / slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0
      memcpy(leds, image_pointer[current_line], sizeof(leds));                           // write line of LEDs to the LED array

      newtime = micros();  // # <timing
      after_copy = newtime - start_time;
      start_time = newtime;  // # timing>

      // FastLED.clear();
      FastLED.show();

      // paint screen here >>>>>>>>>>

      newtime = micros();  // # <timing
      after_paint = newtime - start_time;
      start_time = newtime;  // # timing>

    } else {                                      // heading correct mode
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

  } else {              // normal robot mode
    rainbow_line();     // draw rainbow
    slip = slip * 0.1;  // reduce turning speed
    if (headMode) {
      angle = 0;  // allows user to press headmode button to set forwards when not spinning, lights will flash and drive will stop momentarily
      for (int i = 0; i <= 3; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::White);  // FastLED built-in function
        FastLED.show();
        delay(300);
        FastLED.clear();
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

  newtime = micros();  // # <timing
  unsigned long after_motors = newtime - start_time;
  start_time = newtime;  // # timing>

  // # timing
  if (loopcount == 200) {
    Serial.print("Crossfire read: ");
    Serial.println(after_crsf);

    Serial.print("Crossfire process: ");
    Serial.println(after_crsfprocess);

    Serial.print("trans calcs: ");
    Serial.println(after_calcs);

    Serial.print("Copying: ");
    Serial.println(after_copy);

    Serial.print("painting: ");
    Serial.println(after_paint);

    Serial.print("trans motors: ");
    Serial.println(after_motors);
    Serial.println(start_time - first_time);
    Serial.println();
    loopcount = 0;
  } else {
    loopcount += 1;
  }
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
                // At the moment, speed will not be calculated if we always have a compass reading available, this could mean we don't get anything telemetry wise at low speed

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  static unsigned long before = 0;  // only runs first loop because static
  unsigned long now = micros();
  long looptime = static_cast<float>(now - before);
  before = now;
  // finished loop time measurement

  bool mag_angle = false;
  // if (mag_speed_calc) {
  //   uint8_t status = mmc.readRegister(0x18);  // Read STATUS register
  //   if (status & 0x01) {                      // Check DRDY bit
  //     mag_angle = true;
  //   }
  // }

  if (mag_angle) {
    //register check written by chatGPT, test it works:
    // Check DRDY bit
    /* Get a new sensor even */
    sensors_event_t event;
    mmc.getEvent(&event);

    // Calculate the angle of the vector y,x
    float heading = (atan2(event.magnetic.y, event.magnetic.x) * 180) / PI;

    // Normalize to 0-360
    if (heading < 0) {
      heading = 360 + heading;
    }

    angle = heading + mag_offset;
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
    zrot = zrotspd - (head / 3);                                   // injecting head in here allows us to get a specific degrees/s
    angle = fmod(angle + (zrot * looptime / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop
  }

  // Telemetry stuff
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    // Serial.println(zrot / 6);
    lastGpsUpdate = now;
    // Update the GPS telemetry data with the new values.
    crsf.telemetryWriteGPS(0, head_delay, zrot * 6000 / 360, 0, accel_rad * 100 * correct, 0);
  }
}
