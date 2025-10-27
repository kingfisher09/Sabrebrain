class SceneEvent {
  Element element;
  float startTime;
  float finishTime;

  SceneEvent(Element e, float start, float finish) {
    element = e;
    startTime = start;
    finishTime = finish;
  }

  boolean isActive(float t) {
    return t >= startTime && (finishTime < 0 || t < finishTime);
  }

  void draw(PGraphics g, float t) {
    if (isActive(t)) element.draw(g, t - startTime);
  }
}

class Scene {
  ArrayList<SceneEvent> events;
  float startMillis;

  Scene() {
    events = new ArrayList<SceneEvent>();
    startMillis = millis();
  }

  void restart() {
    startMillis = millis();
  }

  void add(Element e, float start, float finish) {
    events.add(new SceneEvent(e, start, finish));
  }

  void draw(PGraphics g) {

    g.pushMatrix();
    g.pushStyle();
    g.background(0);
    g.translate(canv_centre, canv_centre);
    g.rotate(PI);

    for (SceneEvent ev : events) ev.draw(g, millis());

    g.popStyle();
    g.popMatrix();
  }
}


void createScenes() {

  //// Pride image scene
  //Scene pride_image = new Scene();
  //pride_image.add(new ImageElement("C:\\Users\\ofish\\Pictures\\\\Sabretooth\\Pride roundle.png", 1.0));
  //scenes.put("pride_image", pride_image);


  // Hush Gif scene
  Scene hushGifScene = new Scene();
  // Create the element once (can be reused later)
  GifElement hushGif = new GifElement(this, "C:\\Users\\ofish\\Pictures\\Sabretooth\\hush boom.gif");
  // Add the element to the scene timeline (start, finish in milliseconds)
  hushGifScene.add(hushGif, 0, -1);  //

  // --- Text reveal setup ---
  String word = "SABRETOOTH";
  float x = 0;
  float y = 0;
  color textColor = color(255, 0, 0);
  int letterDelay = 200;      // ms between letters
  int letterVisible = 500;    // ms visible time for each letter
  int startTime = 3000;      // start showing text at 3s
  float fontSize = 200;

  // Loop through letters
  for (int i = 0; i < word.length(); i++) {
    char letter = word.charAt(i);
    String letterStr = str(letter);
    StraightTextElement letterElement = new StraightTextElement(
      letterStr, x, y, textColor, fontSize
      );

    // start and stop timing for this letter
    int appear = startTime + i * (letterDelay + letterVisible);
    int disappear = appear + letterVisible;

    // each letter appears briefly, then disappears before next
    hushGifScene.add(letterElement, appear, disappear);
  }

  hushGifScene.add(new FlashElement(color(255,255,255)), hushGif.duration, hushGif.duration + 500); 
  scenes.put("hush_gif", hushGifScene);
}
