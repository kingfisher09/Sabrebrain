#pragma once

#include <atomic>

// Core 1 writes receiver inputs; other cores may read each member independently.
// Members are individually atomic, not a single frame-consistent snapshot.
struct RadioState {
  std::atomic<float> slip{0};
  std::atomic<float> trans{0};
  std::atomic<float> spin{0};
  std::atomic<float> head{0};
  std::atomic<bool> calibration{false};
  std::atomic<bool> emote{false};
  std::atomic<int> image_mode{0};
  std::atomic<int> invert{1};
  std::atomic<unsigned long> last_link_update_ms{0};
};

extern RadioState radio_state;

void begin_radio_CRSF();
void CRSF_radio_telem(float zrotspd);
void update_radio_CRSF();
