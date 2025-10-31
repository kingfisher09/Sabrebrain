// Sabretooth Exporter (Processing)
// ----------------------------------------------------------
// Converts rectangular frames to polar format and exports
// compressed animation data for POV display.
// Each frame has per-frame mask and diffs arrays, plus duration.

// ==== 4-state diff export (Processing) ======================================
// Modes: 00=new, 01=prevFrame, 10=prevPixel, 11=prevRow
final int MODE_NEW        = 0b00;
final int MODE_PREV_FRAME = 0b01;
final int MODE_PREV_PIXEL = 0b10;
final int MODE_PREV_ROW   = 0b11;

// Geometry constants
final int pixelsPerFrame = numAngles * numRadii;
int idx(int a, int r) { return a * numRadii + r; }

// Accumulators
ArrayList<byte[]> frameMasks = new ArrayList<byte[]>();  
ArrayList<color[]> frameDiffs = new ArrayList<color[]>(); 
ArrayList<Integer> frameDurationsMs = new ArrayList<Integer>(); 

color[] prevFrameLinear = null;

// Capture a single frame into compressed form
// Pass the *frame duration* in milliseconds
void captureAndExportFrame() {
  color[] cur = new color[pixelsPerFrame];

  // --- Sample pixels into a linear array ---
  for (int a = 0; a < numAngles; a++) {
    float theta = map(a, 0, numAngles, 0, TWO_PI);
    for (int r = 1; r < numRadii; r++) {
      float radius = map(r, 0, numRadii, 0, canvas_size / 2);
      int x = int(min(radius * sin(theta), canvas_size - 1));
      int y = int(min(radius * cos(theta), canvas_size - 1));
      color c = rectGraphic.get(x + canv_centre, y + canv_centre);
      cur[idx(a, r)] = c;
    }
    cur[idx(a, 0)] = color(0);
  }

  ArrayList<Byte> maskBytes = new ArrayList<Byte>();
  ArrayList<Integer> diffs = new ArrayList<Integer>();

  int bitPos = 0;
  int currentMaskByte = 0;

  // --- Encode each pixel with 2-bit mode ---
  for (int a = 0; a < numAngles; a++) {
    for (int r = 0; r < numRadii; r++) {
      color c = cur[idx(a, r)];
      int mode;

      if (r > 0 && c == cur[idx(a, r - 1)]) {
        mode = MODE_PREV_PIXEL;
      }
      else if (a > 0 && c == cur[idx(a - 1, r)]) {
        mode = MODE_PREV_ROW;
      }
      else if (prevFrameLinear != null && c == prevFrameLinear[idx(a, r)]) {
        mode = MODE_PREV_FRAME;
      }
      else {
        mode = MODE_NEW;
        diffs.add(c);
      }

      // Pack mode bits into byte stream
      currentMaskByte |= (mode << bitPos);
      bitPos += 2;

      if (bitPos == 8) {
        maskBytes.add((byte)(currentMaskByte & 0xFF));
        currentMaskByte = 0;
        bitPos = 0;
      }
    }
  }

  // Handle partial byte at end
  if (bitPos != 0) maskBytes.add((byte)(currentMaskByte & 0xFF));

  // Store results for this frame
  frameMasks.add(toByteArray(maskBytes));
  frameDiffs.add(toColorArray(diffs));

  prevFrameLinear = cur;
}

// Conversion helpers
byte[] toByteArray(ArrayList<Byte> list) {
  byte[] out = new byte[list.size()];
  for (int i = 0; i < list.size(); i++) out[i] = list.get(i);
  return out;
}
color[] toColorArray(ArrayList<Integer> list) {
  color[] out = new color[list.size()];
  for (int i = 0; i < list.size(); i++) out[i] = list.get(i);
  return out;
}

// Write out per-frame arrays
void writePOVExport(String imageName, String outPath, int frame_time) {
  StringBuilder sb = new StringBuilder();
  String GUARD = imageName.toUpperCase() + "_H";
  sb.append("#ifndef " + GUARD + "\n#define " + GUARD + "\n\n");
  sb.append("#include \"sabre_Vid.h\"\n\n");

  // Geometry and metadata
  sb.append("// Geometry\n");
  sb.append("const uint16_t " + imageName + "_NUM_ANGLES = " + numAngles + ";\n");
  sb.append("const uint16_t " + imageName + "_NUM_RADII  = " + numRadii  + ";\n");
  sb.append("const uint16_t " + imageName + "_NUM_FRAMES = " + frameMasks.size() + ";\n");
  sb.append("const uint16_t " + imageName + "_FRAME_DURATION_MS = " + frame_time + ";\n\n");

  sb.append("// 2-bit modes: 00=new, 01=prevFrame, 10=prevPixel, 11=prevRow\n");
  sb.append("#define POV_MODE_NEW        0x0\n");
  sb.append("#define POV_MODE_PREV_FRAME 0x1\n");
  sb.append("#define POV_MODE_PREV_PIXEL 0x2\n");
  sb.append("#define POV_MODE_PREV_ROW   0x3\n\n");

  // Write all frame mask data into a 2D array
  sb.append("// ---- Masks ----\n");
  sb.append("const uint8_t " + imageName + "_masks[" + frameMasks.size() + "][" + "((" + pixelsPerFrame + " * 2 + 7) / 8)" + "] PROGMEM = {\n");
  for (int f = 0; f < frameMasks.size(); f++) {
    byte[] mask = frameMasks.get(f);
    sb.append("  {");
    for (int i = 0; i < mask.length; i++) {
      sb.append("0x" + hex(mask[i] & 0xFF, 2));
      if (i < mask.length - 1) sb.append(", ");
    }
    sb.append("}");
    if (f < frameMasks.size() - 1) sb.append(",");
    sb.append("\n");
  }
  sb.append("};\n\n");

  // Write each diff array separately
  sb.append("// ---- Diffs ----\n");
  for (int f = 0; f < frameDiffs.size(); f++) {
    color[] diffs = frameDiffs.get(f);
    sb.append("const CRGB " + imageName + "_frame" + f + "_diffs[" + diffs.length + "] PROGMEM = {");
    for (int i = 0; i < diffs.length; i++) {
      color c = diffs[i];
      sb.append("CRGB(" + (int)red(c) + "," + (int)green(c) + "," + (int)blue(c) + ")");
      if (i < diffs.length - 1) sb.append(", ");
    }
    sb.append("};\n");
  }
  sb.append("\n");

  // Pointer table for diffs
  sb.append("const CRGB* const " + imageName + "_diffs[] PROGMEM = {\n");
  for (int f = 0; f < frameDiffs.size(); f++) {
    sb.append("  " + imageName + "_frame" + f + "_diffs");
    if (f < frameDiffs.size() - 1) sb.append(",");
    sb.append("\n");
  }
  sb.append("};\n\n");

  // Final SabreVid object
  sb.append("const SabreVid " + imageName + " = {\n");
  sb.append("  " + imageName + "_NUM_ANGLES,\n");
  sb.append("  " + imageName + "_NUM_RADII,\n");
  sb.append("  " + imageName + "_NUM_FRAMES,\n");
  sb.append("  " + imageName + "_FRAME_DURATION_MS,\n");
  sb.append("  " + imageName + "_masks,\n");
  sb.append("  " + imageName + "_diffs\n");
  sb.append("};\n\n");

  sb.append("#endif // " + GUARD + "\n");

  saveStrings(outPath, new String[]{ sb.toString() });
  println("Wrote header: " + outPath + "  (" + frameMasks.size() + " frames)");
}
