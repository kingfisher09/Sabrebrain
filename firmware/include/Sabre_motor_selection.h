#pragma once

// Channels are configured CRSF channel numbers or GPIO pins for DShot/OneShot.
// A single motor uses L_chan/L_dir; an unused channel is -1.
struct MotorSelection {
    int R_chan = -1;
    int R_dir = 1;
    int L_chan = -1;
    int L_dir = 1;
    bool valid = false;  // False if setup failed or neither single-motor output moved.
};

MotorSelection run_motor_selection(); 
