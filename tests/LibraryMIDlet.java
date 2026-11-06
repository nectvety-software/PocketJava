import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;
import java.io.*;
import java.util.*;

public class LibraryMIDlet extends MIDlet implements Runnable {
    static boolean ready;
    static Thread worker;
    static LibraryCanvas screen;
    static void check(boolean value) { if (!value) throw new RuntimeException("Library assertion failed"); }
    static float floatReturn(float value){return value*2.0f;}
    static double doubleReturn(double value){return value+0.5;}
    protected void startApp() { screen = new LibraryCanvas(); check(!screen.isShown()); Display.getDisplay(this).setCurrent(screen); check(screen.isShown()); worker = new Thread(this); check(!worker.isAlive()); worker.start(); }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
    public void run() {
        try {
            check(worker != null && worker.isAlive());
            InputStream pngStream=getClass().getResourceAsStream("/stream.png");Image streamedImage=Image.createImage(pngStream);check(streamedImage.getWidth()==2 && streamedImage.getHeight()==1);check(pngStream.read()==-1);pngStream.close();
            Image tallFont=Image.createImage("/tall-font.png");check(tallFont.getWidth()==11 && tallFont.getHeight()==1885);
            check(!pngStream.markSupported());pngStream.mark(4);boolean resetFailed=false;try{pngStream.reset();}catch(IOException expected){resetFailed=true;}check(resetFailed);
            String identity=new String("identity");check(identity.toString()==identity);
            check(new RuntimeException("message").getMessage().equals("message") && new RuntimeException().getMessage()==null);
            StringBuffer chars=new StringBuffer("a\u00e9z");check(chars.charAt(1)=='\u00e9');boolean badChar=false;try{chars.charAt(3);}catch(StringIndexOutOfBoundsException expected){badChar=true;}check(badChar);
            check(floatReturn(3.25f)==6.5f && (int)(doubleReturn(4.0)*2.0)==9);
            check(LibraryWide.floatSum(1.0f)==36585.0f && (int)LibraryWide.doubleSum(1.0)==9453 && LibraryWide.longSum(1L)==9453L);
            StringBuffer edited=new StringBuffer("a\u00e9\u1ec7z");check(edited.delete(1,3)==edited && edited.toString().equals("az"));edited.delete(1,99);check(edited.toString().equals("a"));edited.deleteCharAt(0);check(edited.length()==0);
            boolean badDelete=false;try{edited.deleteCharAt(0);}catch(StringIndexOutOfBoundsException expected){badDelete=true;}check(badDelete);
            edited.append("\u00e9");edited.setLength(3);check(edited.length()==3 && edited.charAt(0)=='\u00e9' && edited.charAt(1)==0 && edited.charAt(2)==0);edited.setLength(1);check(edited.toString().equals("\u00e9"));edited.setLength(0);check(edited.length()==0);
            check(String.valueOf('\u1ec7').equals("\u1ec7") && String.valueOf((char)0).length()==1 && String.valueOf((char)0).charAt(0)==0);
            check(new StringBuffer().append(Long.MIN_VALUE).append('/').append(Long.MAX_VALUE).toString().equals("-9223372036854775808/9223372036854775807"));
            Vector grown=new Vector(1,2);check(grown.capacity()==1);grown.addElement("a");grown.addElement("b");check(grown.capacity()==3 && grown.size()==2);grown.addElement("c");grown.addElement("d");check(grown.capacity()==5 && grown.elementAt(1).equals("b"));Vector doubled=new Vector(0,-1);doubled.addElement("a");doubled.addElement("b");doubled.addElement("c");check(doubled.capacity()==4);boolean badCapacity=false;try{new Vector(-1,2);}catch(IllegalArgumentException expected){badCapacity=true;}check(badCapacity);
            ByteArrayInputStream marked=new ByteArrayInputStream(new byte[]{10,20,30,40},1,99);check(marked.markSupported() && marked.read()==20);marked.reset();check(marked.read()==20);marked.mark(1);check(marked.read()==30);marked.close();marked.reset();check(marked.read()==30);
            DataInputStream wrappedMark=new DataInputStream(marked);check(wrappedMark.markSupported());wrappedMark.mark(1);check(wrappedMark.read()==40);wrappedMark.reset();check(wrappedMark.read()==40);
            Graphics fontGraphics=Image.createImage(4,4).getGraphics();Font selectedFont=Font.getFont(32,1,8);fontGraphics.setFont(selectedFont);check(fontGraphics.getFont()==selectedFont);fontGraphics.setFont(null);check(fontGraphics.getFont().getStyle()==0);
            check(Long.parseLong("-9223372036854775808")==Long.MIN_VALUE && Long.parseLong("7fffffffffffffff",16)==Long.MAX_VALUE);
            check(Long.toString(Long.MIN_VALUE).equals("-9223372036854775808") && new Long(4294967297L).longValue()==4294967297L);
            check(new Long(-3L).equals(new Long(-3L)) && !new Long(1L).equals(new Long(4294967297L)));
            boolean longOverflow=false;try{Long.parseLong("9223372036854775808");}catch(NumberFormatException expected){longOverflow=true;}check(longOverflow);
            float f=3.25f;float[] fs=new float[]{f,2.0f};fs[1]=f*2.0f;
            check(fs[1]==6.5f && (int)(f/2.0f)==1 && (float)(double)f==f && -f==-3.25f);
            float nan=fs[0]-f;nan=nan/nan;check(nan!=nan && (int)nan==0 && (long)nan==0L);
            check(Short.parseShort("-32768")==-32768 && Short.parseShort("7fff",16)==32767);
            boolean shortOverflow=false;try{Short.parseShort("32768");}catch(NumberFormatException expected){shortOverflow=true;}check(shortOverflow);
            Random resetRandom=new Random(42L);int resetValue=resetRandom.nextInt();resetRandom.setSeed(42L);check(resetRandom.nextInt()==resetValue);
            ByteArrayOutputStream longsOutput=new ByteArrayOutputStream();DataOutputStream longWriter=new DataOutputStream(longsOutput);longWriter.writeLong(Long.MIN_VALUE);longWriter.writeLong(4294967297L);
            DataInputStream longReader=new DataInputStream(new ByteArrayInputStream(longsOutput.toByteArray()));check(longReader.readLong()==Long.MIN_VALUE && longReader.readLong()==4294967297L);
            check(!screen.hasPointerEvents() && screen.hasRepeatEvents() && screen.getGameAction(screen.getKeyCode(1))==1 && screen.getGameAction(screen.getKeyCode(12))==12);
            check(Thread.currentThread() == worker);
            worker.setPriority(8);check(worker.getPriority()==8);
            boolean badPriority=false;try{worker.setPriority(11);}catch(IllegalArgumentException expected){badPriority=true;}check(badPriority);
            Runtime runtime=Runtime.getRuntime();System.gc();check(Runtime.getRuntime()==runtime);
            check(runtime.totalMemory()>0 && runtime.freeMemory()>=0 && runtime.freeMemory()<=runtime.totalMemory());runtime.gc();
            check(System.getProperty("microedition.encoding").equals("UTF-8") && System.getProperty("unknown.property")==null);
            Font metadata=Font.getFont(32,3,8);check(metadata.getFace()==32 && metadata.getStyle()==3 && metadata.getSize()==8 && metadata.isBold() && metadata.isItalic() && !metadata.isPlain());
            check(Font.getDefaultFont().isPlain());
            byte[] encoded="A\u00e9\u1ec7".getBytes("ISO-8859-1");check(encoded.length==3 && encoded[1]==(byte)233 && encoded[2]==63);
            byte[] wideBytes="A\u1ec7".getBytes("UTF-16LE");check(wideBytes.length==4 && wideBytes[2]==(byte)199 && wideBytes[3]==30);
            boolean badEncoding=false;try{"text".getBytes("UNKNOWN-ENCODING");}catch(UnsupportedEncodingException expected){badEncoding=true;}check(badEncoding);
            ByteArrayOutputStream utfBuffer=new ByteArrayOutputStream();DataOutputStream utfOutput=new DataOutputStream(utfBuffer);
            utfOutput.writeUTF("A\u0000\u00e9\u1ec7");byte[] utfPayload=utfBuffer.toByteArray();check(utfPayload.length==10 && utfPayload[1]==8 && utfPayload[3]==(byte)192 && utfPayload[4]==(byte)128);
            DataInputStream utfInput=new DataInputStream(new ByteArrayInputStream(utfPayload));check(utfInput.readUTF().equals("A\u0000\u00e9\u1ec7"));
            DataInputStream staticUtf=new DataInputStream(new ByteArrayInputStream(utfPayload));check(DataInputStream.readUTF(staticUtf).equals("A\u0000\u00e9\u1ec7"));
            DataInputStream primitives=new DataInputStream(new ByteArrayInputStream(new byte[]{0x12,0x34,0x3f,(byte)0x80,0,0,0x3f,(byte)0xf0,0,0,0,0,0,0,'o','k','\r','\n','z'}));
            check(primitives.readChar()=='\u1234');check(primitives.readFloat()==1.0f);check(primitives.readDouble()==1.0);
            check(primitives.readLine().equals("ok") && primitives.readLine().equals("z") && primitives.readLine()==null);
            long x = 4294967297L;
            long[] longs = new long[]{4294967297L, -3L};
            check(longs[0] == 4294967297L && longs[1] == -3L);
            longs[1] = x + 9L;
            check(longs[1] == 4294967306L);
            check((x * 3L - 1L) / 2L == 6442450945L);
            check(Math.min(8589934593L, 4294967298L) == 4294967298L);
            check(Math.min(-4294967297L, 3L) == -4294967297L);
            check(Math.max(8589934593L, 4294967298L) == 8589934593L);
            check(Math.max(-4294967297L, 3L) == 3L);
            int sinScaled = (int)(Math.sin(0.5) * 1000.0); check(sinScaled >= 479 && sinScaled <= 480);
            check(new Integer(255).byteValue() == (byte)-1 && new Integer(0x1234).shortValue() == (short)0x1234);
            check(Integer.parseInt("7f", 16) == 127 && Integer.parseInt("-101", 2) == -5 && Integer.parseInt("+42", 10) == 42);
            check(Integer.parseInt("-80000000", 16) == Integer.MIN_VALUE && Integer.parseInt("2147483647") == Integer.MAX_VALUE);
            boolean badInteger = false; try { Integer.parseInt("2147483648", 10); } catch (NumberFormatException expected) { badInteger = true; }
            check(badInteger);
            javax.microedition.media.Manager.playTone(69, 250, 80);
            boolean badTone=false;try{javax.microedition.media.Manager.playTone(128, 250, 80);}catch(IllegalArgumentException expected){badTone=true;}check(badTone);
            check(Integer.toHexString(-1).equals("ffffffff") && Integer.toHexString(0).equals("0"));
            check(Integer.toOctalString(-1).equals("37777777777") && Integer.toBinaryString(Integer.MIN_VALUE).length() == 32);
            DataInputStream skipping = new DataInputStream(new ByteArrayInputStream(new byte[]{1,2,3,4,5}));
            check(skipping.skipBytes(-1) == 0 && skipping.skipBytes(2) == 2 && skipping.read() == 3);
            check(skipping.skipBytes(Integer.MAX_VALUE) == 2 && skipping.skipBytes(1) == 0);
            int[][] matrix = new int[2][3]; matrix[1][2] = 42;
            check(matrix[1][2] == 42 && matrix instanceof int[][]);
            String s = new String(new char[]{'A', '\u1ec7', 'Z'});
            check(s.length() == 3 && s.charAt(1) == '\u1ec7');
            byte[] utf8 = new byte[]{65, (byte)0xc3, (byte)0xa9, 90};
            String decoded = new String(utf8, 0, utf8.length, "UTF-8");
            check(decoded.length() == 3 && decoded.charAt(1) == '\u00e9');
            byte[] roundtripBytes = "A\u00e9Z".getBytes();
            String roundtrip = new String(roundtripBytes, "UTF-8");
            check(roundtrip.length() == 3 && roundtrip.charAt(0) == 'A' && roundtrip.charAt(1) == '\u00e9' && roundtrip.charAt(2) == 'Z');
            Image rgb = Image.createRGBImage(new int[]{0xffff0000, 0x0000ff00}, 2, 1, true);
            check(rgb.getWidth() == 2 && rgb.getHeight() == 1);
            boolean nullInvoke = false; try { ((Image)null).getWidth(); } catch (NullPointerException expected) { nullInvoke = true; } check(nullInvoke);
            Graphics offscreen = Image.createImage(2, 2).getGraphics();
            offscreen.drawRGB(new int[]{0xffff0000, 0xff00ff00, 0xff0000ff, 0x00ffffff}, 0, 2, 0, 0, 2, 2, true);
            check(s.substring(1, 2).charAt(0) == '\u1ec7');
            check(s.toCharArray()[1] == '\u1ec7');
            check(new StringBuffer().append('\u1ec7').toString().charAt(0) == '\u1ec7');
            ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            DataOutputStream output = new DataOutputStream(buffer);
            output.writeInt(0x12345678); output.writeShort(-1234);
            byte[] first = buffer.toByteArray(); first[0] = 0;
            byte[] second = buffer.toByteArray(); check(second[0] == 0x12);
            DataInputStream input = new DataInputStream(new ByteArrayInputStream(second));
            check(input.readInt() == 0x12345678 && input.readShort() == -1234);
            boolean eof = false; try { input.readInt(); } catch (EOFException expected) { eof = true; }
            check(eof); input.close();
            Vector vector = new Vector(); vector.addElement("one"); vector.addElement("two");
            Object[] refs = new Object[4]; refs[0] = "one"; refs[2] = vector; check(refs[0].equals("one") && refs[1] == null && refs[2] == vector);
            Object[] refsCopy = new Object[4]; System.arraycopy(refs, 0, refsCopy, 0, 4); check(refsCopy[0].equals("one") && refsCopy[1] == null && refsCopy[2] == vector);
            Object[] vectorCopy = new Object[2]; vector.copyInto(vectorCopy); check(vectorCopy[0].equals("one") && vectorCopy[1].equals("two"));
            System.gc(); Enumeration items = vector.elements();
            check(items.hasMoreElements() && items.nextElement().equals("one"));
            check(items.nextElement().equals("two") && !items.hasMoreElements());
            Hashtable table = new Hashtable(); Object key = vector.elementAt(0), value = vector.elementAt(1);
            check(table.put(key, value) == null && table.get(key) == value && table.containsKey(key) && table.size() == 1 && !table.isEmpty());
            Enumeration keys = table.keys(); Enumeration values = table.elements();
            check(keys.hasMoreElements() && keys.nextElement() == key && !keys.hasMoreElements());
            check(values.hasMoreElements() && values.nextElement() == value && !values.hasMoreElements());
            check(table.remove(key) == value && table.get(key) == null && !table.containsKey(key) && table.remove(key) == null);
            table.put(key, value); table.clear(); check(table.size() == 0 && table.isEmpty() && table.get(key) == null && !table.containsKey(key));
            check("abc".compareTo("abd") < 0 && "abd".compareTo("abc") > 0 && "abc".compareTo("abc") == 0);
            check("abc".compareTo("ab") == 1 && "A\u1ec7".compareTo("A\u00ea") > 0);
            check("x\u1ec7".toUpperCase().equals("X\u1ec6") && "X\u1ec6".toLowerCase().equals("x\u1ec7"));
            check("ab".concat("\u00e9").equals("ab\u00e9") && "ab".concat("").equals("ab"));
            check(String.valueOf((Object)"abc").equals("abc") && String.valueOf((Object)null).equals("null"));
            check(String.valueOf(12345).equals("12345") && String.valueOf(-7).equals("-7"));
            check("A\u00e9A".replace('A', '\u1ec7').equals("\u1ec7\u00e9\u1ec7") && "abc".replace('x', 'y').equals("abc"));
            String[] dupWords = new String[]{"x"}; dupWords[0] = new StringBuffer(dupWords[0]).append("y").toString(); check(dupWords[0].equals("xy"));
            check("A\u1ec7A".indexOf("A", 0) == 0 && "A\u1ec7^Z".indexOf("^") == 2 && "A\u1ec7Z".indexOf('Z') == 2);
            check("A\u1ec7A".lastIndexOf('A') == 2 && "A\u1ec7A".lastIndexOf('\u1ec7') == 1);
            check("abcabc".lastIndexOf('a', 4) == 3 && "abc".lastIndexOf('z') == -1);
            char[] copied = new char[]{'x','x','x','x'};
            "A\u1ec7Z".getChars(1, 3, copied, 1);
            check(copied[0] == 'x' && copied[1] == '\u1ec7' && copied[2] == 'Z' && copied[3] == 'x');
            ByteArrayInputStream availability = new ByteArrayInputStream(new byte[]{1,2,3});
            check(availability.available() == 3 && availability.read() == 1 && availability.available() == 2);
            DataInputStream availabilityData = new DataInputStream(availability);
            check(availabilityData.available() == 2);
            InputStream resource = getClass().getResourceAsStream("/eof.bin");
            check(resource != null && resource.available() == 5);
            check(resource.read() == 10 && resource.read() == 20 && resource.read() == 30 && resource.read() == 40 && resource.read() == 50);
            check(resource.available() == 0 && resource.read() == -1 && resource.read() == -1);
            resource.close();
            System.out.println("SYSTEM OUT PASS");
            Random seededA = new Random(0x123456789abcdefL), seededB = new Random(0x123456789abcdefL);
            check(seededA.nextInt() == seededB.nextInt() && seededA.nextInt(37) == seededB.nextInt(37));
            Thread.sleep(50L); ready = true;
        } catch (Exception failure) { throw new RuntimeException("Unexpected IO failure"); }
    }
}
class LibraryCanvas extends Canvas {
    protected void paint(Graphics graphics) { if (LibraryMIDlet.ready) { LibraryMIDlet.check(!LibraryMIDlet.worker.isAlive()); graphics.drawString("LIBRARY PASS", 4, 4, Graphics.TOP | Graphics.LEFT); } }
}

