#include "sabre_globals.h"
float servoTofloat(float servoSignal, int stickmode);
int powerCurve(int x);

// E-RPM readings
uint32_t erpm_left = 0;
uint32_t erpm_right = 0;

int floatToDshot(float value) {
  // DShot 3D mode: 0 = stop, 48-1047 = reverse (48 slowest), 1049-2047 = forward (2047 fastest), 1048 = unused
    value = constrain(value, -1.0f, 1.0f);  // limits to ±1 stick commands can easily push outside of this range

    if (value == 0) return 0;
    if (value > 0) return (int)(value * 998) + 1049;  // 1049-2047
    return (int)((value + 1) * 999) + 48;             // 48-1047
}

void command_motors(float left, float right) {
  unsigned long nowish = millis();
  if (nowish - stopflag_time > E_stop_time) {  // E-stop, lost signal from transmitter
    right = 0;
    left = 0;
  } else {
    if (watchdog_enabled) {
      watchdog_update();
    } else if (nowish > E_stop_time) {    // only set watchdog after E-stop timeout has had a chance to kick in, prevents restart loop
      watchdog_enable(watchdog_time, 0);  // 0 sets mode to reboot if lost connection
      watchdog_enabled = true;
      watchdog_update();
    }
  }

  // get ERPM telemetry
  motor_Left->getTelemetryErpm(&erpm_left);
  motor_Right->getTelemetryErpm(&erpm_right);

  // Convert -1.0 to 1.0 -> 0 to 2000 which DSHOT expects
  int dshot_left = floatToDshot(left);
  int dshot_right = floatToDshot(right);

  motor_Left->sendThrottle(dshot_left);
  motor_Right->sendThrottle(dshot_right);

  Serial.println("L: " + String(left) + " DSL: " + String(dshot_left) + " R: " + String(right) + " DSR: " + String(dshot_right));
  // Serial.println("dshot_right: " + String(dshot_right));
}

float powerCurve(float x) {
  int power = 3;
  return pow(x, power);
}

float servoTofloat(float servoSignal, int stickmode) {
  // Map the servo signal to the range of -1 to +1 or 0 to 1, provide deazone
  // stickmode 0 for a channel between -1 and 1, stickmode 1 for channel between 0 and 1
  // Deadzone code modifies servo inpout signal before mapping

  if (stickmode == 0)  // Bottom deadzone
  {
    servoSignal = (servoSignal <= 1000 + deadzone) ? 1000 : servoSignal;
    return (servoSignal - 1000.0f) / 1000.0f;  // 1000-2000 -> 0 to 1
  } else if (stickmode == 1)                   // Centred deadzone
  {
    servoSignal = (servoSignal >= 1500 - deadzone && servoSignal <= 1500 + deadzone) ? 1500 : servoSignal;
    return (servoSignal - 1500.0f) / 500.0f;  // 1000-2000 -> -1 to 1
  } else {
    return 0.0f;  // invalid stickmode
  }
}

void updateCRSF() {
  // transmitter inputs
  crsf.update();

  slip = powerCurve(servoTofloat(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTofloat(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTofloat(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTofloat(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  correct = ((servoTofloat(crsf.rcToUs(crsf.getChannel(CORRECT_CH)), 1)) * -correct_max) + 1;
  headMode = crsf.rcToUs(crsf.getChannel(HEAD_MODE_CH)) > 1500;
  trimMode = crsf.rcToUs(crsf.getChannel(TRIM_CH)) > 1500;
  image_mode = crsf.rcToUs(crsf.getChannel(LIGHT_CH));
  emote = crsf.rcToUs(crsf.getChannel(EMOTE_CH)) > 1500;
  if (crsf.rcToUs(crsf.getChannel(INVERT_CH)) > 1500) {
    invert = 1;
  } else {
    invert = -1;
  }
}

void trim() {
  static bool trimming = false;
  if (trimMode) {  // only fires if trim channel active AND has not already fired
    if (!trimming && head != 0) {
      head_trim += head * HEAD_CONTROL_SCALE;
      trimming = true;
    }
  }

  if (trimming) {
    if (head == 0) {  // stick centred again
      trimming = false;
      flash();  // flash to let user know
    } else {
      head = 0;  // avoid doubling the effect
    }
  }
}

void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t linkStatistics) {
  /* Here is where you can read out the link statistics.
    You have access to the following data:
    - RSSI (dBm)
    - Link Quality (%)
    - Signal-to-Noise Ratio (dBm)
    - Transmitter Power (mW) */
  int lqi = linkStatistics.lqi;
  if (lqi > 10) {  // if link is healthy
    stopflag_time = millis();
    // Serial.println(lqi);
  }
}

float wrap360(float angle) {
  return fmodf(fmodf(angle, 360.0f) + 360.0f, 360.0f);
}

float angleDistance(float a, float b) {
  float diff = fmodf(fabsf(a - b), 360.0f);
  return diff > 180.0f ? 360.0f - diff : diff;
}