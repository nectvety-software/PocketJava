import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
import javax.microedition.lcdui.game.GameCanvas;
public class GameCanvasMIDlet extends MIDlet {
    static Display screen;
    protected void startApp(){screen=Display.getDisplay(this);screen.setCurrent(new BufferedGame());}
    protected void pauseApp(){} protected void destroyApp(boolean unconditional){}
}
class BufferedGame extends GameCanvas {
    private boolean flushed;
    BufferedGame(){super(false);Graphics g=getGraphics();g.setColor(0xff0000);g.fillRect(20,100,40,30);g.setGrayScale(128);if(g.getGrayScale()!=128)throw new RuntimeException("Gray scale mismatch");g.fillRect(22,102,3,3);}
    public void paint(Graphics g){g.drawString(flushed?"GAMECANVAS PASS":"BUFFER READY",4,40,Graphics.TOP|Graphics.LEFT);}
    protected void keyPressed(int key){
        if(key==48){GameCanvasMIDlet.screen.setCurrent(new DefaultBufferedGame());return;}
        int first=getKeyStates();int second=getKeyStates();if((first&256)==0||second!=0)throw new RuntimeException("Key latch lost");
        flushGraphics(20,100,40,30);flushed=true;repaint();
    }
}
class DefaultBufferedGame extends GameCanvas {
    DefaultBufferedGame(){super(true);Graphics g=getGraphics();g.setColor(0x0000ff);g.fillRect(80,100,30,30);}
    protected void keyPressed(int key){throw new RuntimeException("Suppressed GameCanvas key delivered");}
}
