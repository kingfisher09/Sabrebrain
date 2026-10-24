# OneShot motor output

Select `MotorProtocol::OneShot` in `motor_config`. The default remains DShot.
The implementation follows `oneshot_Duty()` and `RP2040_PWM` from commit
`b6506b733852b92f00b1ef6837aaae6a83b6c404`, with inputs normalised to -1..+1.

`OneShotMotorConfig` provides defaults, so the `oneshot` member can be omitted:

- `frequency_hz`: 3500 Hz.
- `min_pulse_us`: 125 us (full reverse).
- `max_pulse_us`: 250 us (full forward).
- Zero throttle: midpoint, 187.5 us.

The pulse width is converted to a PWM duty percentage using floating-point
arithmetic. At 3500 Hz the reverse/zero/forward duties are 43.75%, 65.625%,
and 87.5%. The old code truncated the period using integer division; this
version avoids that rounding. Hardware PWM adds its own small quantisation.

The configured left/right pins and directions apply. Single mode only starts
the left output and follows the left command. Two configured GPIOs cannot
alias the same PWM slice/channel (for example GPIO 0 and GPIO 16).

Setup starts continuous zero-throttle pulses and returns immediately. Shared
startup and radio E-stop checks keep the commanded throttle at zero when
required. PWM hardware repeats pulses without per-pulse loop servicing; calls
to `command_motors()` update the duty, with changes applied at PWM wrap.
Zero throttle is a midpoint pulse, not a permanently low signal.

Fixed OneShot settings are checked at compile time in
`test/motor_control/compile_checks.h`, with
separate errors for GPIO/PWM conflicts, directions, pulse timing, and ERPM.
Setup only starts the PWM outputs and handles hardware initialisation failure.
These checks apply to `motor_config`; `setup_motors()` expects validated settings.

OneShot has no motor telemetry; selecting ERPM speed measurement gives a build
error. Use the accelerometer. The watchdog remains responsible for resetting
the controller if the application stops servicing it; PWM otherwise repeats
the last configured duty automatically.

Endpoint, midpoint, and clamp checks are compile-time assertions in the same
test header, included by `src/compile_tests.cpp` so every normal build runs them.
Verify physical pulse widths and ESC response
on hardware; this port has not yet been tested on a robot.
