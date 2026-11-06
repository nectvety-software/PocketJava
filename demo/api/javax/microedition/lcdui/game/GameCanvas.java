package javax.microedition.lcdui.game;
import javax.microedition.lcdui.*;
public class GameCanvas extends Canvas {
    protected GameCanvas(boolean suppressKeyEvents){}
    protected Graphics getGraphics(){return null;}
    public int getKeyStates(){return 0;}
    public void flushGraphics(){} public void flushGraphics(int x,int y,int w,int h){}
    public void paint(Graphics g){}
}
