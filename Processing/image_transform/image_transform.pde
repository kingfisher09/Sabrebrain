import processing.video.*;

// ================================================================
// SETTINGS
// ================================================================

final int STILL = 0;
final int VIDEO = 1;

int mode = STILL;

String image_name = "dreadnought_logo";

// Used in STILL mode
String image_path = "G:\\My Drive\\Robot\\Featherweight\\Dreadnought\\Dreadnought logo.png";

// Used in VIDEO mode
String video_path = "C:\\Users\\ofish\\Pictures\\Sabretooth\\Haloween\\Haloween no dog.mp4";

// If false, videos just loop for previewing without exporting
boolean record = true;

// Rotate input image/video by 180 degrees
boolean rotate_input = false;


// ================================================================
// POV SETTINGS
// ================================================================

int numAngles = 150; // angular resolution — e.g. number of steps in one rotation
int numLEDs = 23;    // how many LEDs per spoke/radius
int numRadii = numLEDs + 1;  // Number of radial slices +1 because we calculate the center but don't use it

int canvas_size = 500;
int canv_centre = canvas_size / 2;

String output_folder = "C:\\Git\\Sabrebrain\\Sabrebrain";


// ================================================================
// GLOBALS
// ================================================================

Movie myVideo;
PImage stillImage;

PGraphics rectGraphic; // For the rectangular graphic
PGraphics polarGraphic; // For the polar-transformed graphic

color[][] output_array = new color[numAngles][numLEDs];

boolean stillExported = false;


// ================================================================
// SETUP
// ================================================================

void setup() {
  size(1000, 500); // One window, split into two halves

  rectGraphic = createGraphics(canvas_size, canvas_size); // Rectangular graphic
  polarGraphic = createGraphics(canvas_size, canvas_size); // Polar-transformed graphic

  if (mode == STILL) {
    stillImage = loadImage(image_path);

    if (stillImage == null) {
      println("ERROR: Could not load image:");
      println(image_path);
      exit();
      return;
    }

    println("Image loaded");
  }

  if (mode == VIDEO) {
    myVideo = new Movie(this, video_path);

    if (record) {
      myVideo.play();
    } else {
      myVideo.loop();  // plays and loops automatically
    }

    println("Video loaded");
    println("Frame rate: " + myVideo.frameRate);
  }
}


// ================================================================
// MAIN LOOP
// ================================================================

void draw() {

  if (mode == STILL) {
    drawStill();
  }

  if (mode == VIDEO) {
    drawVideo();
  }
}


// ================================================================
// STILL IMAGE
// ================================================================

void drawStill() {

  if (!stillExported) {
    createRectGraphic(stillImage);
    createPolarGraphic();
    saveStill();

    stillExported = true;
  }

  background(0);

  // Draw the rectangular graphic on the left
  image(rectGraphic, 0, 0);

  // Draw the polar-transformed graphic on the right
  image(polarGraphic, canvas_size, 0);
}


// ================================================================
// VIDEO
// ================================================================

void drawVideo() {

  if (myVideo.available()) {
    myVideo.read();

    background(0);

    createRectGraphic(myVideo);
    createPolarGraphic();

    if (record) {
      captureAndExportFrame();
    }

    // Draw the rectangular graphic on the left
    image(rectGraphic, 0, 0);

    // Draw the polar-transformed graphic on the right
    image(polarGraphic, canvas_size, 0);
  }

  if (record && myVideo.time() >= myVideo.duration()) {
    println("Video finished, saving");

    String outPath = output_folder + File.separator + image_name + ".h";

    writePOVExport(
      image_name,
      outPath,
      Math.round(1000.0 / myVideo.frameRate)
    );

    noLoop();
  }
}


// ================================================================
// CREATE RECTANGULAR SOURCE IMAGE
// ================================================================

void createRectGraphic(PImage source) {

  rectGraphic.beginDraw();
  rectGraphic.background(0);

  if (rotate_input) {
    rectGraphic.pushMatrix();
    rectGraphic.translate(canvas_size, canvas_size);
    rectGraphic.rotate(PI);
  }

  rectGraphic.image(source, 0, 0, canvas_size, canvas_size);

  if (rotate_input) {
    rectGraphic.popMatrix();
  }

  rectGraphic.endDraw();
}


// ================================================================
// CONVERT RECTANGULAR IMAGE TO POLAR LED DATA
// ================================================================

void createPolarGraphic() {
  polarGraphic.beginDraw();
  polarGraphic.background(0);

  polarGraphic.translate(canv_centre, canv_centre); // sets 0 at centre of canvas

  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI); // Map index to angle (0 to 2π)

    for (int r = 1; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, canvas_size / 2); // Map index to radius (center to edge)

      int x = int(radius * sin(theta));
      int y = int(radius * cos(theta));

      color colour = rectGraphic.get(x + canv_centre, y + canv_centre);

      output_array[a][r - 1] = colour;

      if (red(colour) + green(colour) + blue(colour) > 0) {
        polarGraphic.fill(colour);
        polarGraphic.noStroke();
        polarGraphic.ellipse(x, y, 6, 6);
      }
    }
  }

  polarGraphic.endDraw();
}


// ================================================================
// EXPORT STILL IMAGE
// ================================================================

void saveStill() {

  StringBuilder arduinoArray = new StringBuilder();

  String guardName = image_name.toUpperCase() + "_H";

  arduinoArray.append("#ifndef " + guardName + "\n#define " + guardName + "\n\n");

  arduinoArray.append("const CRGB " + image_name + "[" + numAngles + "][" + numLEDs + "] = {\n{");

  for (int a = 0; a < numAngles; a++) {
    for (int r = 0; r < numLEDs; r++) {
      color colour = output_array[a][r];

      arduinoArray.append(
        "CRGB(" +
        (int)red(colour) + "," +
        (int)green(colour) + "," +
        (int)blue(colour) + ")"
      );

      if (r < numLEDs - 1) {
        arduinoArray.append(", ");
      }
    }

    arduinoArray.append((a < numAngles - 1) ? "},\n{" : "}");
  }

  arduinoArray.append("};\n\n#endif");

  String outPath = output_folder + File.separator + image_name + ".h";

  saveStrings(
    outPath,
    new String[] {arduinoArray.toString()}
  );

  println("Still image saved:");
  println(outPath);
}
