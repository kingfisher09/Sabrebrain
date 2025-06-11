void command_motors(int left, int right) {

  if (stopflag) {  // E-stop
    right = 0;
    left = 0;
  }
  motor_Left->setPWM(MOTOR_LEFT_PIN, oneshot_Freq, oneshot_Duty(left));
  motor_Right->setPWM(MOTOR_RIGHT_PIN, oneshot_Freq, oneshot_Duty(right));
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
  if (lqi < 10) {  // if link has dropped
    stopflag = true;
    Serial.println(lqi);
  } else {
    stopflag = false;
  }
}