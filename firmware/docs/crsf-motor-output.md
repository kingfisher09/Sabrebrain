# CRSF motor output (Snipe)

Select `MotorProtocol::CRSF` in `motor_config` in `include/sabre_config.h`.
The default remains DShot. Set the nested `crsf` configuration:

- `tx_pin`: the shared RP2040 GPIO output (default GPIO 4).
- `left_channel` / `right_channel`: 1-based CRSF channels (default 1 and 2).
- `baud_rate`: defaults to 420000, non-inverted 8N1.
- `update_interval_us`: defaults to 1000 (1 kHz).

Connect the configured TX GPIO to Snipe's **S2** signal input and connect
grounds. GPIO numbering is RP2040 GPIO numbering, not connector position.
This is a transmit-only PIO UART, independent of the radio receiver on Serial1.
It uses one PIO state machine and one DMA channel, claimed from the available
hardware resources. No second motor signal wire is needed. If either resource
is unavailable, initialisation reports an error and motor output stays disabled.

Configure the Snipe's channel mapping to match. Commands are independent motor
values, with `left_direction` / `right_direction` applied before transmission.
Zero is CRSF 992 (1500 us); full reverse/forward are 172/1812 (988/2012 us).
Use the Snipe calibration procedure for the configured channel assignment and
range. In Single mode only the left channel is commanded. All other channels
remain neutral, including the right channel in Single mode.

The shared startup delay and radio E-stop force neutral commands. On radio
loss this firmware continues sending neutral; it does not signal link loss by
stopping CRSF frames. If the firmware stops transmitting, the ESC's own
signal-loss handling applies. No ESC telemetry is received, so ERPM speed
measurement is rejected for CRSF; use the accelerometer.

Frames use CRSF type 0x16, sixteen packed 11-bit channels, and CRC8-DVB-S2.
Each eligible call to `command_motors()` builds a frame and starts DMA. DMA
feeds the PIO FIFO automatically as space becomes available, so the core can
continue other work while the complete frame transmits (about 619 us at
420000 baud). No DMA interrupt or per-byte loop servicing is needed.

The DMA source is a private 26-word buffer (104 bytes), reused only after DMA
finishes reading it. The next frame is not queued while DMA or the FIFO is
still busy. If a send cannot start, the next call retries with fresh commands.
DMA completion means the buffer is reusable, not necessarily that the last
bits have left the pin. PIO preserves the serial start/stop timing between bytes.

Calls to `command_motors()` still schedule each new frame, so 1 kHz is a target
maximum, not an independent periodic timer. Loop frequency affects frame start
times but no longer creates gaps between bytes within a frame. DMA never
repeats an old frame automatically if the application stops running.

`include/sabre_crsf_output.h` contains both packet encoding and the DMA
transmitter declaration; the hardware implementation stays in
`src/Sabre_crsf_dma.cpp`.

Packet checks in `test/motor_control/compile_checks.h` use compile-time
assertions. `src/compile_tests.cpp` includes this header,
so every normal PlatformIO build compiles the CRSF and OneShot checks.

References:

- https://github.com/cb-repo/P037-SNIPE-40A
- https://github.com/tbs-fpv/tbs-crsf-spec/blob/master/crsf.md

Hardware operation with Snipe has not yet been verified.
