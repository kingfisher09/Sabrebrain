#include <math.h>
#include "data_array.h"

void paint_screen(angle){
  int current_line = fmod(floor((angle + half_slice)/slice_size), NUM_SLICES);  // mod wraps the slices back to 0, floor with the half slice keeps things centred around 0

  line = IMAGE_pointer
  memcpy(leds, line, sizeof(line)); // write line of LEDs to the LED array
}