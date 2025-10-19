import javax.imageio.*;
import javax.imageio.metadata.*;
import org.w3c.dom.*;
import java.io.*;


// Function to return array of per-frame delays in milliseconds
int[] getGifFrameDelays(String filename) {
  try {
    ImageReader r = ImageIO.getImageReadersByFormatName("gif").next();
    r.setInput(ImageIO.createImageInputStream(new File(dataPath(filename))));
    int n = r.getNumImages(true);
    int[] delays = new int[n];
    
    for (int i = 0; i < n; i++) {
      IIOMetadata m = r.getImageMetadata(i);
      Node node = m.getAsTree(m.getNativeMetadataFormatName())
                   .getFirstChild();
      while (node != null) {
        if ("GraphicControlExtension".equals(node.getNodeName())) {
          String delayStr = node.getAttributes()
                                .getNamedItem("delayTime").getNodeValue();
          int delay = int(parseInt(delayStr)) * 10; // hundredths → ms
          if (delay == 0) delay = 10; // clamp zero-delay frames
          delays[i] = delay;
        }
        node = node.getNextSibling();
      }
    }
    return delays;
  } catch (Exception e) {
    e.printStackTrace();
    return new int[0];
  }
}
