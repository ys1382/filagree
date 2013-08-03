import java.lang.reflect.Method;

/**
 * Java - filagree bridge
 */
public class javagree {

    /**
     * Callback implementation
     * @param arguments arguments
     */
    public Object callback(Object... arguments) {
        System.out.println("Java - callback:");
        for (Object o : arguments)
            System.out.println("\t" + o);
        return null;
    }

    /**
     * Callback interface
     */
    public interface JavagreeCallback {
        /**
         * callback function
         *
         * @param arguments arguments
         */
        public Object callback(Object...arguments);
    }

    /**
     * Evaluates string in filagree
     *
     * @param c implements JavagreeCallback
     * @param o arguments
     */
    public native long filagreeEval(Object c,
                                Object... o);

    /**
     * main
     */
    public static void main(String[] args) {

        System.loadLibrary("javagree");
        javagree j = new javagree();
        Object result = j.filagreeEval(j, "sys.print('hi')");
        System.out.println("in Java, the result is " + String.valueOf(result));
    }
}
