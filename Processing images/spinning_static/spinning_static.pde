int numAngles = 150; // Number of angle slices (e.g., LEDs per ring)
int numRadii = 24;  // Number of radial slices
float[][][] polarData = new float[numAngles][numRadii][3]; // 3D array for [angle][radius][RGB]

void setup() {
  size(800, 800);
  background(0);
  translate(width / 2, height / 2);
  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI); // Map index to angle (0 to 2π)
    for (int r = 0; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, width / 2); // Map index to radius (center to edge)
      float x = radius * sin(theta);
      float y = radius * -cos(theta);
      int red = 0;
      int green = 0;
      int blue = 0;

      // Example color gradient based on radius and angle
      //float red = map(a, 0, numAngles, 0, 255);
      //float green = map(r, 0, numRadii, 0, 255);
      //float blue = 255 - red;
      int red_stripe_width = 4;

      if (a <= red_stripe_width || a >= numAngles - red_stripe_width) {
        red = 255;
      }


      // Store RGB values in the array
      polarData[a][r][0] = red;
      polarData[a][r][1] = green;
      polarData[a][r][2] = blue;

      if (red + green + blue > 0) {  // skip drawing if pixel is black
        // Visualize the same data on the canvas
        fill(red, green, blue);
        noStroke();
        ellipse(x, y, 12, 12);
      }
    }
  }
}
