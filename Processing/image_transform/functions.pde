int encodeRGB332(color c) {
  float rAdj = red(c);
  float gAdj = green(c);
  float bAdj = blue(c) * 1.1;  // give blue a slight boost
  bAdj = constrain(bAdj, 0, 255);

  int r = int(rAdj * 7.0 / 255.0 + 0.5);
  int g = int(gAdj * 7.0 / 255.0 + 0.5);
  int b = int(bAdj * 3.0 / 255.0 + 0.5);
  return (r << 5) | (g << 2) | b;
}


color visualizeRGB332(int packed) {
  int r = (packed >> 5) & 0x07;
  int g = (packed >> 2) & 0x07;
  int b = packed & 0x03;
  return color(r * 36, g * 36, b * 85);
}
