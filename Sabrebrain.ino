#include "CRSFforArduino.hpp"
#include "RP2040_PWM.h"
#include <Servo.h>

CRSFforArduino crsf = CRSFforArduino(&Serial1);

// settings
int deadzone = 30;
int max_head = 360;  // max heading change in deg/s
int cutoff = 0;      // for handling of spinner (making it run straight)
int oneshot_Freq = 3500;
int lightWidth = 15;
int speed_int = 300;  // miliseconds between speed measurements
int head_delay = 0;
float correct_max = 0.8;  // ± ratio for correct, 0.5 would mean a range from 0.5 to 1.5

// Robot stuff
const int wheel_dia = 50;      // mm
float wheel_pos_dia = 83 * 2;  // mm
const int max_rps = 3.8 * 4 * 330 / 60;
// max wheel speed given by max_rps * wheel_dia / wheel_pos_dia

// pins
const int MOTOR_RIGHT_PIN = 15;
const int MOTOR_LEFT_PIN = 25;
const int headPin = 5;  // LED heading pin
const int ledPin = LED_BUILTIN;
const int ServoPin = 0;  // this needs to be decided

// motors
RP2040_PWM *motor_Right;
RP2040_PWM *motor_Left;
float oneshot_Duty(int thoucentage);

// RF stuff
// note channel 5 is used for shutdown so not in the list
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int CORRECT_CH = 6;
const int HEAD_MODE_CH = 7;

int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);

// movement commands
float slip = 0;
float trans = 0;
float head = 0;
float spin = 0;
float correct = 1;

// rotation tracking
float angle = 0;
bool pauseflag = false;


void setup() {
  // put your setup code here, to run once:
  // Initialise CRSF for Arduino.

  if (!crsf.begin()) {
    Serial.println("CRSF for Arduino initialisation failed!");
    while (1) {
      delay(10);
    }
  }

  pinMode(headPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  // initialise motors
  Serial.begin(115200);
  motor_Right = new RP2040_PWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(0));
  motor_Left = new RP2040_PWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(0));
  Serial.println("Thread 0 started");
  delay(3000);  // wait for ESCs to start up
}

void setup1() {
  Serial.println("Thread 1 started");
}

void loop() {  // Loop 0 handles crsf receive and motor commands

  crsf.update();
  slip = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTothoucentage(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTothoucentage(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  correct = ((servoTothoucentage(crsf.rcToUs(crsf.getChannel(CORRECT_CH)), 1) / 1000.0) * correct_max) + 1;
  bool headMode = crsf.rcToUs(crsf.getChannel(HEAD_MODE_CH)) > 1500;

  int left_sig, right_sig;

  // robot control modes
  if (spin > 0) {  // spinning mode
    if (pauseflag) {
      left_sig = slip + trans;
      right_sig = -slip + trans;
    } else if (!headMode) {
      float cosresult = cos(radians(angle));
      float sinresult = sin(radians(angle));
      float delta = (trans * (abs(cosresult) > cutoff ? cosresult : 0)) - (slip * (abs(sinresult) > cutoff ? sinresult : 0));
      spin = spin + delta > 1000 ? 1000 - trans : spin;  // reduce spin speed if delta would push them over 1000
      left_sig = spin + delta;
      right_sig = -spin + delta;
    } else {
      static float head_change = 0;               // var to hold heading change between loops while button is held
      if (abs(slip) > 200 || abs(trans) > 200) {  // make sure stick is a reasonable distance from centre. Otherwise the stick vibration when released gives the wrong result
        head_change = degrees(atan2(-slip, trans));
      } else {
        angle = angle + head_change;
        head_change = 0;
      }
      left_sig = spin;
      right_sig = -spin;
    }
  } else {  // normal robot mode
    digitalWrite(headPin, HIGH);
    left_sig = slip + trans;
    right_sig = -slip + trans;
  }
  motor_Right->setPWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(right_sig));
  motor_Left->setPWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(left_sig));
}

void loop1() {  // Loop 1 handles speed calculation

  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  static unsigned long before = 0;  // only runs first loop because static
  unsigned long now = micros();
  long looptime = static_cast<float>(now - before);
  before = now;
  // finished loop time measurement

  float zrotCalc = spin * correct * max_rps * 360 * wheel_dia / (wheel_pos_dia * 1000);  // deg/s
  Serial.println(spin * max_rps * 60/ 1000);

  float zrot = zrotCalc + (head / 3);                            // injecting head in here allows us to get a specific degrees/s
  angle = fmod(angle + (zrot * looptime / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop


  if (fmod(angle + 180, 360) > 180 + head_delay - lightWidth && fmod(angle + 180, 360) < 180 + head_delay + lightWidth) {  // check whether to flash heading LED
    digitalWrite(headPin, HIGH);
  } else {
    digitalWrite(headPin, LOW);
  }


  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    lastGpsUpdate = now;
    // Update the GPS telemetry data with the new values.
    crsf.telemetryWriteGPS(0, 0, zrotCalc * 6000 / 360, 0, 0, 0);
  }
}

int powerCurve(int x) {
  int power = 3;
  long divisor = (pow(1000, power - 1));
  return (pow(x, power)) / divisor;
}

float servoTothoucentage(int servoSignal, int stickmode) {
  // Map the servo signal to the range of -100 to +100, provide deazone = 1 for a channel with 1000 being default, deazone 2 for 1500 default
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

// float readTelemRPM() {
//   if (Serial1.available() >= 10) {  // Check if all 10 bytes are available
//     // Read the 10 bytes from the serial buffer
//     byte temperature = Serial1.read();
//     byte voltageHigh = Serial1.read();
//     byte voltageLow = Serial1.read();
//     byte currentHigh = Serial1.read();
//     byte currentLow = Serial1.read();
//     byte consumptionHigh = Serial1.read();
//     byte consumptionLow = Serial1.read();
//     byte rpmHigh = Serial1.read();
//     byte rpmLow = Serial1.read();
//     byte crc = Serial1.read();

//     // Combine high and low bytes to reconstruct values
//     int voltage = (voltageHigh << 8) | voltageLow;
//     int current = (currentHigh << 8) | currentLow;
//     int consumption = (consumptionHigh << 8) | consumptionLow;
//     int rpm = (rpmHigh << 8) | rpmLow;

//     // Print the parsed values to the Serial Monitor

//     float readRPM = rpm * 8.3333;


//     // Optionally, you can perform CRC validation here
//   }
// }
