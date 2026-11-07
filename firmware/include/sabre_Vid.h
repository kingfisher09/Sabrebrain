// This header creates a structure called SabreVid that holds an animation
#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "sabre_screen_frame.h"

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
  const uint8_t* masks;  // Contiguous frame masks; stride belongs to this video.
  const CRGB* const* diffs;                                           // Array of pointers to per-frame diffs
  const size_t maskBytesPerFrame;
  const size_t maskFrameCount;

  // Accept existing generated headers without tying their array width to config.
  template<size_t Frames, size_t MaskBytes>
  constexpr SabreVid(uint16_t angles, uint16_t radii, uint16_t frames, uint16_t duration,
                    const uint8_t (&frame_masks)[Frames][MaskBytes],
                    const CRGB* const* frame_diffs)
      : numAngles(angles), numRadii(radii), numFrames(frames), frameDurationMs(duration),
        masks(&frame_masks[0][0]), diffs(frame_diffs), maskBytesPerFrame(MaskBytes),
        maskFrameCount(Frames) {}

  // Flat data is also suitable for future file-loaded videos.
  constexpr SabreVid(uint16_t angles, uint16_t radii, uint16_t frames, uint16_t duration,
                    const uint8_t* frame_masks, const CRGB* const* frame_diffs)
      : numAngles(angles), numRadii(radii), numFrames(frames), frameDurationMs(duration),
        masks(frame_masks), diffs(frame_diffs),
        maskBytesPerFrame(sabre_screen::mask_bytes(angles, radii)), maskFrameCount(frames) {}

  // --- Accessor functions ---
  constexpr const uint8_t* getMask(uint16_t frame) const {
    return masks + static_cast<size_t>(frame) * maskBytesPerFrame;
  }

  inline const CRGB* getDiff(uint16_t frame) const {
    return diffs[frame];  // diffs[frame] points to a variable-length CRGB array
  }

  // Optional: total pixels per frame
  inline uint32_t pixelsPerFrame() const {
    return (uint32_t)numAngles * numRadii;
  }
};
