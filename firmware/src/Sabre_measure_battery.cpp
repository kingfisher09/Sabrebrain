#include "sabre_config.h"
#include "sabre_measure_battery.h"
#include <Arduino.h>

int bat_pin;
float bat_ratio = 0;
int cell_count;

void settup_measure_bat(Sabre_Config config) {
  bat_pin = config.bat_pin;
  bat_ratio = config.bat_read_ratio;
  cell_count = config.cell_count;
  analogReadResolution(10);
  pinMode(bat_pin, INPUT);
}

float measure_bat(){
    if (!bat_ratio){
        Serial.println("Measure bat not initiated");
    }
    return analogRead(bat_pin) * (3.3f / 1023.0f) * bat_ratio / cell_count;
}