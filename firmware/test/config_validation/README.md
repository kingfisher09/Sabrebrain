# JSON validator tests

These are runtime tests for `validate_config()` and `config_from_json()`, unlike the static assertions
compiled by the normal firmware build. They require a desktop C++ compiler,
not a robot. From the firmware directory, build and run, for example:

```text
g++ -std=c++17 -Iinclude -I.pio/libdeps/xiao_rp2040/ArduinoJson/src src/Sabre_config_validation.cpp src/Sabre_config_unpack.cpp test/config_validation/validation_checks.cpp -o .pio/config_validation_tests.exe
.pio/config_validation_tests.exe
```

ArduinoJson is installed by the normal PlatformIO build. Run tests without
defining `NDEBUG`, so assertions remain enabled. The suite checks defaults,
type/schema_version errors, wrong JSON types, motor mappings, timing constraints,
empty and expanded LED layouts, nonfinite positions, and a self-contained JSON fixture.
Motor selection checks cover swapped A/B mappings, direction corrections,
invalid/shared outputs, and a single motor assigned to B.
Unpacking checks cover protocol conversion, settings overrides (including
false and zero values), replacing layouts, defaults on repeated loads, and
preserving the destination when validation fails.
