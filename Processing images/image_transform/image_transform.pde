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
  image(polarGraphic, 500, 0);
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
  polarGraphic.background(0);

  polarGraphic.translate(canv_centre, canv_centre); // sets 0 at centre of canvas

  rectGraphic.loadPixels();
  //polarGraphic.loadPixels();

//print(rectGraphic.pixels[50]);

  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI); // Map index to angle (0 to 2π)
    for (int r = 0; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, canvas_size / 2); // Map index to radius (center to edge)

      int x = int(min(radius * sin(theta), canvas_size - 1)); // lock max value to the size of the pixel array
      int y = int(min(radius * cos(theta), canvas_size - 1)); // lock max value to the size of the pixel array
      
      print(x);
      print(" ");
      print(y);
      print(", ");
      int pixelColor = rectGraphic.pixels[x + y];
    }
  }

  //for (int x = 0; x < rectGraphic.width; x++) {
  //  for (int y = 0; y < rectGraphic.height; y++) {
  //    int pixelColor = rectGraphic.pixels[x + y * rectGraphic.width];

  //    // Map (x, y) to polar coordinates
  //    float angle = map(x, 0, rectGraphic.width, 0, TWO_PI);
  //    float r = map(y, 0, rectGraphic.height, 0, radius);

  //    int polarX = (int)(centerX + r * cos(angle));
  //    int polarY = (int)(centerY + r * sin(angle));

  //    if (polarX >= 0 && polarX < polarGraphic.width && polarY >= 0 && polarY < polarGraphic.height) {
  //      polarGraphic.pixels[polarX + polarY * polarGraphic.width] = pixelColor;
  //    }
  //  }
}

//polarGraphic.updatePixels();
//polarGraphic.endDraw();
