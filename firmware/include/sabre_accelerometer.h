#pragma once

// Call these from core 1 only; setup starts a nonblocking power reset.
void setup_accelerometer();
bool accelerometer_ready();

// Advances startup/retries. Returns true only when a new filtered m/s^2
// reading has been written; leaves the supplied value unchanged otherwise.
bool update_accelerometer(float& filtered_acceleration);
