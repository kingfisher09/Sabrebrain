void command_motors(int left, int right) {

  if (stopflag) {  // E-stop
    right = 0;
    left = 0;
  }
  motor_Left->setPWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(left, LEFT_MOTOR_DIRECTION));
  motor_Right->setPWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(right, RIGHT_MOTOR_DIRECTION));
}

int powerCurve(int x) {
  int power = 3;
  long divisor = (pow(1000, power - 1));
  return (pow(x, power)) / divisor;
}

float servoTothoucentage(int servoSignal, int stickmode) {
  // Map the servo signal to the range of -1000 to +1000 or 0 to 1000, provide deazone
  // stickmode 0 for a channel between -1000 and 1000, 1 for channel between 0 and 1000

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

float oneshot_Duty(int thoucentage, int dir_flip) {  // function to turn thoucentages into oneshot pulses
  if (abs(dir_flip) != 1) {                          // check dirlfip is ±1
    Serial.println("dirflip set incorrectly in motor settup");
    while (true) {
      delay(1000);
    }
  }

  thoucentage = thoucentage * dir_flip;  // apply direction flip

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

void updateCRSF() {
  // transmitter inputs
  crsf.update();

  slip = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTothoucentage(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTothoucentage(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTothoucentage(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  correct = ((servoTothoucentage(crsf.rcToUs(crsf.getChannel(CORRECT_CH)), 1) / 1000.0) * correct_max) + 1;
  headMode = crsf.rcToUs(crsf.getChannel(HEAD_MODE_CH)) > 1500;
  head_delay = map(crsf.rcToUs(crsf.getChannel(DIR_CH)), 1000, 2000, -5, 5);
  mag_speed_calc = crsf.rcToUs(crsf.getChannel(MAG_CH)) < 1500;  // turn mag on or off
  // flip_rot_direction = crsf.rcToUs(crsf.getChannel(MAG_CH)) < 1500;  // turn mag on or off
}

void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t linkStatistics) {
  /* Here is where you can read out the link statistics.
    You have access to the following data:
    - RSSI (dBm)
    - Link Quality (%)
    - Signal-to-Noise Ratio (dBm)
    - Transmitter Power (mW) */
  int lqi = linkStatistics.lqi;
  if (lqi < 10) {  // if link has dropped
    stopflag = true;
    // Serial.println(lqi);
  } else {
    stopflag = false;
  }
}

float read_mag() {
  /* Get a new sensor event */
  sensors_event_t event;
  mmc.getEvent(&event);

  // Calculate the angle of the vector y,x
  float heading = (atan2(event.magnetic.y, event.magnetic.x) * 180) / PI;

  // Normalize to 0-360
  heading = fmod(heading + 360, 360);

  if (flip_rot_direction) {
    heading = 360 - heading;
  }
  return heading;
}

float wrap360(float angle) {
  return fmodf(fmodf(angle, 360.0f) + 360.0f, 360.0f);
}

float angleDistance(float a, float b) {
  float diff = fmodf(fabsf(a - b), 360.0f);
  return diff > 180.0f ? 360.0f - diff : diff;
}