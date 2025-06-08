// UIDescription: This example shows how to blur a strip of LEDs. It uses the blur1d function to blur the strip and fadeToBlackBy to dim the strip. A bright pixel moves along the strip.
// Author: Zach Vorhies

#include <FastLED.h>

#define NUM_LEDS 23
#define DATA_PIN 27  // Change this to match your LED strip's data pin
#define BRIGHTNESS 255

CRGB pict[78] = {
  CRGB(0, 0, 255), CRGB(0, 0, 30), CRGB(17, 89, 255), CRGB(0, 0, 255),
  CRGB(0, 255, 0), CRGB(255, 0, 0), CRGB(80, 130, 240), CRGB(10, 210, 180),
  CRGB(75, 50, 220), CRGB(180, 90, 170), CRGB(130, 250, 60), CRGB(33, 99, 201),
  CRGB(255, 0, 0), CRGB(15, 0, 255), CRGB(202, 60, 230), CRGB(0, 220, 40),
  CRGB(91, 33, 143), CRGB(70, 255, 180), CRGB(192, 100, 20), CRGB(0, 0, 255),
  CRGB(10, 100, 200), CRGB(150, 80, 255), CRGB(255, 90, 90), CRGB(110, 160, 250),
  CRGB(85, 240, 0), CRGB(190, 60, 70), CRGB(44, 200, 110), CRGB(132, 255, 77),
  CRGB(50, 40, 255), CRGB(245, 180, 10), CRGB(25, 130, 255), CRGB(200, 100, 40),
  CRGB(111, 255, 111), CRGB(20, 80, 240), CRGB(210, 55, 150), CRGB(255, 200, 30),
  CRGB(100, 100, 100), CRGB(45, 150, 230), CRGB(240, 60, 190), CRGB(130, 70, 220),
  CRGB(160, 255, 60), CRGB(78, 44, 200), CRGB(230, 160, 90), CRGB(70, 100, 250),
  CRGB(0, 200, 100), CRGB(255, 120, 80), CRGB(88, 90, 210), CRGB(115, 255, 150),
  CRGB(30, 190, 255), CRGB(140, 60, 200), CRGB(90, 255, 90), CRGB(60, 140, 255),
  CRGB(255, 0, 60), CRGB(50, 100, 200), CRGB(220, 90, 130), CRGB(100, 250, 90),
  CRGB(255, 70, 200), CRGB(85, 200, 0), CRGB(10, 255, 140), CRGB(170, 50, 255),
  CRGB(200, 110, 80), CRGB(135, 255, 120), CRGB(60, 200, 240), CRGB(245, 100, 60),
  CRGB(95, 130, 255), CRGB(0, 255, 90), CRGB(150, 200, 100), CRGB(255, 50, 180),
  CRGB(180, 255, 40), CRGB(50, 60, 255), CRGB(100, 255, 190), CRGB(0, 40, 130),
  CRGB(80, 220, 160), CRGB(255, 150, 50), CRGB(60, 250, 120), CRGB(0, 40, 255),
  CRGB(160, 240, 90), CRGB(25, 0, 200)
};

CRGB leds[NUM_LEDS];
int pos = 0;
int dir = 1;
int start_delay = 30;
float delayms = start_delay;
float delay_ratio = 0.9;
void setup() {
  Serial.begin(9600);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // Add a bright pixel that moves
  // leds[pos] = CHSV(pos * 5, 255, 255);
  memcpy(leds, pict, sizeof(pict));
  // // Blur the entire strip
  // blur1d(leds, NUM_LEDS, 172);
  // fadeToBlackBy(leds, NUM_LEDS, 16);
  FastLED.show();
  // pos = (pos + dir);
  // Serial.println(delayms);
  // // Serial.println(dir);
  // if (pos > NUM_LEDS) {
  // //   // pos = 0;
  //   dir = -1;
  //   delayms = delayms * delay_ratio;
  // }
  // if (pos < 0){
  //   dir = 1;
  //   delayms = delayms * delay_ratio;
  // }

  // if (delayms < 1){
  //   delayms = start_delay;
  // }

  // delay(delayms);
}
