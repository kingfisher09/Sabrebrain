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
  rectGraphic.rotate(PI);

  wrapText("SABRETOOTH", 132, color(255, 0, 0));
  //wrapText("HE HE HE", 165, color(0, 255, 0));
  draw_outer_ring();  
  //draw_white_flash();  
  draw_pointer_arrow();
  draw_image();
  rectGraphic.endDraw();
}

void createPolarGraphic() {
  polarGraphic.beginDraw();
  polarGraphic.background(0);

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
        polarGraphic.ellipse(x, y, 6, 6);
      }
    }
  }
  polarGraphic.endDraw();
}


void wrapText(String txt, float start_angle, color text_col) {
  float angleStep = radians(30);
  float radius = 130;
  
  PFont boldFont;
  String fontlink = "C:\\WINDOWS\\FONTS\\BRLNSR.TTF";
  boldFont = createFont(fontlink, 120); // Load the bold font from the data folder
  rectGraphic.textFont(boldFont);
  rectGraphic.fill(text_col);

  for (int i = 0; i < txt.length(); i++) {
    char letter = txt.charAt(i);
    float angle = radians(start_angle) + angleStep * i; // Adjust for starting position at the top

    // Calculate position on the circle
    float x = cos(angle) * radius;
    float y = sin(angle) * radius;

    // Save current transformation state
    rectGraphic.pushMatrix();

    // Translate and rotate to align text with curve
    rectGraphic.textAlign(CENTER);
    rectGraphic.translate(x, y);
    rectGraphic.rotate(angle + HALF_PI); // Rotate to align with tangent
    rectGraphic.text(letter, 0, 0); // Draw character

    // Restore transformation state
    rectGraphic.popMatrix();
  }
}
