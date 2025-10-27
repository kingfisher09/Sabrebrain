abstract class Element {
  int duration = -1;
  // subclasses implement this to define what they draw
  abstract void draw(PGraphics g, float t);
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

  void draw(PGraphics g, float t) {
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

  void draw(PGraphics g, float t) {
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

  void draw(PGraphics g, float t) {
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

  void draw(PGraphics g, float t) {
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

  void draw(PGraphics g, float t) {
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
  boolean flip;
  int flipper;

  // Uses global defaults for fontPath/fontSize
  WrappedTextElement(String t, float mid, float step, float r, color c, boolean f) {
    this(t, mid, step, r, c, f, defaultFontSize);
  }

  WrappedTextElement(String t, float mid, float step, float r, color c, boolean f, float size) {
    txt = t;
    midAngleDeg = mid;
    angleStepDeg = step;
    col = c;
    radius = r;
    fontPath = defaultFontPath;
    fontSize = size;
    flip = f;
  }

  void draw(PGraphics g, float t) {
    int flipper = flip ? -1 : 1;  // 1 for normal, -1 for flipped

    g.pushMatrix();
    g.pushStyle();

    PFont font = createFont(fontPath, fontSize);
    g.textFont(font);
    g.fill(col);
    g.textAlign(CENTER);
    float textHeight = g.textAscent() + g.textDescent();
    float drawrad = flip ? radius + textHeight : radius;

    // compute the starting angle so text is centered on midAngle
    float totalWidth = (txt.length() - 1) * angleStepDeg;
    float startAngleDeg = midAngleDeg * flipper - totalWidth / 2;

    for (int i = 0; i < txt.length(); i++) {
      char letter = txt.charAt(i);
      float angle = radians(startAngleDeg + angleStepDeg * i) * flipper;

      float x = cos(angle) * drawrad;
      float y = sin(angle) * drawrad;

      g.pushMatrix();
      g.translate(x, y);

      // adjust letter orientation based on flip
      if (flip) {
        g.rotate(angle - HALF_PI);
      } else {
        g.rotate(angle + HALF_PI);
      }

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
  float lastFrameTime = 0;
  int[] frameDelays;
  PImage[] animation;
  boolean reverse = false;  // ✅ new flag

  GifElement(PApplet parent_, String path) {
    this(parent_, path, false);  // default = forward
  }

  GifElement(PApplet parent_, String path, boolean reverse_) {
    parent = parent_;
    reverse = reverse_;
    gif = new Gif(parent, path);
    animation = Gif.getPImages(parent, path);
    totalFrames = animation.length;
    frameDelays = getGifFrameDelays(path);

    duration = 0;
    for (int i = 0; i < frameDelays.length; i++) {
      duration += frameDelays[i];
    }
  }

  void draw(PGraphics g, float t) {
    // Advance frames manually based on delay timing
    if (t - lastFrameTime >= frameDelays[currentFrame]) {
      if (reverse) {
        currentFrame--;
        if (currentFrame < 0) currentFrame = totalFrames - 1;
      } else {
        currentFrame = (currentFrame + 1) % totalFrames;
      }
      lastFrameTime = t;
    }

    // Draw the current GIF frame
    PImage frame = animation[currentFrame];
    g.pushMatrix();
    g.translate(canv_centre, canv_centre);
    g.rotate(PI);
    g.background(0);
    g.image(frame, 0, 0, canvas_size, canvas_size);
    g.popMatrix();
  }
}


class StraightTextElement extends Element {
  String txt;
  float x, y;         // text position (centered by default)
  color col;
  float fontSize;
  int align;          // optional alignment (LEFT, CENTER, RIGHT)

  StraightTextElement(String t, float xpos, float ypos, color c, float size) {
    this(t, xpos, ypos, c, size, CENTER);
  }

  StraightTextElement(String t, float xpos, float ypos, color c, float size, int align_) {
    txt = t;
    x = xpos;
    y = ypos;
    col = c;
    fontSize = size;
    align = align_;
  }

  void draw(PGraphics g, float t) {
    g.pushMatrix();
    g.pushStyle();

    // flip 180° to compensate for scene rotation
    g.rotate(PI);

    PFont font = createFont(defaultFontPath, fontSize);
    g.textFont(font);
    g.fill(col);
    g.textAlign(align, CENTER);

    // draw at mirrored coordinates (since we rotated)
    g.text(txt, -x, -y);

    g.popStyle();
    g.popMatrix();
  }
}
