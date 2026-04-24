// This header creates a structure called SabreVid that holds an animation
#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "sabre_config.h"

// -----------------------------------------------------------------------------
// SabreVid — description of an exported animation for the POV system
//
//  - Masks:   [numFrames][maskBytesPerFrame]
//              → Each frame’s 2-bit mode flags (fixed length per frame)
//
//  - Diffs:   array of pointers to variable-length CRGB arrays
//              → Each frame’s list of new colours
//
//  - Simple accessors to retrieve a frame’s mask or diff data
// -----------------------------------------------------------------------------

struct SabreVid {
  // --- Geometry / metadata ---
  const uint16_t numAngles;        // Angular resolution (e.g. 150)
  const uint16_t numRadii;         // Radial resolution (e.g. 24)
  const uint16_t numFrames;        // Number of frames in animation
  const uint16_t frameDurationMs;  // Duration of each frame in ms

  // --- Data arrays ---
  const uint8_t (*masks)[MASK_BYTES_PER_FRAME];  // Fixed-length 2D array in PROGMEM
  const CRGB* const* diffs;                                           // Array of pointers to per-frame diffs

  // --- Accessor functions ---
  inline const uint8_t* getMask(uint16_t frame) const {
    return masks[frame];  // each mask row has the same fixed length
  }

  inline const CRGB* getDiff(uint16_t frame) const {
    return diffs[frame];  // diffs[frame] points to a variable-length CRGB array
  }

  // Optional: total pixels per frame
  inline uint32_t pixelsPerFrame() const {
    return (uint32_t)numAngles * numRadii;
  }
};
