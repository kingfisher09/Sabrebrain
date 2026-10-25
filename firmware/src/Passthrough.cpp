// Runs USB-to-ESC passthrough so a computer can configure or program the ESCs.
// Note: this uses a static version of the BLHeli-Passthrough library because I couldn't get PIO to nicely download a live version from https://github.com/BrushlessPower/BlHeli-Passthrough

#include "sabre_globals.h"
#include "esc_passthrough.h"

void passthrough() {
    uint8_t pins[] = {static_cast<uint8_t>(global_config.motor_config.left_pin),
                      static_cast<uint8_t>(global_config.motor_config.right_pin)};
    beginPassthrough(pins, 2);
    Serial.println("Starting passthrough on pins " + String(pins[0]) + " & " + String(pins[1]));
    while (true) {
        processPassthrough();
    }
}
