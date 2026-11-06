import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
public class VerifyMIDlet extends MIDlet {
    static int ready=17;
    static int divisor=0;
    static int recurse(int n){return n<2?1:n*recurse(n-1);}
    static void check(boolean yes){if(!yes){int x=1/divisor;}}
    public VerifyMIDlet(){}
    protected void startApp(){
        int min=-2147483648; int neg=-1;
        check(min/neg==min);check(min%neg==0);
        check((min>>>31)==1);check((min>>31)==-1);
        check((2147483647+ready)==-2147483632);
        check(recurse(7)==5040);
        VerifyChild child=new VerifyChild();check(child.number==23);check(child.get()==23);
        int[] values=new int[8];values[3]=987;check(values[3]==987);check(values.length==8);
        for(int i=0;i<6000;i++){Object o=new Object();values=new int[12];values[4]=i;}
        check(values[4]==5999);check(child.number==23);
        Display.getDisplay(this).setCurrent(new VerifyCanvas());
    }
    protected void pauseApp(){}
    protected void destroyApp(boolean force){}
}
class VerifyParent {int number=23;int get(){return number;}}
class VerifyChild extends VerifyParent {}
class VerifyCanvas extends Canvas {
    protected void paint(Graphics g){g.drawString("SEMANTICS PASS",0,0,20);}
}
