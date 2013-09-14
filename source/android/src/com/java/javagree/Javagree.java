package com.java.javagree;

import java.util.HashMap;
import java.util.Iterator;

/**
 *
 * Java - filagree bridge
 *
 */
public class Javagree {

    static {
    	System.loadLibrary("javagree"); 
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
   public native int eval(Object callback, String name, String program, Object sys);


   /**
     *
     * Test
     *
     * to test javagree functionality
     *
     */
   static class Test {

       public int x = 6;

       final public static String test_ui = " \n ui = sys.ui( nil, ['label', 'text':'clickez ici'] )";

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
           HashMap<?,?> map2 = (HashMap<?,?>)map;
           Iterator<?> iterator = map2.keySet().iterator();
           while (iterator.hasNext()) {
               Object key = iterator.next();
               Object value = map2.get(key);
               System.out.println("\t" + key + " " + value);
           }

           return list2.length;
       }
   }
   
   public static void doTesting(MainActivity activity) 
   {
	   Sys sys = new Sys(activity);
       Javagree a = new Javagree();
       Test test = new Test();
       a.eval(test, "tc", "sys.print('fg gets ' + tc.x + tc.z([7,8,9], ['p':99]))", null);

       String imports = sys.read("ui.fg");
       a.eval(test, "tc", imports + Test.test_ui, sys);
	}
}
