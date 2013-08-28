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
     * main
     *
     * to test javagree functionality
     *
     */
    public static void main(String[] args) {
        Javagree j = new Javagree();
        Test test = new Test();
        Object result = j.eval(test, "tc", "sys.print('fg gets ' + tc.x + tc.y([7,8,9], ['p':99]))");
    }
}
