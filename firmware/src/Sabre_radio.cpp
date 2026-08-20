#include "sabre_globals.h"

static float powerCurve(float x);
static float servoTofloat(float servoSignal, int stickmode);
bool calib_button;

void updateCRSF() {
  // transmitter inputs
  crsf.update();

  slip = powerCurve(servoTofloat(crsf.rcToUs(crsf.getChannel(SLIP_CH)), 1));
  trans = powerCurve(servoTofloat(crsf.rcToUs(crsf.getChannel(TRANS_CH)), 1));
  spin = servoTofloat(crsf.rcToUs(crsf.getChannel(SPIN_CH)), 0);
  head = servoTofloat(crsf.rcToUs(crsf.getChannel(HEAD_CH)), 1);
  calib_button = crsf.rcToUs(crsf.getChannel(CALIB_CH)) > 1500;
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