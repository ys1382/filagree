import java.lang.reflect.Method;

/**
 *
 * Java - filagree bridge
 *
 */
public class Javagree {

    static { System.loadLibrary("javagree"); }

    /**
     *
     * Evaluates string in filagree
     *
     * @param callback object
     * @param name of callback object
     * @param program program
     *
     */
    public native long eval(Object callback, String name, String program);
    
    class TestCallback {
        public int x = 1;
        int y(int z) {
            System.out.println("z = " + z);
            return x;
        }
    }
    
    /**
     *
     * main
     *
     * to test javagree functionality
     *
     */
    public static void main(String[] args) {
        Javagree j = new Javagree();
        TestCallback testCallback = j.new TestCallback();
        Object result = j.eval(testCallback, "tc", "sys.print(tc.x + tc.y())");
    }
}
