// Identifies physical motor outputs and forward direction using single-character answers.
#include "Sabre_motor_selection.h"
#include "sabre_config.h"
#include "sabre_motor_control.h"
#include "sabre_globals.h"
#include "sabre_radio.h"
#include <Arduino.h>
#include <cstring>

namespace {
enum class TestMotorChan { Stopped, A, B };

bool update_test_motor(TestMotorChan output) {
  static TestMotorChan previous_output = TestMotorChan::Stopped;
  static bool needs_zero = true;
  static bool started = false;
  if (output == TestMotorChan::Stopped) {
    command_motors(0, 0);
    return true;
  }
  if (output != previous_output) {
    previous_output = output;
    needs_zero = true;
    started = false;
    Serial.println("Lower spin throttle to zero.");
  }
  const float throttle = radio_state.spin.load();
  if (needs_zero) {
    command_motors(0, 0);
    if (throttle == 0) {
      needs_zero = false;
      Serial.println("Raise throttle to desired test speed to start.");
    }
    return false;
  }
  command_motors(output == TestMotorChan::A ? throttle : 0, output == TestMotorChan::B ? throttle : 0);
  if (throttle > 0) started = true;
  return started;
}

char ask(const char* question, const char* answers, TestMotorChan output = TestMotorChan::Stopped) {
  // Start each physical output at zero before accepting an observation.
  while (!update_test_motor(output)) {
    while (Serial.available()) Serial.read();
    delay(1);
  }
  Serial.println(question);
  while (true) {
    const bool ready = update_test_motor(output);
    if (Serial.available()) {
      char answer = Serial.read();
      if (answer >= 'a' && answer <= 'z') answer -= 'a' - 'A';
      if (ready && std::strchr(answers, answer) && answer != '\0') return answer;
    }
    delay(1);
  }
}

void send_zero_for(unsigned long duration_ms) {
  const unsigned long started = millis();
  do {
    command_motors(0, 0);
    delay(1);
  } while (millis() - started < duration_ms);
}
}  // namespace

MotorSelection run_motor_selection() {
  MotorSelection result;
  MotorConfig test_config = global_config.motor_config;
  const bool dual = test_config.count == MotorCount::Dual;
  // Test physical outputs, independent of existing left/right direction settings.
  // Initialise both outputs even for a single motor so B can be tried if A is unused.
  test_config.count = MotorCount::Dual;
  test_config.left_direction = 1;
  test_config.right_direction = 1;
  if (!setup_motors(test_config)) {
    Serial.println("Motor initialisation failed; selection cancelled.");
    return result;
  }
  send_zero_for(test_config.startup_delay_ms);

  while (!Serial) {
    command_motors(0, 0);
    delay(1);
  }
  ask("WARNING: Motors will spin during wizzard. Secure the robot with wheels clear. Connect transmitter and enter Y to confirm.",
      "Y");

  // select output channel/pin numbers that we are choosing between
  const int outputA = test_config.protocol == MotorProtocol::CRSF
                          ? test_config.crsf.channelA
                          : test_config.pinA;
  const int outputB = test_config.protocol == MotorProtocol::CRSF
                          ? test_config.crsf.channelB
                          : test_config.pinB;

  if (dual) {
    const char wheel = ask("Output A: which motor is spinning? L = left, R = right.", "LR", TestMotorChan::A);
    const int directionA = ask("Is that wheel spinning forwards or backwards? F = forwards, B = backwards.",
                               "FB", TestMotorChan::A) == 'F'
                               ? 1
                               : -1;
    send_zero_for(100);
    const int directionB = ask("Output B (the other wheel). F = forwards, B = backwards.",
                               "FB", TestMotorChan::B) == 'F'
                               ? 1
                               : -1;
    result.L_chan = wheel == 'L' ? outputA : outputB;
    result.R_chan = wheel == 'R' ? outputA : outputB;
    result.L_dir = wheel == 'L' ? directionA : directionB;
    result.R_dir = wheel == 'R' ? directionA : directionB;
    result.valid = true;
  } else {
    const bool onA = ask("Output A: is the motor spinning? Y = yes, N = no.", "YN", TestMotorChan::A) == 'Y';
    bool spinning = onA;
    if (!onA) {
      send_zero_for(100);
      spinning = ask("Output B: is the motor spinning? Y = yes, N = no.", "YN", TestMotorChan::B) == 'Y';
    }
    if (spinning) {
      result.L_chan = onA ? outputA : outputB;
      result.L_dir = ask("Is the motor moving forwards or backwards? F = forwards, B = backwards.",
                         "FB", onA ? TestMotorChan::A : TestMotorChan::B) == 'F'
                         ? 1
                         : -1;
      result.valid = true;
    } else {
      Serial.println("Neither output moved the motor. Check connections and motor settings.");
    }
  }

  // Allow rate-limited protocols to transmit zero before returning to the caller.
  const unsigned long interval_us = test_config.protocol == MotorProtocol::CRSF
                                        ? test_config.crsf.update_interval_us
                                        : test_config.dshot.update_interval_us;
  send_zero_for(interval_us / 1000UL + 100UL);
  Serial.println(result.valid ? "Motor selection complete. Motors stopped." : "Motor selection failed. Motors stopped.");
  return result;
}
