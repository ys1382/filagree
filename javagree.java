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

    /**
     *
     * Test
     *
     * to test javagree functionality
     *
     */
    static class Test {

        public int x = 7;

        public Integer y(Integer z) {
            System.out.println("in y");
            return new Integer(x + z);
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
        Test test = new Test();
        Object result = j.eval(test, "tc", "sys.print(tc.x + tc.y(9))");
    }
}
