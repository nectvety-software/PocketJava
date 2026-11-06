import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
public class MonitorMIDlet extends MIDlet {
    static final Object lock=new Object();
    static int stage;static boolean held,done;
    protected void startApp(){
        boolean caught=false;try{synchronized((Object)null){stage=99;}}catch(NullPointerException expected){caught=true;}
        if(!caught)throw new RuntimeException("Null monitor accepted");
        synchronized(lock){synchronized(lock){stage=1;}}
        try{synchronized(lock){throw new RuntimeException("Release on exception");}}catch(RuntimeException expected){}
        Display.getDisplay(this).setCurrent(new MonitorCanvas());
        new Thread(new Holder()).start();new Thread(new Waiter()).start();
    }
    protected void pauseApp(){} protected void destroyApp(boolean unconditional){}
}
class Holder implements Runnable {
    public void run(){synchronized(MonitorMIDlet.lock){MonitorMIDlet.held=true;
        try{Thread.sleep(66);}catch(Exception e){throw new RuntimeException();}
        if(MonitorMIDlet.stage!=1)throw new RuntimeException("Lock did not exclude waiter");
        MonitorMIDlet.stage=2;MonitorMIDlet.held=false;
    }}
}
class Waiter implements Runnable {
    public void run(){synchronized(MonitorMIDlet.lock){
        if(MonitorMIDlet.held||MonitorMIDlet.stage!=2)throw new RuntimeException("Incorrect lock order");
        MonitorMIDlet.stage=3;MonitorMIDlet.done=true;
    }}
}
class MonitorCanvas extends Canvas {
    protected void paint(Graphics g){if(MonitorMIDlet.done)g.drawString("MONITORS PASS",4,4,Graphics.TOP|Graphics.LEFT);}
}
