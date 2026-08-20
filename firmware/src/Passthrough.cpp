// Code for BLHeli ESC passthrough programming
// Note: this uses a static version of the BLHeli-Passthrough library because I couldn't get PIO to nicely download a live version from https://github.com/BrushlessPower/BlHeli-Passthrough

#include "sabre_globals.h"
#include "esc_passthrough.h"

void passthrough() {
    uint8_t pins[] = {MOTOR_LEFT_PIN, MOTOR_RIGHT_PIN};
    beginPassthrough(pins, 2);
    Serial.println("Starting passthrough on pins " + String(pins[0]) + " & " + String(pins[1]));
    while (true) {
        processPassthrough();
    }
}
