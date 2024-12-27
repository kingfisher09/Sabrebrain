#include <Servo.h>
#include "LSM6DS3.h"
#include "Wire.h"

// Create an instance of the LSM6DS3 class
LSM6DS3 myIMU(I2C_MODE, 0x6A);  // I2C device address 0x6A

const int ledPin = LED_BUILTIN;
const int headPin = 9;

// slipChan = 2;
// transChan = 3;
// spinChan = 4;
// headChan = 5;

float rotationSum = 0;
int lightWidth = 15;

unsigned long before = millis();

Servo Right_Motor;
Servo Left_Motor;
float angle = 0;

int nextChan = 2;
int spin_command = 0;  //out of 1000
int translate = 0;     //out of 1000
int slip = 0;           //out of 1000
float headChange = 0;
int cutoff = 0;

const int SERVO_MIN = 1000;  // Minimum servo signal
const int SERVO_MAX = 2000;  // Maximum servo signal

int powerCurve(int x){
   int power = 3;
  long divisor = (pow(1000, power - 1));
  return (pow(x, power))/divisor;
}

void read_chans() {  // cycles through the 4 channels reading updating a different one each tim it is called

  int readSig = pulseIn(nextChan, HIGH, 10000);

  int deadzone = 30;
  if (nextChan == 4) {  // deadzones
    if (readSig <= 1000 + deadzone) {
      readSig = 1000;
    }
  } else {
    if (readSig >= 1500 - deadzone && readSig <= 1500 + deadzone) {
      readSig = 1500;
    }
  }

  if (readSig == 0) {  // if a wire comes unplugged stop!
    Left_Motor.writeMicroseconds(1500);
    Right_Motor.writeMicroseconds(1500);
    digitalWrite(LED_BUILTIN, LOW);
    while (true) {
      delay(1000);
    }
  }

  if (nextChan == 2) {
    slip = powerCurve(servoTothoucentage(readSig));
    nextChan = 3;
  } else if (nextChan == 3) {
    translate = powerCurve(servoTothoucentage(readSig));
    nextChan = 4;
  } else if (nextChan == 4) {
    spin_command = map(readSig, SERVO_MIN, SERVO_MAX, 0, 1000);
    nextChan = 5;
  } else if (nextChan == 5) {
    headChange = map(readSig, SERVO_MIN, SERVO_MAX, -2, 2);
    nextChan = 2;
  }
}

float servoTothoucentage(int servoSignal) {
  // Map the servo signal to the range of -100 to +100
  return map(servoSignal, SERVO_MIN, SERVO_MAX, -1000, 1000);
}

int thoucentageToServo(float thoucentage) {
  // Map the thoucentage to the corresponding servo signal
  if (thoucentage > 1000){  // cap to ± 1000
    thoucentage = 1000;
  }
  if (thoucentage < -1000){
    thoucentage = -1000;
  }
  return map(thoucentage, -1000, 1000, SERVO_MIN, SERVO_MAX);
}

void setup() {
  // put your setup code here, to run once:
  Right_Motor.attach(0);
  Right_Motor.writeMicroseconds(1500);  // Corrected the typo here
  Left_Motor.attach(1);
  Left_Motor.writeMicroseconds(1500);

  pinMode(ledPin, OUTPUT);
  pinMode(headPin, OUTPUT);

  Serial.begin(9600);
  Serial.println("Starting up");
  digitalWrite(headPin, HIGH);
  delay(5000);
  digitalWrite(headPin, LOW);
  delay(500);


  // Call .begin() to configure the IMU
  if (myIMU.begin() != 0) {
    Serial.println("Device error");
  } else {
    Serial.println("Device OK!");
  }
}

void loop() {
  read_chans();

  if (spin_command > 0) {  // spinning mode

    float looptime = static_cast<float>(millis() - before);

    before = millis();  // for measuring looptime

    float zrotReading = myIMU.readFloatGyroZ() + 0.07;
    angle = fmod(angle + (zrotReading * looptime / 1000) + 360 + headChange, 360);  // will not work if rotate more than 360° negative per loop

    Serial.print("Looptime: ");
    Serial.println(looptime);
    Serial.println(angle);

    if (angle < lightWidth || angle > (360 - lightWidth)) {  // check whether to flash heading LED
      digitalWrite(headPin, HIGH);
    } else {
      digitalWrite(headPin, LOW);
    }


    int spin = spin_command + translate > 1000 ? 1000 - translate : spin_command;  // set motor spin speed
    float cosresult = cos(radians(angle));
    float sinresult = sin(radians(angle));
    float delta = (translate * (abs(cosresult) > cutoff ? cosresult : 0)) - (slip * (abs(sinresult) > cutoff ? sinresult : 0));

    int left_sig = thoucentageToServo(spin + delta);
    int right_sig = thoucentageToServo(-spin + delta);
    Serial.print("Right motor:");
    Serial.print(right_sig);
    Serial.print(" Left motor: ");
    Serial.println(left_sig);

    Left_Motor.writeMicroseconds(left_sig);
    Right_Motor.writeMicroseconds(right_sig);

  } else {  // normal robot mode
    digitalWrite(headPin, HIGH);
    int left_sig = thoucentageToServo(slip + translate);
    int right_sig = thoucentageToServo(-slip + translate);
    Serial.print("Right motor:");
    Serial.print(right_sig);
    Serial.print(" Left motor: ");
    Serial.println(left_sig);

    Left_Motor.writeMicroseconds(left_sig);
    Right_Motor.writeMicroseconds(right_sig);
  }
}
