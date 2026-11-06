import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
import com.nokia.mid.ui.FullCanvas;
public class GraphicsMIDlet extends MIDlet {
    protected void startApp(){Regions canvas=new Regions();if(!(canvas instanceof Canvas))throw new RuntimeException("FullCanvas must extend Canvas");Display.getDisplay(this).setCurrent(canvas);}
    protected void pauseApp(){}
    protected void destroyApp(boolean unconditional){}
}
class Regions extends FullCanvas {
    private final Image image=Image.createRGBImage(new int[]{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff,0xff000000,0xffffff00},3,2,false);
    protected void paint(Graphics g){
        g.setClip(0,0,240,320);g.setColor(0x808080);g.fillRect(0,0,240,320);
        for(int t=0;t<8;t++)g.drawRegion(image,0,0,3,2,t,10+t*10,10,Graphics.TOP|Graphics.LEFT);
        g.setClip(1,1,2,1);g.drawRegion(image,0,0,3,2,0,0,0,Graphics.TOP|Graphics.LEFT);
        g.drawRegion(image,0,0,3,2,0,200,200,Graphics.TOP|Graphics.LEFT);
    }
}
