#pragma once

// Explicit values are stored in /SYSTEM/next_boot_mode.bin; keep them stable.
enum class BootMode { Robot = 0, USBStorage = 1, MotorSelection = 2, Passthrough = 3 };

// Core 0 reads/resets the request and loads Robot/wizard config after mounting FatFS.
// Missing/invalid config selects USB storage before publishing the final mode.
void load_config_and_select_boot_mode();
BootMode boot_mode();
bool boot_mode_selected();

// Core 1 saves serial mode requests between its other filesystem operations.
// Requests take effect only after a manual power cycle.
void serial_update_boot_mode();

// Used after a successful wizard save. Returns false only if the request cannot be saved.
bool restart_in_file_transfer_mode();
