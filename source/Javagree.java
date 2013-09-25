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

	private Object callback;
	private String name;

    static { System.loadLibrary("javagree"); }

	Javagree(Object callback, String name) {
		this.callback = callback;
		this.name = name;
	}
    
	/**
	 *
	 * Evaluates string in filagree
	 *
	 * @param callback object
	 * @param name of callback object
	 * @param program program
	 * @param sys implements HAL platform API
	 *
	 */
	private native int evalSource(Object callback, String name, String sourceCode, Object sys);
	private native int evalBytes(Object callback, String name, byte[] byteCode, Object sys);
    
	int eval(String source) {
		return this.evalSource(this.callback, this.name, source, this);
	}
    
	int eval(byte[] bytes) {
		return this.evalBytes(this.callback, this.name, bytes, this);
	}

    public Integer[] window(int w, int h) {
        return new Integer[]{600,800};
    }
    
    public Object[] button(Object uictx,
                           Object x, Object y, Object w, Object h,
                           String logic, String text, String image) {
        Integer x2 = (Integer)x;
        System.out.println("button " + x2 +","+ y +","+ w +","+ h
                           +", logic="+ logic +", text="+ text +", image="+ image);
        return new Object[]{null,22,33};
    }
    
    
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

        public Integer y(Object a, Object b) {
            System.out.println("y: " + a +","+ b);
            return 99;
        }

        public Integer z(Object list, Object map) {

            System.out.println("list:");
            Object[] list2 = (Object[])list;
            for (int i=0; i<list2.length; i++)
                System.out.println("\t" + list2[i]);

            System.out.println("map:");
            HashMap map2 = (HashMap)map;
            Iterator iterator = map2.keySet().iterator();
            while (iterator.hasNext()) {
                Object key = iterator.next();
                Object value = map2.get(key);
                System.out.println("\t" + key + " " + value);
            }

            return list2.length;
        }
    } // Test

    /**
     *
     * main
     *
     * to test javagree functionality
     *
     */
    public static void main(String[] args) {
        Test test = new Test();
        Javagree j = new Javagree(test, "tc");
        Object result = j.eval("sys.print('fg gets ' + tc.x + tc.z([7,8,9], ['p':99]))");
        j.eval(test.test_ui);
    }
}
