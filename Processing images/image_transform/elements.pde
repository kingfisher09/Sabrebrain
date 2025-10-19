abstract class Element {
  abstract void draw(PGraphics g);
}

class RingElement extends Element {
  float diameter;
  color col;
  float weight;

  RingElement(float d, color c, float w) {
    diameter = d;
    col = c;
    weight = w;
  }

  void draw(PGraphics g) {
    g.stroke(col);
    g.strokeWeight(weight);
    g.noFill();
    g.ellipse(0, 0, diameter, diameter);
  }
}


class PointerArrowElement extends Element {
  color col;
  float weight;

  PointerArrowElement(color c, float w) {
    col = c;
    weight = w;
  }

  void draw(PGraphics g) {
    g.stroke(col);
    g.strokeWeight(weight);
    g.strokeCap(SQUARE);
    g.line(20, canv_centre-20, canv_centre/3.5, 50);
    g.line(-20, canv_centre-20, -canv_centre/3.5, 50);
  }
}


class ImageElement extends Element {
  PImage img;
  float x, y, scaleFactor;

  ImageElement(String path, float s) {
    img = loadImage(path);
    scaleFactor = s;
  }

  void draw(PGraphics g) {
    g.pushMatrix();
    g.rotate(PI); // same orientation correction
    g.image(img, -(scaleFactor * img.width/2),
      -(scaleFactor * img.height/2),
      scaleFactor * img.width,
      scaleFactor * img.height);
    g.popMatrix();
  }
}

class ArcSegmentElement extends Element {
  float diameter;
  float startDeg, endDeg;
  color col;

  ArcSegmentElement(float d, float start, float end, color c) {
    diameter = d;
    startDeg = start;
    endDeg = end;
    col = c;
  }

  void draw(PGraphics g) {
    g.pushStyle();
    g.fill(col);
    g.noStroke();
    g.arc(0, 0, diameter, diameter, radians(startDeg), radians(endDeg), PIE);
    g.popStyle();
  }
}

class FlashElement extends Element {
  color col;
  float alpha;

  FlashElement(color c) {
    col = c;
  }

  void draw(PGraphics g) {
    g.pushStyle();
    g.fill(col, 255);
    g.noStroke();
    g.rectMode(CENTER);
    g.rect(0, 0, canvas_size, canvas_size);
    g.popStyle();
  }
}

class WrappedTextElement extends Element {
  String txt;
  float midAngleDeg;   // now you specify midpoint instead of start
  float angleStepDeg;
  float radius;
  color col;
  String fontPath;
  float fontSize;

  // Uses global defaults for fontPath/fontSize
  WrappedTextElement(String t, float mid, float step, float r, color c) {
    this(t, mid, step, r, c, defaultFontPath, defaultFontSize);
  }

  WrappedTextElement(String t, float mid, float step, float r, color c, String font, float size) {
    txt = t;
    midAngleDeg = mid;
    angleStepDeg = step;
    radius = r;
    col = c;
    fontPath = font;
    fontSize = size;
  }

  void draw(PGraphics g) {
    g.pushMatrix();
    g.pushStyle();

    PFont font = createFont(fontPath, fontSize);
    g.textFont(font);
    g.fill(col);
    g.textAlign(CENTER);

    // compute the starting angle so text is centered on midAngle
    float totalWidth = (txt.length() - 1) * angleStepDeg;
    float startAngleDeg = midAngleDeg - totalWidth / 2;

    for (int i = 0; i < txt.length(); i++) {
      char letter = txt.charAt(i);
      float angle = radians(startAngleDeg + angleStepDeg * i);
      float x = cos(angle) * radius;
      float y = sin(angle) * radius;

      g.pushMatrix();
      g.translate(x, y);
      g.rotate(angle + HALF_PI);
      g.text(letter, 0, 0);
      g.popMatrix();
    }

    g.popStyle();
    g.popMatrix();
  }
}

class GifElement extends Element {
  PApplet parent;
  int currentFrame = 0;
  int totalFrames;
  float frameLengthMS;
  float lastFrameTime = 0;
  int[] frameDelays;
  PImage[] animation;

  GifElement(PApplet parent_, String path) {
    parent = parent_;
    gif = new Gif(parent, path);
    gif.play();
    animation = Gif.getPImages(parent, path);
    totalFrames = animation.length;
    frameDelays = getGifFrameDelays(path);
  }

  void draw(PGraphics g) {

    // Advance frames manually based on delay timing
    if (millis() - lastFrameTime >= frameDelays[currentFrame]) {
      currentFrame = (currentFrame + 1) % totalFrames;
      lastFrameTime = millis();
    }

    //// Draw the current GIF frame
    PImage frame = animation[currentFrame];
    g.pushMatrix();
    g.translate(canv_centre, canv_centre);
    g.rotate(PI);
    g.background(0);
    g.image(frame, 0, 0, canvas_size, canvas_size);
    g.popMatrix();
  }

  // Helper to get current frame for polar conversion
  //PImage getCurrentFrame() {
  //  return gif.getFrame(currentFrame);
  //}
}
