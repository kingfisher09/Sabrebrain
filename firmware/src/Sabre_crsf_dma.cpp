// Sends complete CRSF frames through a PIO serial output fed automatically by DMA.
#include "sabre_crsf_output.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio_instructions.h>

// 8N1 UART, eight PIO cycles per bit. Same four-instruction scheme as
// Raspberry Pi's BSD-3-Clause pico-examples/pio/uart_tx/uart_tx.pio.
// Optional side-set controls start/stop bits; OUT sends the eight data bits.
static const uint16_t uart_instructions[] = {
    static_cast<uint16_t>(pio_encode_pull(false, true) |
                          pio_encode_sideset_opt(1, 1) | pio_encode_delay(7)),
    static_cast<uint16_t>(pio_encode_set(pio_x, 7) |
                          pio_encode_sideset_opt(1, 0) | pio_encode_delay(7)),
    static_cast<uint16_t>(pio_encode_out(pio_pins, 1)),
    static_cast<uint16_t>(pio_encode_jmp_x_dec(2) | pio_encode_delay(6)),
};
static const pio_program uart_program = {uart_instructions, 4, -1};

bool CrsfDmaOutput::begin(unsigned int pin, unsigned long baud) {
  if (pio_ || pin > 29 || baud == 0) return false;
  const float divider = static_cast<float>(clock_get_hz(clk_sys)) / (8.0f * baud);
  if (divider < 1.0f || divider >= 65536.0f) return false;

  // Share hardware resource claims with the SDK and other Arduino libraries.
  const PIO candidates[] = {pio0, pio1};
  for (PIO candidate : candidates) {
    if (!pio_can_add_program(candidate, &uart_program)) continue;
    const int sm = pio_claim_unused_sm(candidate, false);
    if (sm < 0) continue;
    const int dma = dma_claim_unused_channel(false);
    if (dma < 0) {
      pio_sm_unclaim(candidate, sm);
      return false;
    }
    const unsigned int offset = pio_add_program(candidate, &uart_program);
    pio_ = candidate;
    state_machine_ = sm;
    dma_channel_ = dma;

    // Drive idle high before connecting the GPIO to the state machine.
    pio_sm_set_pins_with_mask(pio_, sm, 1u << pin, 1u << pin);
    pio_sm_set_pindirs_with_mask(pio_, sm, 1u << pin, 1u << pin);
    pio_gpio_init(pio_, pin);
    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, offset, offset + 3);
    sm_config_set_sideset(&config, 2, true, false);  // Enable bit + one pin.
    sm_config_set_sideset_pins(&config, pin);
    sm_config_set_out_pins(&config, pin, 1);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&config, divider);
    pio_sm_init(pio_, sm, offset, &config);

    dma_channel_config dma_config = dma_channel_get_default_config(dma);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio_, sm, true));
    dma_channel_configure(dma, &dma_config, &pio_->txf[sm], dma_words_, 0, false);
    pio_sm_set_enabled(pio_, sm, true);
    return true;
  }
  return false;
}

bool CrsfDmaOutput::send(const uint8_t* frame) {
  if (!pio_ || dma_channel_is_busy(dma_channel_) ||
      !pio_sm_is_tx_fifo_empty(pio_, state_machine_)) return false;

  // Each FIFO entry is a 32-bit word; PIO uses its low eight bits as a byte.
  for (size_t i = 0; i < sabre_crsf::frame_size; ++i) dma_words_[i] = frame[i];
  dma_channel_set_read_addr(dma_channel_, dma_words_, false);
  dma_channel_set_trans_count(dma_channel_, sabre_crsf::frame_size, true);
  return true;
}
