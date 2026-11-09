// Runs USB-to-ESC passthrough so a computer can configure or program the ESCs.
// Note: this uses a static version of the BLHeli-Passthrough library because I couldn't get PIO to nicely download a live version from https://github.com/BrushlessPower/BlHeli-Passthrough

#include "sabre_globals.h"
#include "esc_passthrough.h"

void passthrough() {
    uint8_t pins[] = {static_cast<uint8_t>(global_config.motor_config.pinA),
                      static_cast<uint8_t>(global_config.motor_config.pinB)};
    const uint8_t pin_count = global_config.motor_config.count == MotorCount::Single ? 1 : 2;
    beginPassthrough(pins, pin_count);
    // Keep the USB serial stream exclusively for MSP/4-way protocol traffic.
    while (true) {
        processPassthrough();
    }
}
