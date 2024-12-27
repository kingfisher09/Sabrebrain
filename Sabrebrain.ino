#include <Servo.h>
#include "LSM6DS3.h"
#include "Wire.h"

// Create an instance of the LSM6DS3 class
LSM6DS3 myIMU(I2C_MODE, 0x6A);  // I2C device address 0x6A

float rotationSum = 0;
int lightWidth = 10;

Servo Right_Motor;
Servo Left_Motor;
int angle = 0;
int speed_command = 0; //out of 1000
int translate = 0; //out of 1000
int cutoff = 0;

const int SERVO_MIN = 1000;  // Minimum servo signal
const int SERVO_MAX = 2000;  // Maximum servo signal

const int ledPin = LED_BUILTIN;
const int headPin = 10;

const int transpin = 5;
const int speedpin = 4;

float servoTothoucentage(int servoSignal) {
  // Map the servo signal to the range of -100 to +100
  return map(servoSignal, SERVO_MIN, SERVO_MAX, -1000, 1000);
}

int thoucentageToServo(float thoucentage) {
  // Map the thoucentage to the corresponding servo signal
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
  delay(5000);
}

void loop() {
  int before = micros();
  translate = servoTothoucentage(pulseIn(transpin, HIGH));
  speed_command = map(pulseIn(speedpin, HIGH), SERVO_MIN, SERVO_MAX, 0, 1000);
  int speed = speed_command + translate > 1000 ? 1000 - translate : speed_command;
  float cosresult = cos(radians(angle));
  float delta = translate * (abs(cosresult) > cutoff ? cosresult : 0);

  int left_sig = thoucentageToServo(speed + delta);
  int right_sig = thoucentageToServo(-speed + delta);
  Left_Motor.writeMicroseconds(left_sig);
  Right_Motor.writeMicroseconds(right_sig);



  angle = angle + 1;  // only used in this testing phase
  if (angle == 360) {
    angle = 0;
  }

}
