#pragma once

#include "config/sabre_calibration.h"

bool load_calibration(SabreCalibration& cal);
bool save_calibration(const SabreCalibration& cal);
bool validate_calibration(const SabreCalibration& cal);