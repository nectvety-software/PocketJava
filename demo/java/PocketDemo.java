import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
/** Real Java bytecode. Uses only standard MIDP APIs. No native game logic. */
public final class PocketDemo extends MIDlet {
    private DemoCanvas screen;
    public PocketDemo() {screen=new DemoCanvas();}
    protected void startApp(){Display.getDisplay(this).setCurrent(screen);}
    protected void pauseApp(){}
    protected void destroyApp(boolean unconditional){}
}
final class DemoCanvas extends Canvas {
    private int x=112,y=170,tick=0;
    private int[] stars={12,42,84,116,170,210};
    public DemoCanvas(){setFullScreenMode(true);}
    protected void keyPressed(int key){
        int action=getGameAction(key);
        if(action==1)y-=8;if(action==6)y+=8;
        if(action==2)x-=8;if(action==5)x+=8;
        x=Math.max(8,Math.min(216,x));y=Math.max(98,Math.min(258,y));repaint();
    }
    protected void paint(Graphics g){
        tick=(tick+1)%200;
        g.setColor(0x101b2d);g.fillRect(0,0,getWidth(),getHeight());
        g.setColor(0x23354e);for(int i=0;i<stars.length;i++)g.fillRect(stars[i],96+(i*31+tick)%160,2,2);
        g.setColor(0x44dab0);g.fillRect(0,0,240,5);
        g.drawString("POCKET JAVA",16,20,Graphics.TOP|Graphics.LEFT);
        g.setColor(0xffffff);g.drawString("REAL MIDLET / BYTECODE",16,47,20);
        g.setColor(0x7e96b3);g.drawString("240 x 320  |  MRE",16,71,20);
        g.setColor(0x44dab0);g.fillRect(x,y,16,16);
        g.setColor(0xffffff);g.fillRect(x+5,y+4,6,5);
        g.setColor(0x23354e);g.fillRect(12,280,216,28);
        g.setColor(0xd4e5ff);g.drawString("2 4 6 8 : MOVE",24,286,20);
    }
}
