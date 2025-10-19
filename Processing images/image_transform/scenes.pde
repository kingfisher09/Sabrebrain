class Scene {
  ArrayList<Element> elements;
  float frameRate; // optional
  int frameCount;  // optional

  Scene() {
    elements = new ArrayList<Element>();
    frameCount = 1;
  }

  void add(Element e) {
    elements.add(e);
  }

  void draw(PGraphics g) {
    g.pushMatrix();
    g.background(0);
    g.translate(canv_centre, canv_centre);
    g.rotate(PI);
    for (Element e : elements) e.draw(g);
    g.popMatrix();
  }
}

void createScenes() {

  // Pride image scene
  Scene pride_image = new Scene();
  pride_image.add(new ImageElement("C:\\Users\\ofish\\Pictures\\\\Sabretooth\\Pride roundle.png", 1.0));
  scenes.put("pride_image", pride_image);


  // Hush Gif scene
  Scene Hush_gif = new Scene();
  Hush_gif.add(new GifElement(this, "C:\\Users\\ofish\\Pictures\\\\Sabretooth\\Hush gif.gif"));
  scenes.put("hush_gif", Hush_gif);
}
