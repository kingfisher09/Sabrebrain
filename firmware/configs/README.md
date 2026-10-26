# Robot configuration draft

Put exactly one `.json` file in `/SABREBRAIN/config` in FatFS, for example
`/SABREBRAIN/config/Sabretooth.json`. Creating
this example does not upload it or change the robot's current settings.
Calibration remains separate.

`cell_count` is the battery's number of series cells (a positive integer).
It defaults to 1 if omitted and is used for per-cell voltage measurement.
Set it to the actual battery cell count in each robot's JSON.

## Copying configuration onto the robot

1. Open the PlatformIO serial monitor at 115200 baud. Send `1` for file transfer.
   Enter is not required by the firmware (a terminal that buffers input may
   still require it). Serial input uses USB, not the radio UART.
2. Wait for the confirmation that the next mode was saved, then power cycle
   the controller. Sending the command does not stop or restart the robot.
3. The controller appears as a USB drive. Normal robot hardware, motor outputs,
   and calibration writes are not started in this mode. The onboard NeoPixel is green.
4. Copy `config/robot.example.json` into the drive's `SABREBRAIN/config` folder,
   naming it something useful such as `Sabretooth.json`. Edit or replace it for your robot.
   You can also copy files off the drive for backups, or delete unwanted files.
5. Eject the drive using the computer's safe removal command, then power cycle
   the controller. It starts Robot mode, validates the JSON, and loads it.
   Invalid or missing config automatically starts USB file transfer instead,
   with robot hardware disabled. Fix the JSON, eject, and power cycle to retry.

The loader accepts any filename ending in `.json` (case-insensitive), but requires
exactly one matching file in that folder. Zero or multiple JSON files cause USB
file transfer mode; it never chooses arbitrarily. Other file types and subfolders
are ignored. Keep backup configurations outside `SABREBRAIN/config`.
The old `/SABREBRAIN/config.json` location is no longer loaded: move that file
into the new folder. Calibration stays at `/SABREBRAIN/calibration.bin`.

If configuration loading fails, read `/SABREBRAIN/errors.txt` on the USB drive.
It records the latest failure, including JSON parsing errors, validation errors
(with the setting's path), missing files, or multiple config files. When a file
was selected, its path is included. The report is written before exporting the
drive and removed after a successful config load, so stale failures do not
remain. Serial still prints the error as well. If storage cannot be written,
the report cannot be saved and the serial output explains that failure.

The USB serial port may disconnect and reappear during power cycles. Eject the
USB drive before disconnecting power or resetting the controller. Ejecting
alone does not start the robot. No serial command is required to leave drive
mode: the next startup is already set to Robot.

In Robot mode, core 1 prints a menu every 10 seconds while USB serial is open:
`Enter 1 for file transfer, 2 for motor selection, 3 for ESC passthrough`.
Other characters are ignored. The first successfully saved selection writes
the one-byte `/SYSTEM/next_boot_mode.bin` file and prints
`Power cycle to enter <selected mode>`. Menu messages and further selections
then stop until the next startup. The current operating mode continues until
the controller is power cycled. Returning to Robot needs only a power cycle.

At startup, core 0 mounts FatFS, reads the saved mode, and resets the file to
Robot after reading it, including when its contents are already zero or invalid.
A missing or invalid file
uses `Sabre_Config::boot_mode` in `include/sabre_config.h`, whose options are
`BootMode::Robot`, `BootMode::USBStorage`, `BootMode::MotorSelection`, and
`BootMode::Passthrough`. A valid saved value overrides this build-time default.
If resetting the saved mode fails, robot hardware stays disabled.

When Robot is selected, core 0 loads and validates configuration before either
core starts robot hardware. On a fresh board, or if loading fails, it selects
USB storage automatically for this boot. This fallback is not saved as the
next mode: each power cycle retries Robot and checks the configuration again.

`2` selects the serial motor wizard. After confirmation with Y, connect the
transmitter, lower spin throttle to zero, then raise it to the desired test speed.
The wizard applies that throttle to physical output A, asks which wheel moved
(L/R for dual motors, Y/N for a single motor), and asks direction (F/B).
Throttle remains adjustable while waiting for answers; lowering it to zero stops
the motor. Dual motors then test B; a single motor only tests B if A did not move
it. Throttle must return to zero before starting the next output. Core 1 updates
the receiver while core 0 uses normal motor commands, including radio-link
E-stop and watchdog handling. Radio loss is handled entirely by normal motor
control, including watchdog reboot if the link remains lost; the wizard adds no
separate radio recovery logic. Answers are single characters, case-insensitive, without
requiring Enter. Motors are commanded to zero before the function returns.
The returned selection contains left/right pin or CRSF channel numbers and
direction corrections; a single motor uses the left fields. The wizard saves
the mapping in `motors.selection` in the loaded JSON file, keeping its filename
and unrelated settings. After a successful save it automatically restarts into
file transfer mode (green NeoPixel), so you can back up the updated config.
Eject the drive and power cycle to return to Robot with the new mapping.
A previous-file backup is kept
beside it with `.bak` appended; this does not count as a second JSON config.
If a save fails, see `/SABREBRAIN/errors.txt`. If power is lost during replacement
and only `.json.bak` remains, restore its `.json` name in USB storage.
Motor selection loads the robot's config at startup
and falls back to file transfer if that config is invalid.
Core 1 stays idle in all special modes;
the menu is available exclusively in Robot mode. Passthrough retains its
dedicated serial operation. A one-shot passthrough request returns to Robot
on the next startup; a build-time passthrough default applies when no valid
saved file exists.

The onboard NeoPixel indicates the active mode: off for Robot, green for file
transfer, blue for motor selection, and red for ESC passthrough.

The USB drive exports the entire existing 0.5 MB FatFS partition, including
`SABREBRAIN` (configuration, calibration, and other robot files) and `SYSTEM`.
The binary mode file is internal bookkeeping, but is visible on that drive.
Firmware filesystem access stays stopped for the whole USB drive session,
including after eject. The serial mode menu does not run in drive mode.
The firmware creates `SABREBRAIN` if needed and does not overwrite robot config
or calibration. Copying image files is possible, but displaying external images
still needs a separate implementation.

`validate_config()` in `src/Sabre_config_validation.cpp` checks the parsed
JSON before conversion. It accepts omitted optional settings, rejects null or
incorrectly typed values, checks motor pins/channels and protocol timing, and
checks each supplied LED position. Every supplied protocol block is checked,
even if inactive; only selected-protocol motor pin/channel conflicts apply.
It currently supports the motor and LED fields shown in this example. Unknown
fields are left alone for future extensions. The current DShot implementation
uses 300 kbit/s, so other `bitrate_kbps` values are rejected for now.

`config_from_json()` in `src/Sabre_config_unpack.cpp` starts with fresh struct defaults,
validates the document, and unpacks the supported fields into a temporary
configuration. Only the complete result is assigned to the caller. A failed
load leaves the old settings unchanged. Filesystem access remains in
`load_config()` in storage.

- `type` is `"SCFG"`, matching the config identifier in `Sabre_file_types.h`.
  JSON stores it as a string; the loader checks that identifier before
  interpreting the settings.
- `schema_version` identifies the file format, not the firmware version.
- `robot_name` is a user-chosen label for backups and the future interface.
- `protocol` is `dshot`, `crsf`, or `oneshot`.
- `count` is 1 or 2. A single motor uses the output selected for the left motor.
- `startup_delay_ms` applies to all protocols.
- `A` and `B` identify physical motor outputs, independent of left/right naming.
  For DShot/OneShot, `pin` is the physical RP2040 GPIO number.
  For CRSF, `channel` selects the ESC channel instead (1-based, defaults 1 for
  A and 2 for B), with the shared `tx_pin` in the `crsf` block.
  The example includes both mappings: DShot/OneShot use `pin` and ignore
  `channel`; CRSF uses `channel` and ignores the per-motor `pin`.
- `selection.left` and `selection.right` each contain `output` (`"A"` or `"B"`)
  and `direction` (`1` or `-1`). Defaults are left=A, right=B, both positive.
  Dual motors must select different outputs; a single motor can use A or B.
  Physical pin/channel definitions do not change when the wizard saves a mapping.
  Old direction fields under `A`/`B` remain accepted as defaults for configs
  without explicit selection directions.
- Only the selected protocol needs its settings block. The loader
  supplies documented defaults for omitted optional settings. This example
  includes all three protocol blocks for reference; `protocol` selects which
  one is used.

The protocol defaults mirror the firmware:

| Protocol | Settings |
| --- | --- |
| DShot | 300 kbit/s, 2000 us update interval, desync detection enabled, 200 ms detection time |
| CRSF | TX GPIO 4, 420000 baud, 1000 us update interval |
| OneShot | 3500 Hz, 125 us minimum pulse, 250 us maximum pulse |

For CRSF, the settings keys are `tx_pin`, `baud_rate`, and
`update_interval_us`. For OneShot they will be `frequency_hz`, `min_pulse_us`,
and `max_pulse_us`. Unselected protocol blocks can be omitted entirely.

`leds.top` and `leds.bottom` list LED positions in physical strip order.
Each entry supplies `radius_mm` and `angle_deg`, matching `LedPosition`.
The example contains 26 top LEDs and 9 bottom LEDs; firmware defaults are empty.
For a different robot, add/remove entries and set their
positions. The loader replaces each supplied layout, rather than appending it to
the defaults. LED counts come directly from the vector sizes.
An omitted layout should retain its default; an explicitly empty array means
zero LEDs on that side. Both arrays can be empty for a robot without LEDs;
screen setup and rendering are then skipped. Omitting `leds` keeps the default
layouts (currently empty).

Radio, movement, display timing, and board wiring are outside this first pass.

## Firmware settings structure

`Sabre_Config` in `include/sabre_config.h` now groups the firmware settings and
their defaults. The active instance is `global_config`, defined in `main.cpp`.
For example, motor setup uses `global_config.motor_config`, radio processing
uses `global_config.SLIP_CH`, and display mapping uses the LED position arrays
in that same object. JSON currently populates the robot name, motor options,
and LED layouts. Other struct fields retain their defaults until their JSON
mapping and validation are added.

The startup mode, compiled image dimensions,
and FastLED template pins remain `static constexpr` members of `Sabre_Config`.
They are build-time options, not fields that a JSON file can change yet.
Compile-time configuration checks cover defaults only; the JSON loader
validates supported file values before publishing them to either core.

Physical LED position lists are runtime settings, and their sizes determine
the counts. Display and mapping buffers are sized during screen
setup, and FastLED receives the configured count rather than a hard-coded 26.
Changing counts or layouts requires restarting/reinitialising the
display; do not resize its buffers while rendering. Allocation is limited by
available RAM, not a compiled LED count.
