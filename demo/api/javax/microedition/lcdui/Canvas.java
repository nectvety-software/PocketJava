package javax.microedition.lcdui;
public abstract class Canvas extends Displayable {
    protected Canvas(){}
    protected abstract void paint(Graphics g);
    protected void keyPressed(int key){}
    protected void keyReleased(int key){}
    public final void repaint(){}
    public final void setFullScreenMode(boolean full){}
    public int getGameAction(int key){return 0;}
    public int getKeyCode(int action){return 0;}
    public boolean hasPointerEvents(){return false;}
    public boolean hasPointerMotionEvents(){return false;}
    public boolean hasRepeatEvents(){return false;}
}
