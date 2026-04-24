// Software for melty brain robot written by Owen Fisher 2024-25
#include "sabre_globals.h"

// images here:
#include "image_taunt.h"

// videos here:
#include "bouncing_pumpkin.h"
#include "sabremation.h"

int sel_video = 0;  // only used in main so no need to move

LIS331 xl;  // accelerometer thing
int g_range = 100;
void check_g_range(float reading);

CRSFforArduino crsf = CRSFforArduino(&Serial1);

/* This needs to be up here, to prevent compiler warnings. */
void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

// End Sabrescreen stuff

bool flash_now = false;  // whether currently doing a flash
// motors
BidirDShotX1* motor_Right;
BidirDShotX1* motor_Left;

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
bool headMode = false;  // remove this ASAP
int image_mode;
bool emote;
float invert = 1;

// rotation tracking
float angle = 0;                    // current robot angle
unsigned long last_angle_time = 0;  // program time when last angle was calculated
float zrotspd = 0;                  // measured speed
float zrot = 0;                     // measured speed with heading control injected
int16_t xoff = 0;
int16_t yoff = 0;
int16_t zoff = 0;
motorSpeeds motor_speeds;

// filter settings
float x = 0.3;
float a0 = 1 - x;
float b1 = x;
float prev_filt_val = 0;

void setup() {
  Serial.begin(115200);
  // Passthrough mode, keep this at the top of setup!
  if (passthrough_mode) {
    passthrough();
  }

  delay(1000);
  Serial.println("Thread 0 starting...");
  // Initialise CRSF for Arduino.
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

  delay(500);  // experimental delay to allow time for ESC boot before we start sending packets

  // initialise motors
  motor_Right = new BidirDShotX1(MOTOR_RIGHT_PIN, 300);
  motor_Left = new BidirDShotX1(MOTOR_LEFT_PIN, 300);
  motor_Left->sendThrottle(0);
  motor_Right->sendThrottle(0);

  Serial.println("Thread 0 started");
  // send throttle 0 in loop till start delay has finished
  unsigned long start = millis();
  while (millis() - start < ESC_start_delay) {
    motor_Left->sendThrottle(0);
    motor_Right->sendThrottle(0);
    delayMicroseconds(200);
  }
}

void setup1() {
  // Passthrough mode, keep this at the top of setup!
  if (passthrough_mode) {
    while (true) {
      delay(1000);
    }
  }
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
    xl.setFullScale(LIS331::LOW_RANGE);
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
}

void loop() {                    // Loop 0 handles motor commands, angle calc and updating pixels
  unsigned long now = micros();  // # timing
  static int loopcount = 0;      // # timing

  // angle calc
  zrot = zrotspd - (head * HEAD_CONTROL_SCALE) - head_trim;                     // add in head for changing angle
  angle = fmod(angle + (zrot * (now - last_angle_time) / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop
  last_angle_time = micros();

  float left_sig, right_sig;

  // robot control modes
  if (spin > 0) {  // spinning mode
    float cosresult = cos(radians(angle));
    float sinresult = sin(radians(angle));
    float delta = (TRANS_SIGN * trans * cosresult) + (SLIP_SIGN * slip * sinresult);  // calculate motor delta

    // prevent over translating which flips motor direction
    float limit = spin * MAX_DELTA;
    delta = (delta > limit) ? limit : ((delta < -limit) ? -limit : delta);

    left_sig = spin + delta;
    right_sig = -spin + delta;
    paint_screen(angle);  // update screen

  } else {  // normal robot mode

    rainbow_line();  // draw rainbow

    // normal driving with minimum motor speed
    left_sig = slip + trans;
    left_sig = (abs(left_sig) < min_drive) ? 0 : left_sig;
    right_sig = -slip + trans;
    right_sig = (abs(right_sig) < min_drive) ? 0 : right_sig;
  }

  motor_speeds = command_motors(left_sig * invert, right_sig * invert);
}

void loop1() {  // Loop 1 handles speed calculation and telemetry, also loading images
  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  unsigned long now = micros();
  static int loopcount = 0;  // # timing

  updateCRSF();  // update control
  if (speed_source == SensorType::Accelerometer) {
    if (xl.newXData()) {
      int16_t x, y, z;
      xl.readAxes(x, y, z);
      x = x + xoff;
      y = y + yoff;
      float xg = xl.convertToG(g_range, x);
      float yg = xl.convertToG(g_range, y);
      float zg = xl.convertToG(g_range, z);

      float max_g = max(max(fabs(xg), fabs(yg)), fabs(zg));
      check_g_range(max_g);

      float measure_accel =
          9.81 * sqrt(pow(xg, 2) + pow(yg, 2) + pow(zg, 2));  // given in m/s^2

      // FILTER ACCEL
      float filtered_accel = (measure_accel * a0) + (prev_filt_val * b1);
      prev_filt_val = filtered_accel;

      zrotspd = degrees(sqrt(filtered_accel / (correct * accel_rad)));  // deg/s
    }
  } else if (speed_source == SensorType::Accelerometer){
    float average_ERPM = (motor_speeds.left + motor_speeds.right)/2;  // this will need to change when I allow for single motors
    zrotspd = average_ERPM / (base_ERPM_cal * correct);
  }

    // Telemetry stuff
    static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    // Serial.println(zrot / 6);
    lastGpsUpdate = now;

    // Update the GPS telemetry data with the new values.
    
    //Telemetry depends on speed measurement mode
    float telem_calib = 0;
    if (speed_source == SensorType::Accelerometer) {
      telem_calib = accel_rad * 100 * correct;
    } else if (speed_source == SensorType::ERPM){
      telem_calib  /= base_ERPM_cal * correct;
    }
    crsf.telemetryWriteGPS(0, 0, zrotspd * 6000 / 360, 0, accel_rad * 100 * correct, 0);
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
    if (sel == 0) {
      load_vid(sabremation);
    }
    if (sel == 1) {
      load_vid(bouncing_pumpkin);
    }
    if (sel == 2) {
      load_vid(sabremation);
    }
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

void check_g_range(float reading) {
  // #AddLogging
  static int high_count = 0;
  static int low_count = 0;

  if (reading >= g_range * 0.9f) {
    high_count += 1;
    low_count = 0;
    if (high_count < 20) return;
    // increase range
    switch (g_range) {
      case 100:
        g_range = 200;
        xl.setFullScale(LIS331::MED_RANGE);
        break;

      case 200:
        g_range = 400;
        xl.setFullScale(LIS331::HIGH_RANGE);
        break;

      default:
        break;
    }

    high_count = 0;
    low_count = 0;

  } else if (reading <= g_range * 0.4f) {
    low_count += 1;
    high_count = 0;
    if (low_count < 20) return;
    // decrease range
    switch (g_range) {
      case 200:
        g_range = 100;
        xl.setFullScale(LIS331::LOW_RANGE);
        break;

      case 400:
        g_range = 200;
        xl.setFullScale(LIS331::MED_RANGE);
        break;

      default:
        break;
    }

    high_count = 0;
    low_count = 0;
  } else {
    // reset both counters
    high_count = 0;
    low_count = 0;
  }
}
