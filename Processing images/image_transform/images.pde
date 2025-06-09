void draw_pointer_arrow() {
  rectGraphic.stroke(255, 0, 0);
  rectGraphic.strokeWeight(20);
  rectGraphic.strokeCap(SQUARE);
  rectGraphic.line(20, canv_centre-20, canv_centre/3.5, 50);
  rectGraphic.line(-20, canv_centre-20, -canv_centre/3.5, 50);
}

void draw_outer_ring() {
  rectGraphic.stroke(0, 0, 255);
  rectGraphic.strokeWeight(10);
  rectGraphic.noFill();
  rectGraphic.ellipse(0, 0, 480, 480);
}

void draw_white_flash() {
  rectGraphic.fill(255, 255, 255);
  rectGraphic.noStroke();
  rectGraphic.rectMode(CENTER);
  rectGraphic.rect(0, 0, canvas_size, canvas_size);
}


void draw_image() {
  float scale = 1;
  PImage img;
  rectGraphic.rotate(radians(180));
  img = loadImage("C:\\Users\\ofish\\Pictures\\Sabrepic5.png");
  boolean fitwidth = true;
  if (fitwidth) {
    scale = (float)canvas_size / img.width;
  } else {
    scale = (float)canvas_size / img.height;
  }
  
  rectGraphic.image(img, -(scale * img.width/2), -(scale * img.height/2), scale * img.width, scale * img.height);
  
  rectGraphic.rotate(radians(180));  // reset image orientation
}
