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

motorSpeeds command_motors(float left, float right) {
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
  motorSpeeds return_speeds;
  return_speeds.left = erpm_left;
  return_speeds.right = erpm_right;

  // Guard against oversending motor updates
  static unsigned long lastMotorUpdate = 0;
  unsigned long now = micros();
  if (now - lastMotorUpdate < dshot_delay) return return_speeds;

  // Convert -1.0 to 1.0 -> DSHOT signals, flip direction as desired
  int dshot_left = floatToDshot(left * LEFT_MOTOR_DIRECTION);
  int dshot_right = floatToDshot(right * RIGHT_MOTOR_DIRECTION);

  motor_Left->sendThrottle(dshot_left);
  motor_Right->sendThrottle(dshot_right);
  lastMotorUpdate = now;

  if (!desync_detection) return return_speeds;

  static bool desyncing = false;
  if ((erpm_left > 63000 && fabs(left) < 0.001f) || ((erpm_right > 63000 && fabs(right) < 0.001f))) {
    static uint32_t start_desync = 0;
    if (!desyncing) {
      desyncing = true;
      start_desync = nowish;
    }
    if (nowish - start_desync > desync_detect_time) {
      Serial.println("Desync");
      watchdog_reboot(0, 0, 1);
      while (true) {
      };
    }
  } else {
    desyncing = false;
  }
  return return_speeds;  // would be nice to clean up the returns throughout here
}

// Commented  these out as I can't find a use anywhere:

// float wrap360(float angle) {
//   return fmodf(fmodf(angle, 360.0f) + 360.0f, 360.0f);
// }

// float angleDistance(float a, float b) {
//   float diff = fmodf(fabsf(a - b), 360.0f);
//   return diff > 180.0f ? 360.0f - diff : diff;
// }