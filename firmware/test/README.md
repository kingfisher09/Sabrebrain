# Compile-time motor checks

`motor_control/compile_checks.h` contains OneShot configuration/conversion
checks and CRSF conversion/packet checks. `src/compile_tests.cpp` includes
this header and provides a single place to add other compile-time test suites.
The OneShot conversion definition lives in `include/sabre_oneshot_output.h`
so both motor control and the tests can evaluate the same function.
Configuration assertions check a constexpr default `MotorConfig`;
settings read from a file will require runtime validation in the loader.

Use the usual PlatformIO **Build** action in VS Code, or `pio run`.
Every `static_assert` must be true for compilation to succeed. No robot,
upload, or runtime test runner is required. These checks do not validate
physical signals or ESC operation.

The folder name alone does not make these files part of a normal build;
PlatformIO builds `src/compile_tests.cpp` as a normal source file, and its
explicit includes select the checks. PlatformIO's separate **Test** action is for
test suites with a test runner, which these compile-time checks do not use.
