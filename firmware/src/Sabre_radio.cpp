// Converts CRSF receiver channels into robot commands and tracks radio link health for the E-stop.

#include "sabre_radio.h"
#include "sabre_globals.h"
#include "sabre_measure_battery.h"

static CRSFforArduino crsf = CRSFforArduino(&Serial1);
static void onLinkStatisticsUpdate(serialReceiverLayer::link_statistics_t);

static float read_channel(int channel);
static float powerCurve(float x);
static float servoTofloat(float servoSignal, int stickmode);
RadioState radio_state;

void begin_radio_CRSF() {
  if (!crsf.begin()) {
    Serial.println("CRSF for Arduino initialisation failed!");
    while (true) delay(10);
  }

  crsf.setLinkStatisticsCallback(onLinkStatisticsUpdate);
}

void update_radio_CRSF() {
  // transmitter inputs
  crsf.update();

  radio_state.slip = powerCurve(servoTofloat(read_channel(global_config.SLIP_CH), 1));
  radio_state.trans = powerCurve(servoTofloat(read_channel(global_config.TRANS_CH), 1));
  radio_state.spin = servoTofloat(read_channel(global_config.SPIN_CH), 0);
  radio_state.head = servoTofloat(read_channel(global_config.HEAD_CH), 1);
  radio_state.calibration = read_channel(global_config.CALIB_CH) > 1500;
  radio_state.image_mode = read_channel(global_config.IMAGE_CH);
  radio_state.emote = read_channel(global_config.EMOTE_CH) > 1500;
  if (read_channel(global_config.INVERT_CH) > 1500) {
    radio_state.invert = 1;
  } else {
    radio_state.invert = -1;
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
    radio_state.last_link_update_ms = millis();
    // Serial.println(lqi);
  }
}

// Clamp every receiver channel to the nominal servo range before using it.
static float read_channel(int channel) {
  const float pulse_us = crsf.rcToUs(crsf.getChannel(channel));
  return constrain(pulse_us, 1000.0f, 2000.0f);
}

float powerCurve(float x) {
  int power = 3;
  return pow(x, power);
}

float servoTofloat(float servoSignal, int stickmode) {
  // Map the servo signal to the range of -1 to +1 or 0 to 1, provide deazone
  // stickmode 0 maps to 0..1; stickmode 1 maps to -1..1
  // Deadzone code modifies servo inpout signal before mapping

  if (stickmode == 0)  // Bottom deadzone
  {
    servoSignal = (servoSignal <= 1000 + global_config.deadzone) ? 1000 : servoSignal;
    return (servoSignal - 1000.0f) / 1000.0f;  // 1000-2000 -> 0 to 1
  } else if (stickmode == 1)                   // Centred deadzone
  {
    servoSignal = (servoSignal >= 1500 - global_config.deadzone && servoSignal <= 1500 + global_config.deadzone) ? 1500 : servoSignal;
    return (servoSignal - 1500.0f) / 500.0f;  // 1000-2000 -> -1 to 1
  } else {
    return 0.0f;  // invalid stickmode
  }
}

void CRSF_radio_telem(float zrotspd) {
  unsigned long now = micros();
  static unsigned long lastGpsUpdate = 0;
  if (now - lastGpsUpdate >= 500000) {
    lastGpsUpdate = now;
    float bat_volt = measure_bat();

    // Update the GPS telemetry data with the new values.

    // Telemetry depends on speed measurement mode
    float telem_calib = 0;
    if (global_config.speed_source == SensorType::Accelerometer) {
      telem_calib = global_calibration.accel_radius_m * 100;
    } else if (global_config.speed_source == SensorType::ERPM) {
      telem_calib = global_config.base_ERPM_cal;
    }
    crsf.telemetryWriteGPS(0, 0, zrotspd * 6000 / 360, 0, telem_calib, global_calibration.accel_trim_point_count);
    crsf.telemetryWriteBattery(bat_volt * 100.0f, 0, 0, 0);
  }
}
