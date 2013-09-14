package com.java.javagree;

//import java.lang.reflect.Method;
//import java.util.ArrayList;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Iterator;

import android.content.Context;
import android.util.Log;

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
    * Sys
    *
    * filagree -> OS bridge. Should be extended to implement Android-specific HAL functions.
    * 
    */
   static class Sys {

       private Context context;

       public Sys(Context context) { this.context = context; }

       public String read(String name) {
    	   
    	   Log.d("Javagree", "read " + name);

           InputStream input;
           
           try {

        	   input = this.context.getAssets().open(name);

               int size = input.available();
               byte[] buffer = new byte[size];
               input.read(buffer);
               input.close();

              return new String(buffer);

           } catch (IOException e) {
               e.printStackTrace();
               return "";
           }
       }

       public Integer[] window(int w, int h) {
           return new Integer[]{600,800};
       }

       public Object[] button(Object uictx,
                                 Object x, Object y, Object w, Object h,
                                 String logic, String text, String image) {
           Integer x2 = (Integer)x;
           Log.d("filagree", "button " + x2 +","+ y +","+ w +","+ h +", logic="+ logic +", text="+ text +", image="+ image);
           return new Object[]{null,22,33};
       }
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

       final public static String test_ui = " \n ui = sys.ui( nil, ['button', 'text':'clickez ici'] )";

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
   
   public static void doTesting(Context context) 
   {
	   Sys sys = new Sys(context);
       Javagree a = new Javagree();
       Test test = new Test();
       a.eval(test, "tc", "sys.print('fg gets ' + tc.x + tc.z([7,8,9], ['p':99]))", null);

       String imports = sys.read("ui.fg");
       a.eval(test, "tc", imports + Test.test_ui, sys);
	}
}
