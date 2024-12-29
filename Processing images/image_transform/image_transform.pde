int numAngles = 150; // Number of angle slices (e.g., LEDs per ring)
int numRadii = 24;  // Number of radial slices
float[][][] polarData = new float[numAngles][numRadii][3]; // 3D array for [angle][radius][RGB]

PGraphics rectGraphic; // For the rectangular graphic
PGraphics polarGraphic; // For the polar-transformed graphic
int canvas_size = 500;
int canv_centre = canvas_size/2;

void setup() {
  size(1000, 500); // One window, split into two halves
  rectGraphic = createGraphics(canvas_size, canvas_size); // Rectangular graphic
  polarGraphic = createGraphics(canvas_size, canvas_size); // Polar-transformed graphic

  createRectGraphic();
  createPolarGraphic();
}

void draw() {
  background(0);

  // Draw the rectangular graphic on the left
  image(rectGraphic, 0, 0);

  // Draw the polar-transformed graphic on the right
  image(polarGraphic, canvas_size, 0);
}

void createRectGraphic() {
  rectGraphic.beginDraw();
  rectGraphic.background(0);
  rectGraphic.translate(canv_centre, canv_centre); // sets 0 at centre of canvas

  // Draw some shapes (example graphic)
  rectGraphic.fill(255, 0, 0);
  rectGraphic.rectMode(CENTER);
  rectGraphic.rect(0, 0, 200, 200);

  rectGraphic.fill(0, 0, 255);
  rectGraphic.ellipse(0, 0, 100, 100);

  rectGraphic.endDraw();
}

void createPolarGraphic() {
  polarGraphic.beginDraw();
  polarGraphic.background(255);

  polarGraphic.translate(canv_centre, canv_centre); // sets 0 at centre of canvas


  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI); // Map index to angle (0 to 2π)
    for (int r = 0; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, canvas_size / 2); // Map index to radius (center to edge)

      int x = int(min(radius * sin(theta), canvas_size - 1)); // lock max value to the size of the pixel array
      int y = int(min(radius * cos(theta), canvas_size - 1)); // lock max value to the size of the pixel array

      color colour = rectGraphic.get(x + canv_centre, y + canv_centre);

      if (red(colour) + green(colour) + blue(colour) > 0) {
        polarGraphic.fill(colour);
        polarGraphic.noStroke();
        polarGraphic.ellipse(x, y, 12, 12);
      }
    }
  }
  polarGraphic.endDraw();
}
