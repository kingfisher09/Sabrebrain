import processing.video.*;
Movie myVideo;

int numAngles = 150; // angular resolution — e.g. number of steps in one rotation
int numLEDs = 23;    // how many LEDs per spoke/radius
int numRadii = numLEDs + 1;  // Number of radial slices +1 because we calculate the center but don't use it

String image_name = "haloween_vid";
String video_path = "C:\\Users\\ofish\\Pictures\\Sabretooth\\Haloween\\Haloween no dog.mp4";

//String image_name = "bouncing_pumpkin";
//String video_path = "C:\\Users\\ofish\\Pictures\\Sabretooth\\Haloween\\Bouncing pumpkin.mp4";

boolean record = true;

PGraphics rectGraphic; // For the rectangular graphic
PGraphics polarGraphic; // For the polar-transformed graphic
int canvas_size = 500;
int canv_centre = canvas_size/2;
color[][] output_array = new color[numAngles][0];



void setup() {
  size(1000, 500); // One window, split into two halves
  rectGraphic = createGraphics(canvas_size, canvas_size); // Rectangular graphic
  polarGraphic = createGraphics(canvas_size, canvas_size); // Polar-transformed graphic

  myVideo = new Movie(this, video_path);
  if (record) {
    myVideo.play();
  } else {
    myVideo.loop();  // plays and loops automatically
  }
  println("loaded");
  println("Frame rate: " + myVideo.frameRate);
}

void draw() {
  if (myVideo.available()) {
    background(0);
    createRectGraphic();
    createPolarGraphic();

    if (record) {
      captureAndExportFrame();
    }

    // Draw the rectangular graphic on the left
    image(rectGraphic, 0, 0);

    // Draw the polar-transformed graphic on the right
    image(polarGraphic, canvas_size, 0);
  }
  
  if (record) {
    if (myVideo.time() >= myVideo.duration()) {
      println("Video finished, saving");
      String outPath = "C:\\Git\\Sabrebrain\\Sabrebrain" + File.separator + image_name + ".h";
      writePOVExport(image_name, outPath, Math.round(1000.0 / myVideo.frameRate));
      noLoop();
    }
  }
}

void createRectGraphic() {

  rectGraphic.beginDraw();
  rectGraphic.background(0);
  myVideo.read();
  rectGraphic.image(myVideo, 0, 0, canvas_size, canvas_size);
  rectGraphic.endDraw();
}

void createPolarGraphic() {
  polarGraphic.beginDraw();
  polarGraphic.background(0);

  polarGraphic.translate(canv_centre, canv_centre); // sets 0 at centre of canvas

  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI); // Map index to angle (0 to 2π)
    for (int r = 1; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, canvas_size / 2); // Map index to radius (center to edge)

      int x = int(min(radius * sin(theta), canvas_size - 1)); // lock max value to the size of the pixel array
      int y = int(min(radius * cos(theta), canvas_size - 1)); // lock max value to the size of the pixel array

      color colour = rectGraphic.get(x + canv_centre, y + canv_centre);

      if (red(colour) + green(colour) + blue(colour) > 0) {
        polarGraphic.fill(colour);
        polarGraphic.noStroke();
        polarGraphic.ellipse(x, y, 6, 6);
      }

      output_array[a] = append(output_array[a], colour);
    }
  }
  polarGraphic.endDraw();
}
