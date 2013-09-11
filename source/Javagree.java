import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

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
    public native int eval(Object callback, String name, String program, Object sys);
    
    /**
     *
     * Returns multiple values to filagree
     *
     * @param val first of zero or more values
     *
     */
    public static native void return_multiple(Object... val);
    
    /**
     *
     * Test
     *
     * to test javagree functionality
     *
     */
    static class Test {

        public int x = 6;
        final public static String test_ui = "import '../test/ui' " +
                                             "ui = sys.ui( nil, ['button', 'text':'clickez ici'] )";

        public Integer y(Object list, Object map) {

            System.out.println("list:");
            Object[] list2 = (Object[])list;
            for (int i=0; i<list2.length; i++)
                System.out.println("\t" + list2[i]);

            System.out.println("map:");
            HashMap map2 = (HashMap)map;
            Iterator iterator = map2.keySet().iterator();
            while (iterator.hasNext()) {
                String key = iterator.next().toString();
                String value = map2.get(key).toString();
                System.out.println("\t" + key + " " + value);
            }

            return list2.length;
        }
    }

    /**
     *
     * Sys
     *
     * filagree -> OS bridge
     *
     */
    static class Sys {

        public Sys() {}
        public Integer[] window(int w, int h) {
            System.out.println("inside screen");
            return new Integer[]{240,320};
        }

        public static void button(Object uictx,
                                  int x, int y, int w, int h,
                                  String logic, String text, String image) {
            Integer x2 = (Integer)x;
            System.out.println("button " + x +","+ y +","+ w +","+ h +","+ logic +","+ text +","+ image);
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
        Sys sys = new Sys();
        Javagree j = new Javagree();
        Test test = new Test();
        //Object result = j.eval(test, "tc", "sys.print('fg gets ' + tc.x + tc.y([7,8,9], ['p':99]))");
        j.eval(test, "tc", test.test_ui, sys);
    }
}
