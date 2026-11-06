package javax.microedition.lcdui;
public class Graphics {
    public static final int TOP=16, LEFT=4;
    public void setColor(int rgb){}
    public void fillRect(int x,int y,int width,int height){}
    public void drawRect(int x,int y,int width,int height){}
    public void drawString(String text,int x,int y,int anchor){}
    public void drawRGB(int[] rgbData,int offset,int scanlength,int x,int y,int width,int height,boolean processAlpha){}
    public void setClip(int x,int y,int width,int height){}
    public void drawRegion(Image image,int x,int y,int width,int height,int transform,int dx,int dy,int anchor){}
    public void setGrayScale(int value){} public int getGrayScale(){return 0;}
    public void drawChar(char c,int x,int y,int anchor){}
    public void setFont(Font font){} public Font getFont(){return null;}
}
