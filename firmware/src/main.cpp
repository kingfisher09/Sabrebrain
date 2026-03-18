// Software for melty brain robot written by Owen Fisher 2024-25
#include "sabre_globals.h"

// images here:
#include "image_taunt.h"

// videos here:
#include "bouncing_pumpkin.h"
#include "haloween_vid.h"
#include "sabremation.h"

int sel_video = 0; // only used in main so no need to move

LIS331 xl;  // accelerometer thing

CRSFforArduino crsf = CRSFforArduino(&Serial1);

/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// End Sabrescreen stuff

bool flash_now = false;  // whether currently doing a flash
// motors
RP2040_PWM* motor_Right;
RP2040_PWM* motor_Left;

// safety stuff
unsigned long stopflag_time = 0;
int E_stop_time = 100;          // ms allowed between ELRS signals before shutting down motors
int watchdog_time = 1000;       // ms after E_stop before resetting MCU
bool watchdog_enabled = false;  // bool to record watchdog status. Watchdog will be enabled when transmitter first sends data, MCU will restart 1s after estop if no more signals are received

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
float correct = 1;
bool headMode;
bool trimMode = false;
float head_trim = 0;
int image_mode;
bool emote;
float invert  = 1;

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
  load_vid(haloween_vid);
}

void loop() {                    // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // angle calc
  zrot = zrotspd - (head * HEAD_CONTROL_SCALE) - head_trim;                     // add in head for changing angle
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
      left_sig = spin * invert;
      right_sig = -spin * invert;
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
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
                // At the moment, speed will not be calculated if we always have a compass reading available, this could mean we don't get anything telemetry wise at low speed

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # timing

  updateCRSF();  // update control
  trim();

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
    float xg = xl.convertToG(200, x);
    float yg = xl.convertToG(200, y);
    float zg = xl.convertToG(200, z);

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
    if (sel == 0) { load_vid(haloween_vid); }
    if (sel == 1) { load_vid(bouncing_pumpkin); }
    if (sel == 2) { load_vid(sabremation); }
    sel_video = sel;
  }

  if (!emote) {
    // play annimation
    load_frame();
  } else {
    memcpy(current_frame, image_taunt, sizeof(image_taunt));
    frame_num = -1;  // I don't like this, would be nicer to have a function to start playing vid
  }

  // flash annimation
  if (flash_now) {
    flashing();
  }
}
