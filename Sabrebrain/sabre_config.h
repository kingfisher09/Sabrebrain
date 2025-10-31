#pragma once

// Geometry and derived constants
constexpr uint16_t SABRE_NUM_ANGLES = 150;
constexpr uint16_t SABRE_NUM_LEDS   = 23;

constexpr uint16_t SABRE_MASK_BYTES_PER_FRAME =
    (SABRE_NUM_ANGLES * SABRE_NUM_LEDS * 2 + 7) / 8;
