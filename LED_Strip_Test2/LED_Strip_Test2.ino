// UIDescription: This example shows how to blur a strip of LEDs. It uses the blur1d function to blur the strip and fadeToBlackBy to dim the strip. A bright pixel moves along the strip.
// Author: Zach Vorhies

#include <FastLED.h>

#define NUM_LEDS 23
#define DATA_PIN 27  // Change this to match your LED strip's data pin
#define BRIGHTNESS 255

CRGB leds[NUM_LEDS];
int pos = 0;
int dir = 1;
const int hue_change = round(255 / NUM_LEDS); 
float delayms = 30;
void setup() {
  Serial.begin(9600);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // Add a bright pixel that moves
  leds[pos] = CHSV(pos * hue_change, 255, 255);
  // Blur the entire strip
  blur1d(leds, NUM_LEDS, 172);
  fadeToBlackBy(leds, NUM_LEDS, 16);
  FastLED.show();
  pos = (pos + dir);
  Serial.println(delayms);
  // Serial.println(dir);
  if (pos > NUM_LEDS) {
    // pos = 0;
    dir = -1;
  }
  if (pos < 0) {
    dir = 1;
  }

  delay(delayms);
}
