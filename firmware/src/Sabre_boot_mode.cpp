// Reads one-shot startup requests and saves serial mode selections for the next power cycle.
#include "sabre_boot_mode.h"
#include "sabre_config.h"

#include <Arduino.h>
#include <atomic>  // library designed to help 2 cores speak to eachother
#include <cstring>
#include <FatFS.h>
#include <FastLED.h>

namespace {
constexpr const char* mode_path = "/SYSTEM/next_boot_mode.bin";
std::atomic<bool> selected{false};
BootMode current_mode = BootMode::Robot;

void show_mode_colour() {
  static CRGB mode_led;
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
  FastLED.addLeds<WS2812B, PIN_NEOPIXEL, GRB>(&mode_led, 1);
  // Use modest brightness for the onboard indicator.
  switch (current_mode) {
    case BootMode::Robot: mode_led = CRGB::Black; break;
    case BootMode::USBStorage: mode_led = CRGB(0, 32, 0); break;
    case BootMode::MotorSelection: mode_led = CRGB(0, 0, 32); break;
    case BootMode::Passthrough: mode_led = CRGB(32, 0, 0); break;
  }
  FastLED.show();
}

bool save_restart_mode(BootMode mode) {  // function to save the mode for next startup
  if (!FatFS.exists("/SYSTEM") && !FatFS.mkdir("/SYSTEM")) return false;
  File file = FatFS.open(mode_path, "w");
  if (!file) return false;
  const bool written = file.write(static_cast<uint8_t>(mode)) == 1;
  file.flush();
  file.close();
  return written;
}

}  // namespace

void load_config_and_select_boot_mode() {
  current_mode = Sabre_Config::boot_mode;
  if (FatFS.exists(mode_path)) {
    File file = FatFS.open(mode_path, "r");
    const int value = file && file.size() == 1 ? file.read() : -1;
    file.close();
    if (value >= 0 && value <= static_cast<int>(BootMode::Passthrough)) {
      current_mode = static_cast<BootMode>(value);
    } else {
      Serial.println("Invalid next boot mode file; using build-time default");
    }
    // Always reset the file after reading, including zero or invalid contents.
    if (!save_restart_mode(BootMode::Robot)) {
      Serial.println("Could not reset next boot mode; robot hardware will stay disabled");
      current_mode = BootMode::MotorSelection;
    }
  }
  // Finish loading configuration before releasing core 1 or starting hardware.
  if ((current_mode == BootMode::Robot || current_mode == BootMode::MotorSelection) &&
      !load_config(global_config)) {
    Serial.println("Configuration missing or invalid; starting USB file transfer");
    current_mode = BootMode::USBStorage;
  }
  show_mode_colour();  // Register before either core starts other LED hardware.
  selected = true;
}

BootMode boot_mode() { return current_mode; }
bool boot_mode_selected() { return selected.load(); }

bool restart_in_file_transfer_mode() {
  if (!save_restart_mode(BootMode::USBStorage)) {
    Serial.println("Could not save file transfer request; motors remain stopped.");
    return false;
  }
  Serial.println("Restarting into file transfer mode.");
  Serial.flush();
  rp2040.reboot();
  while (true) delay(1);
}

void serial_update_boot_mode() {
  static bool selection_saved = false;
  static bool menu_printed = false;
  static unsigned long last_menu_ms = 0;
  if (selection_saved) return;

  const unsigned long now = millis();
  if (Serial && (!menu_printed || now - last_menu_ms >= 10000UL)) {
    Serial.println("Enter 1 for file transfer, 2 for motor selection, 3 for ESC passthrough");
    last_menu_ms = now;
    menu_printed = true;
  }

  while (Serial.available()) {
    BootMode requested_mode;
    const char* mode_name;
    switch (Serial.read()) {
      case '1':
        requested_mode = BootMode::USBStorage;
        mode_name = "file transfer";
        break;
      case '2':
        requested_mode = BootMode::MotorSelection;
        mode_name = "motor selection";
        break;
      case '3':
        requested_mode = BootMode::Passthrough;
        mode_name = "ESC passthrough";
        break;
      default:
        continue;  // Ignore Enter and any other characters.
    }
    if (!save_restart_mode(requested_mode)) {
      Serial.println("Could not save next boot mode; please try again");
      return;
    }
    selection_saved = true;
    Serial.print("Power cycle to enter ");
    Serial.println(mode_name);
    return;
  }
}
