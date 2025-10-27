import gifAnimation.*;
Gif gif;

int numAngles = 150; // angular resolution — e.g. number of steps in one rotation
int numLEDs = 23;    // how many LEDs per spoke/radius
int numRadii = numLEDs + 1;  // Number of radial slices +1 because we calculate the center but don't use it
String image_name = "";

String defaultFontPath = "C:\\WINDOWS\\FONTS\\BRLNSR.TTF";
float defaultFontSize  = 120;

Scene currentScene;
HashMap<String, Scene> scenes = new HashMap<String, Scene>();
;

PGraphics rectGraphic; // For the rectangular graphic
PGraphics polarGraphic; // For the polar-transformed graphic
int canvas_size = 500;
int canv_centre = canvas_size/2;
color[][] output_array = new color[numAngles][0];


ArrayList<Element> elements = new ArrayList<Element>();  // array to hold drawing elements

void setup() {
  size(1000, 500); // One window, split into two halves
  rectGraphic = createGraphics(canvas_size, canvas_size); // Rectangular graphic
  polarGraphic = createGraphics(canvas_size, canvas_size); // Polar-transformed graphic
  createScenes();
  currentScene = scenes.get("hush_gif");  // <------------------ Scene input here!
}

void draw() {
  background(0);
  createRectGraphic();
  createPolarGraphic();
  //savePolarPoints();

  // Draw the rectangular graphic on the left
  image(rectGraphic, 0, 0);

  // Draw the polar-transformed graphic on the right
  image(polarGraphic, canvas_size, 0);
}

void createRectGraphic() {
  rectGraphic.beginDraw();
  rectGraphic.background(0);

  // Draw whatever the current scene is
  currentScene.draw(rectGraphic);
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

void savePolarPoints() {

  // --------------- Convert to Arduino-compatible syntax ---------

  StringBuilder arduinoArray = new StringBuilder();
  arduinoArray.append("#ifndef " + image_name.toUpperCase() + "_H\n#define " + image_name.toUpperCase() + "_H\n\n");

  arduinoArray.append("const CRGB " + image_name + "[" + numAngles + "][" + numLEDs + "] = {\n{");
  for (int i = 0; i < output_array.length; i++) {
    for (int j = 0; j < output_array[i].length; j++) {
      arduinoArray.append("CRGB(" + (int)red(output_array[i][j]) + "," + (int)green(output_array[i][j]) + "," + (int)blue(output_array[i][j]) + ")");  // build CRGB from colour
      if (j < output_array[i].length - 1) arduinoArray.append(", "); // Add commas between elements
    }
    arduinoArray.append((i < output_array.length - 1) ? "},\n{" : "}");
  }
  arduinoArray.append("};\n\n#endif");

  // Save to a text file
  saveStrings("C:\\Git\\Sabrebrain\\Sabrebrain\\" + image_name + ".h", new String[]{arduinoArray.toString()});

  println("Byte array saved!");
}
