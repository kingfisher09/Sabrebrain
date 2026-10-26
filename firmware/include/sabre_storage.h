#pragma once

#include "config/sabre_calibration.h"
#include "Sabre_motor_selection.h"

bool storage_setup();
bool load_calibration(SabreCalibration& cal);
bool save_calibration(const SabreCalibration& cal);
bool validate_calibration(const SabreCalibration& cal);
bool save_motor_selection(const MotorSelection& selection);
