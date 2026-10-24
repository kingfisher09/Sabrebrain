#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cmath>
#include <hardware/pio.h>

// CRSF 0x16: sixteen little-endian 11-bit channels in a 22-byte payload.
namespace sabre_crsf {
constexpr size_t channel_count = 16;
constexpr size_t frame_size = 26;
constexpr uint16_t neutral = 992;

constexpr uint16_t throttle_channel(float throttle) {
  // Standard CRSF range corresponding to 988..2012 us, centred at 1500 us.
  // Invalid calculations can produce NaN; treat it as zero throttle.
  if (std::isnan(throttle)) throttle = 0.0f;
  if (throttle < -1.0f) throttle = -1.0f;
  if (throttle > 1.0f) throttle = 1.0f;
  return static_cast<uint16_t>(neutral + throttle * 820.0f + 0.5f);
}

constexpr void encode_channels(const uint16_t* channels, uint8_t* frame) {
  frame[0] = 0xC8;
  frame[1] = 24;  // Type + payload + CRC.
  frame[2] = 0x16;
  for (size_t i = 3; i < frame_size; ++i) frame[i] = 0;
  for (size_t channel = 0; channel < channel_count; ++channel) {
    for (size_t bit = 0; bit < 11; ++bit) {
      const size_t offset = channel * 11 + bit;
      frame[3 + offset / 8] |= ((channels[channel] >> bit) & 1u) << (offset % 8);
    }
  }
  uint8_t crc = 0;
  for (size_t i = 2; i < frame_size - 1; ++i) {
    crc ^= frame[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0xD5 : crc << 1;
    }
  }
  frame[frame_size - 1] = crc;
}
}  // namespace sabre_crsf

// Single-core, transmit-only UART. One DMA channel feeds one PIO state machine.
class CrsfDmaOutput {
 public:
  bool begin(unsigned int pin, unsigned long baud);
  bool send(const uint8_t* frame);

 private:
  PIO pio_ = nullptr;
  int state_machine_ = -1;
  int dma_channel_ = -1;
  // DMA reads this buffer until the transfer completes; send() guards reuse.
  uint32_t dma_words_[sabre_crsf::frame_size]{};
};
