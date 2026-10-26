// Drives one or two motors via DShot, CRSF, or OneShot, with startup, E-stop, and watchdog handling.

#include "sabre_motor_control.h"
#include "sabre_globals.h"
#include "sabre_radio.h"
#include "sabre_crsf_output.h"
#include "sabre_oneshot_output.h"
#include <RP2040_PWM.h>

// Shared state.
static MotorConfig active_config{};
static bool motors_initialised = false;
static unsigned long motor_init_time = 0;
static bool motor_startup_pending = false;

static bool watchdog_enabled = false;  // bool to record watchdog status. Watchdog will be enabled when transmitter first sends data, MCU will restart 1s after estop if no more signals are received

// DShot state.
static BidirDShotX1* motor_Right;
static BidirDShotX1* motor_Left;
// E-RPM readings
static uint32_t erpm_left = 0;
static uint32_t erpm_right = 0;

// CRSF state.
static CrsfDmaOutput motor_crsf;
static uint8_t crsf_frame[sabre_crsf::frame_size];
static unsigned long last_crsf_update = 0;
static bool crsf_frame_started = false;

// OneShot state. Hardware PWM repeats pulses without servicing the loop.
static RP2040_PWM* oneshot_left;
static RP2040_PWM* oneshot_right;

// Protocol helpers, in the same order as the implementations below.
static bool setup_dshot(const MotorConfig& config);
static motorSpeeds command_dshot(float left, float right);
static int floatToDshot(float value);

static bool setup_crsf(const MotorConfig& config);
static motorSpeeds command_crsf(float left, float right, const MotorConfig& config);

static bool setup_oneshot(const MotorConfig& config);
static motorSpeeds command_oneshot(float left, float right);
static motorSpeeds command_outputs(float left, float right);

// Public motor control interface.
bool setup_motors(const MotorConfig& config) {
  active_config = config;
  // Protocol helpers treat A as logical left and B as logical right internally.
  // Keep the stored physical A/B definitions unchanged; remap only this copy.
  if (config.left_output == MotorOutput::B) {
    std::swap(active_config.pinA, active_config.pinB);
    std::swap(active_config.crsf.channelA, active_config.crsf.channelB);
  }
  switch (active_config.protocol) {
    case MotorProtocol::DShot:
      motors_initialised = setup_dshot(active_config);
      break;
    case MotorProtocol::CRSF:
      motors_initialised = setup_crsf(active_config);
      break;
    case MotorProtocol::OneShot:
      motors_initialised = setup_oneshot(active_config);
      break;
  }
  if (motors_initialised) {
    motor_init_time = millis();
    motor_startup_pending = true;
  }
  return motors_initialised;
}

motorSpeeds command_motors(float left, float right) {
  const unsigned long last_link_update_ms = radio_state.last_link_update_ms.load();
  unsigned long nowish = millis();

  // Safety things
  if (nowish - last_link_update_ms > global_config.E_stop_time) {  // E-stop, lost signal from transmitter
    right = 0;
    left = 0;
  } else {
    if (watchdog_enabled) {
      watchdog_update();
    } else if (nowish > global_config.E_stop_time) {    // only set watchdog after E-stop timeout has had a chance to kick in, prevents restart loop
      watchdog_enable(global_config.watchdog_time, 0);  // 0 sets mode to reboot if lost connection
      watchdog_enabled = true;
      watchdog_update();
    }
  }

  return command_outputs(left, right);
}

static motorSpeeds command_outputs(float left, float right) {
  if (!motors_initialised) return {};
  const unsigned long nowish = millis();

  // Keep sending zero through the selected protocol while the ESC starts.
  // Latch completion so millis() rollover cannot restart the delay later.
  if (motor_startup_pending) {
    if (nowish - motor_init_time < active_config.startup_delay_ms) {
      left = 0;
      right = 0;
    } else {
      motor_startup_pending = false;
    }
  }

  switch (active_config.protocol) {
    case MotorProtocol::DShot:
      return command_dshot(left, right);
    case MotorProtocol::CRSF:
      return command_crsf(left, right, active_config);
    case MotorProtocol::OneShot:
      return command_oneshot(left, right);
  }
  return {};
}

// DShot implementation.
static bool setup_dshot(const MotorConfig& config) {
  // initialise motors
  if (config.count == MotorCount::Dual) {
    motor_Right = new BidirDShotX1(config.pinB, config.dshot.bitrate_kbps);
  }
  motor_Left = new BidirDShotX1(config.pinA, config.dshot.bitrate_kbps);
  motor_Left->sendThrottle(0);
  if (motor_Right) motor_Right->sendThrottle(0);
  return true;
}

static motorSpeeds command_dshot(float left, float right) {
  unsigned long nowish = millis();

  // get ERPM telemetry
  motor_Left->getTelemetryErpm(&erpm_left);
  if (motor_Right) motor_Right->getTelemetryErpm(&erpm_right);
  motorSpeeds return_speeds;
  return_speeds.left = erpm_left;
  return_speeds.right = erpm_right;

  // Guard against oversending motor updates
  static unsigned long lastMotorUpdate = 0;
  unsigned long now = micros();
  if (now - lastMotorUpdate < active_config.dshot.update_interval_us) return return_speeds;

  // Convert -1.0 to 1.0 -> DSHOT signals, flip direction as desired
  int dshot_left = floatToDshot(left * active_config.left_direction);
  int dshot_right = floatToDshot(right * active_config.right_direction);

  motor_Left->sendThrottle(dshot_left);
  if (motor_Right) motor_Right->sendThrottle(dshot_right);
  lastMotorUpdate = now;

  if (!active_config.dshot.desync_detection) return return_speeds;

  static bool desyncing = false;
  if ((erpm_left > 63000 && fabs(left) < 0.001f) || (motor_Right && erpm_right > 63000 && fabs(right) < 0.001f)) {
    static uint32_t start_desync = 0;
    if (!desyncing) {
      desyncing = true;
      start_desync = nowish;
    }
    if (nowish - start_desync > active_config.dshot.desync_detect_time_ms) {
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

static int floatToDshot(float value) {
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
    // Map -1.0 Ã¢â€ â€™ revMax, 0.0- Ã¢â€ â€™ revMin
    return revMin + (int)((-value) * (revMax - revMin));
  } else {
    // Map 0.0+ Ã¢â€ â€™ fwdMin, +1.0 Ã¢â€ â€™ fwdMax
    return fwdMin + (int)(value * (fwdMax - fwdMin));
  }
}

// CRSF implementation.
static bool setup_crsf(const MotorConfig& config) {
  const auto& crsf = config.crsf;
  const bool dual = config.count == MotorCount::Dual;
  if (crsf.tx_pin < 0 || crsf.tx_pin > 29 ||
      crsf.channelA < 1 || crsf.channelA > 16 ||
      (dual && (crsf.channelB < 1 || crsf.channelB > 16 ||
                crsf.channelB == crsf.channelA)) ||
      crsf.baud_rate == 0 || crsf.update_interval_us == 0 ||
      crsf.update_interval_us < (260000000ULL + crsf.baud_rate - 1) / crsf.baud_rate) {
    Serial.println("Invalid CRSF motor configuration");
    return false;
  }
  if (global_config.speed_source == SensorType::ERPM) {
    Serial.println("CRSF motor output has no ERPM telemetry; use the accelerometer");
    return false;
  }
  if (!motor_crsf.begin(crsf.tx_pin, crsf.baud_rate)) {
    Serial.println("CRSF motor PIO/DMA initialisation failed");
    return false;
  }
  return true;
}

static motorSpeeds command_crsf(float left, float right, const MotorConfig& config) {
  const unsigned long now = micros();
  if (!crsf_frame_started || now - last_crsf_update >= config.crsf.update_interval_us) {
    uint16_t channels[sabre_crsf::channel_count];
    for (auto& channel : channels) channel = sabre_crsf::neutral;
    channels[config.crsf.channelA - 1] =
        sabre_crsf::throttle_channel(left * config.left_direction);
    if (config.count == MotorCount::Dual) {
      channels[config.crsf.channelB - 1] =
          sabre_crsf::throttle_channel(right * config.right_direction);
    }
    sabre_crsf::encode_channels(channels, crsf_frame);
    // send() copies into its own buffer only when the previous DMA is done.
    // If busy, retry with fresh commands on the next loop; never queue frames.
    if (motor_crsf.send(crsf_frame)) {
      last_crsf_update = now;
      crsf_frame_started = true;
    }
  }
  return {};  // This output does not receive motor telemetry.
}

// OneShot125 implementation, ported from b6506b7's oneshot_Duty().
static bool setup_oneshot(const MotorConfig& config) {
  const auto& output = config.oneshot;
  const bool dual = config.count == MotorCount::Dual;
  const float zero_duty = oneshot_duty(0.0f, output);
  oneshot_left = new RP2040_PWM(config.pinA, output.frequency_hz, zero_duty);
  // The constructor stores settings; setPWM() starts the hardware output.
  bool started = oneshot_left->setPWM();
  if (dual && started) {
    oneshot_right = new RP2040_PWM(config.pinB, output.frequency_hz, zero_duty);
    started = oneshot_right->setPWM();
  }
  if (!started) {
    // Stop any partially initialised output and leave the signal pins low.
    pwm_set_enabled(pwm_gpio_to_slice_num(config.pinA), false);
    if (dual) pwm_set_enabled(pwm_gpio_to_slice_num(config.pinB), false);
    digitalWrite(config.pinA, LOW);
    pinMode(config.pinA, OUTPUT);
    if (dual) {
      digitalWrite(config.pinB, LOW);
      pinMode(config.pinB, OUTPUT);
    }
    Serial.println("OneShot motor PWM initialisation failed");
  }
  return started;
}

static motorSpeeds command_oneshot(float left, float right) {
  const auto& output = active_config.oneshot;
  oneshot_left->setPWM(active_config.pinA, output.frequency_hz,
                      oneshot_duty(left * active_config.left_direction, output));
  if (oneshot_right) {
    oneshot_right->setPWM(active_config.pinB, output.frequency_hz,
                         oneshot_duty(right * active_config.right_direction, output));
  }
  return {};  // OneShot does not return motor telemetry.
}
