#include <Arduino_LSM6DSOX.h>
#include <ArduinoBLE.h>  // Bluetooth Library
#include <CrsfSerial.h>
#include "RP2040_PWM.h"

// settings
#define rotAxis z  // allows for easy axis
int deadzone = 30;
int max_head = 360;  // max heading change in deg/s
int cutoff = 0;      // for handling of spinner (making it run straight)
int oneshot_Freq = 3500;
int lightWidth = 15;
const int head_delay = -15; // used to allign the heading light with direction of movement
const float gyro_fudge = 1.142;


// pins
const int MOTOR_RIGHT_PIN = 15;
const int MOTOR_LEFT_PIN = 25;
const int headPin = 5;
const int ledPin = LED_BUILTIN;

// motors
RP2040_PWM *motor_Right;
RP2040_PWM *motor_Left;
float oneshot_Duty(int thoucentage);

// RF stuff
CrsfSerial crsf(Serial1, CRSF_BAUDRATE);
// note channel 5 is used for shutdown so not in the list
const int SLIP_CH = 1;
const int TRANS_CH = 2;
const int SPIN_CH = 3;
const int HEAD_CH = 4;
const int WEAP_CH = 6;
const int HEAD_MODE_CH = 7;
const int ATTACK_CH = 8;

// BLE stuff
#define BLE_UUID_FLOAT_VALUE "C8F88594-2217-0CA6-8F06-A4270B675D69"
BLEService customService("Gyro");
// Syntax: BLE<DATATYPE>Characteristic <NAME>(<UUID>, <PROPERTIES>, <DATA LENGTH>)
BLEStringCharacteristic ble_gyro(BLE_UUID_FLOAT_VALUE, BLENotify, 9);

int powerCurve(int x);
float servoTothoucentage(int servoSignal, int stickmode);
float get_spin();

// movement commands
float slip = 0;
float trans = 0;
float head = 0;

// rotation tracking
float angle = 0;

bool stopflag = false;

// remove after debug
int loopcount = 0;

void setup() {
  delay(1000);
  // put your setup code here, to run once:
  crsf.begin();
  pinMode(headPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  // initialise motors
  Serial.begin(115200);
  motor_Right = new RP2040_PWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(0));
  motor_Left = new RP2040_PWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(0));
  delay(3000);  // wait for ESCs to start up
}

void setup1() {
  // put your setup code here, to run once:
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    stopflag = true;
    while (1)
      ;
  }
  // if (!BLE.begin()) {
  //   Serial.println("BLE failed to Initiate");
  //   delay(500);
  //   while (1)
  //     ;
  // }
  // BLE.setLocalName("Sabretooth");
  // BLE.setAdvertisedService(customService);
  // customService.addCharacteristic(ble_gyro);
  // BLE.addService(customService);
  // BLE.advertise();
}

void loop() {
  while(stopflag){
    digitalWrite(ledPin, HIGH);    
  }
  crsf.loop();  // required for ELRS to work
  slip = powerCurve(servoTothoucentage(crsf.getChannel(SLIP_CH), 1));
  trans = powerCurve(servoTothoucentage(crsf.getChannel(TRANS_CH), 1));
  float spin = servoTothoucentage(crsf.getChannel(SPIN_CH), 0);
  head = servoTothoucentage(crsf.getChannel(HEAD_CH), 1);
  float weap = servoTothoucentage(crsf.getChannel(WEAP_CH), 1);
  bool headMode = crsf.getChannel(HEAD_MODE_CH) > 1500;
  bool attack = crsf.getChannel(ATTACK_CH) > 1500;

  int left_sig, right_sig;

  // if (loopcount == 50) {
  //   Serial.println(angle);  // remove this after debug!
  //   loopcount = 0;
  // } else {
  //   loopcount = loopcount + 1;
  // }

  // robot control modes
  if (spin > 0) {  // spinning mode
    if (!headMode) {
      float cosresult = cos(radians(angle));
      float sinresult = sin(radians(angle));
      float delta = (trans * (abs(cosresult) > cutoff ? cosresult : 0)) - (slip * (abs(sinresult) > cutoff ? sinresult : 0));
      spin = spin + delta > 1000 ? 1000 - trans : spin;  // reduce spin speed if delta would push them over 1000
      left_sig = spin + delta;
      right_sig = -spin + delta;
    } else { 
      static float head_change = 0;  // var to hold heading change between loops while button is held
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

void loop1() {
  // loop time measurement. Could be moved to separate function but if it was accessed by the other thread everything would break
  static unsigned long before = 0;  // only runs first loop because static
  unsigned long now = micros();
  long looptime = static_cast<float>(now - before);
  before = now;
  // finished loop time measurement
  
  float zrotReading = get_spin();
  float zrot = zrotReading + (head / 3);                                // injecting head in here allows us to get a specific degrees/s
  angle = fmod(angle + (zrot * looptime / 1000000) + 360, 360);  // will not work if rotate more than 360° negative per loop


  // BLEDevice central = BLE.central();  // this seems important though I don't know why
  Serial.println(looptime);
  // static bool sendBLE = true;
      Serial.print("angle: ");
      Serial.println(angle);
  if (fmod(angle + 180, 360) > 180 +  head_delay - lightWidth && fmod(angle + 180, 360) < 180 + head_delay + lightWidth) {  // check whether to flash heading LED
    digitalWrite(headPin, HIGH);

  } else {
    digitalWrite(headPin, LOW);
  }
}

float get_spin() {
  while (!IMU.gyroscopeAvailable()) {
    ;
  }
  float x, y, z;
  IMU.readGyroscope(x, y, z);
  static float spin = 0;
  float spin_measure = hypot(x, y, z) * constrain(z, -1, 1);
  if (spin_measure < 9999) {  // occasionally the IMU reads inf... not sure why but this seems to catch it
    spin = spin_measure;
  }

  return spin * gyro_fudge;
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
  float outMin = 123.6f;
  float outMax = 246.6f;  // these numbers reached experimentally by playing with speeds
  float mic = outMin + (thoucentage - inMin) * (outMax - outMin) / (inMax - inMin);

  float dut = mic * 100 / oneshot_P;  //turn into percentage duty cycle
  return dut;
}