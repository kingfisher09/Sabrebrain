#include "sabre_globals.h"

float servoTofloat(float servoSignal, int stickmode);
int powerCurve(int x);

// E-RPM readings
uint32_t erpm_left = 0;
uint32_t erpm_right = 0;

int floatToDshot(float value) {
  // This function maps -1 to 1 -> 1-1000 (reverse speed, 1 is slow) and 1001 to 2000 (forward speed)
  // May want to make these #advancedusersetting at some point but likely not

  value = constrain(value, -1.0f, 1.0f);

  const int revMin = 50;
  const int revMax = 1000;  // your test upper limit for reverse
  const int fwdMin = 1050;
  const int fwdMax = 2000;

  if (value == 0) {
    return 0;  // or 1048 if you later want true neutral instead of stop
  }

  if (value < 0.0f) {
    // Map -1.0 → revMax, 0.0- → revMin
    return revMin + (int)((-value) * (revMax - revMin));
  } else {
    // Map 0.0+ → fwdMin, +1.0 → fwdMax
    return fwdMin + (int)(value * (fwdMax - fwdMin));
  }
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

  // Guard against oversending motor updates
  static unsigned long lastMotorUpdate = 0;
  unsigned long now = micros();
  if (now - lastMotorUpdate < dshot_delay) return;

  // Convert -1.0 to 1.0 -> DSHOT signals, flip direction as desired
  int dshot_left = floatToDshot(left * LEFT_MOTOR_DIRECTION);
  int dshot_right = floatToDshot(right * RIGHT_MOTOR_DIRECTION);

  motor_Left->sendThrottle(dshot_left);
  motor_Right->sendThrottle(dshot_right);
  lastMotorUpdate = now;

  // Userful for debugging, commented for speed of running
  // Serial.println("L: " + String(left, 3) + " DSL: " + String(dshot_left) + " ERPM_L: " + String(erpm_left) + " | R: " + String(right, 3) + " DSR: " + String(dshot_right) + " ERPM_R: " + String(erpm_right));
  
  if (!desync_detection) return;
  
  static bool desyncing = false;
  if ((erpm_left > 63000 && fabs(left) < 0.001f) || ((erpm_right > 63000 && fabs(right) < 0.001f))) {
    static uint32_t start_desync = 0;
    if (!desyncing){
      desyncing = true;
      start_desync = nowish;
    }
    if (nowish - start_desync > desync_detect_time) {
      Serial.println("Desync");
      watchdog_reboot(0, 0, 1);
      while (true) {};
    }
  } else {
    desyncing = false;
  }
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
  image_mode = crsf.rcToUs(crsf.getChannel(IMAGE_CH));
  emote = crsf.rcToUs(crsf.getChannel(EMOTE_CH)) > 1500;
  if (crsf.rcToUs(crsf.getChannel(INVERT_CH)) > 1500) {
    invert = 1;
  } else {
    invert = -1;
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